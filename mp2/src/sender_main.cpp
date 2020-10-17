/*
 * File:   sender_main.c
 * Author:
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <netdb.h>
#include <math.h>

#include <iostream>
#include <deque>

#include "sender.h"

#define SENDER_BUF_SIZE 4096
#define RECV_BUF_SIZE 4096
#define CONTENT_SIZE 4088
#define MSS 1
#define SOCKET_TIMEOUT_MILLISEC 25
#define SOCKET_TIMEOUT_MICROSEC SOCKET_TIMEOUT_MILLISEC * 1000

using namespace std;

class State;
class SlowStart;
class CongAvoid;
class FastRecovery;
class ReliableSender;

enum SenderAction { sendNew, resend, waitACK };

void timeoutBase(ReliableSender *sender);

/*
 * Packet structure: packet id + content size +    content
 *                    4 bytes       4 bytes       4088 bytes
 *
 * Total size of a packet: 4096 bytes
 * The size of a packet is fixed, even though the content is less than 4088 bytes
 *
 * Receiver behavior:
 * 1. Whenever the receiver receives a packet, it send the packet id back to the sender.
 * 2. Write packets to file sequentially. If there is a packet lost, the after packets queue until
 *    the lost packet gets received.
 *
 * Sender behavior: just follows the tcp protocol
 */

struct sockaddr_in si_other;
int s, slen;

void diep(const char *s) {
    perror(s);
    exit(1);
}

class Packet {
    private:

    int id_, content_len_;
    char content_[CONTENT_SIZE];

    public:

    Packet(int id, int content_len, char* buf) {
        id_ = id;
        content_len_ = content_len;
        memcpy(content_, buf, CONTENT_SIZE);
    }

    int id() {
        return id_;
    }

    void fillData(char *buf) {
        memcpy(buf, &id_, 4);  // int, 4 bytes
        memcpy(buf+4, &content_len_, 4);  // int, 4 bytes
        memcpy(buf+8, content_, CONTENT_SIZE);  // 4088 bytes
    }
};

State::State() {}
State::State(ReliableSender *sender) {
    context_ = sender;
}
void State::timeout() {
    timeoutBase(context_);
}
State::~State() {};

class ReliableSender {
    private:
    int lastReceivedACKId_;

    FILE *fp_;
    unsigned long long remainingBytesToRead_;  // may not equal to file size
    bool isFileExhausted_;  // true if either the file is exhausted or
                            // remainingBytesToRead_ turns to 0 or negative
    State *state_;
    deque<Packet> sentWithoutAckPackets;
    char fileReadBuf_[CONTENT_SIZE];
    char sendBuf_[SENDER_BUF_SIZE];
    char recvBuf_[RECV_BUF_SIZE];
    int socket_;
    struct addrinfo *receiverinfo_;
    struct timeval timeoutVal_;

    deque<Packet> loadNewPacketsFromFile() {
        deque<Packet> new_deq;
        if (isFileExhausted_) return new_deq;

        int packetIdToAdd = sentWithoutAckPackets.size() == 0 ?
                leftPacketId_ : sentWithoutAckPackets.back().id() + 1;
        int bytesRead;
        int contentSize;
        int newPacketCnt = round(windowSize_) - sentWithoutAckPackets.size();
        while (newPacketCnt-- > 0) {
            bytesRead = fread(fileReadBuf_, 1, CONTENT_SIZE, fp_);
            contentSize = remainingBytesToRead_ >= bytesRead ?
                    bytesRead : remainingBytesToRead_;
            Packet packet(packetIdToAdd++, contentSize, fileReadBuf_);
            remainingBytesToRead_ -= bytesRead;
            new_deq.push_back(move(packet));
            if (bytesRead == 0 || remainingBytesToRead_ <= 0) {
                isFileExhausted_ = true;
                break;
            }
        }
        return new_deq;
    }

    static int getLargestACKId(char *buf, int bytesRead) {
        int largest_id = 0, packet_id, i = 0;
        while (i + 4 <= bytesRead) {
            memcpy(&packet_id, buf+i, 4);
            if (packet_id > largest_id) {
                largest_id = packet_id;
            }
            i += 4;
        }
        return largest_id;
    }

    public:
    float windowSize_;
    int ssthresh_;
    int leftPacketId_;  // the left side of the sliding window, should be the next ACK id
    int dupACKCnt_;
    SenderAction nextAction_;

    ReliableSender(FILE *fp, unsigned long long bytesToTransfer, int socket,
            struct addrinfo *receiverinfo) {
        windowSize_ = MSS;
        ssthresh_ = 64;
        leftPacketId_ = 0;
        lastReceivedACKId_ = -1;
        dupACKCnt_ = 0;
        nextAction_ = sendNew;
        remainingBytesToRead_ = bytesToTransfer;
        fp_ = fp;
        isFileExhausted_ = false;
        socket_ = socket;
        receiverinfo_ = receiverinfo;
        timeoutVal_.tv_sec = 0;
        timeoutVal_.tv_usec = SOCKET_TIMEOUT_MICROSEC;
        memset(recvBuf_, 0, RECV_BUF_SIZE);
    }

    void changeState(State *state) {
        if (state_ != nullptr) {
            delete state_;
        }
        state_ = state;
    }

    int sendSinglePacket(Packet *packet) {
        int sentBytes;
        packet->fillData(sendBuf_);
        sentBytes = sendto(socket_, sendBuf_, SENDER_BUF_SIZE, 0,
                receiverinfo_->ai_addr, receiverinfo_->ai_addrlen);
        if (sentBytes == -1) {
            perror("fail to send packet");
        }
        return sentBytes;
    }

    void sendNewPackets() {
        deque<Packet> newPackets = loadNewPacketsFromFile();
        for (auto it = newPackets.begin(); it != newPackets.end(); it++) {
            // send packet
            sendSinglePacket(&(*it));

            // push to sliding window
            sentWithoutAckPackets.push_back(*it);
        }
    }

    void resendOldPacket() {
        sendSinglePacket(&sentWithoutAckPackets[0]);
    }

    void setSocketRecvTimeout() {
        // set once or set everytime before calling recvfrom?
        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal_,
                       sizeof(timeoutVal_)) < 0) {
            perror("fail to set socket timeout");
        }
    }

    int getACKIdOrTimeout() {
        setSocketRecvTimeout();
        int recvBytes = recvfrom(socket_, recvBuf_, RECV_BUF_SIZE, 0, NULL, NULL);
        if (recvBytes < 0) {
            return -1;  // timeout
        } else if (recvBytes < 4) {
            perror("unable to decode ACK id");
            return lastReceivedACKId_;
        } else {
            return max(getLargestACKId(recvBuf_, recvBytes), lastReceivedACKId_);
        }
    }

    bool isFinished() {
        return sentWithoutAckPackets.size() == 0 && isFileExhausted_;
    }

    void removeACKedPacketsFromWindow(int ackId) {
        while (sentWithoutAckPackets.size() > 0 && sentWithoutAckPackets[0].id() <= ackId) {
            sentWithoutAckPackets.pop_front();
        }
    }

    void working() {
        while (!isFinished()) {
            switch(nextAction_) {
                case sendNew: sendNewPackets(); break;
                case resend: resendOldPacket(); break;
                case waitACK: break;
            }
            int ackId = getACKIdOrTimeout();
            if (ackId == -1) { // timeout
                state_->timeout();
            } else if (ackId == lastReceivedACKId_) {
                state_->dupACK();
            } else {  // new ACK
                lastReceivedACKId_ = ackId;
                removeACKedPacketsFromWindow(ackId);
                state_->newACK(ackId);
            }
        }
    }
};

class CongAvoid: public State {  // Congestion avoidance state
    public:
    CongAvoid(ReliableSender *context) : State(context){}

    void dupACK() {
        context_->dupACKCnt_++;
        context_->nextAction_ = waitACK;
        if (context_->dupACKCnt_ >= 3) {
            context_->ssthresh_ = round(context_->windowSize_ / 2) + 1;
            context_->windowSize_ = context_->ssthresh_ + 3;
            context_->nextAction_ = resend;
            FastRecovery *fastRecoveryState = new FastRecovery(context_);
            context_->changeState((State *) fastRecoveryState);
        }
    }

    void newACK(int ackId) {
        context_->dupACKCnt_ = 0;
        int step = ackId - context_->leftPacketId_ + 1;
        while (step-- > 0) {
            context_->windowSize_ =
                    context_->windowSize_ + MSS * (MSS / context_->windowSize_);
        }
        context_->nextAction_ = sendNew;
        context_->leftPacketId_ = ackId;
    }
};

FastRecovery::FastRecovery(ReliableSender *context) : State(context){}
void FastRecovery::dupACK() {
    context_->windowSize_ += MSS;
    context_->nextAction_ = sendNew;
}
void FastRecovery::newACK(int ackId) {
    context_->dupACKCnt_ = 0;
    context_->windowSize_ = context_->ssthresh_;
    context_->nextAction_ = sendNew;
    CongAvoid *congAvoidState = new CongAvoid(context_);
    context_->changeState((State *) congAvoidState);
    context_->leftPacketId_ = ackId;
}

class SlowStart: public State {  // Slow start state
    public:

    SlowStart(ReliableSender *context) : State(context){}

    void dupACK() {
        context_->dupACKCnt_++;
        context_->nextAction_ = waitACK;
    }

    void newACK(int ackId) {
        context_->dupACKCnt_ = 0;
        int step = ackId - context_->leftPacketId_ + 1;
        context_->windowSize_ += MSS * step;
        context_->nextAction_ = sendNew;
        context_->leftPacketId_ = ackId;
        if (context_->windowSize_ >= context_->ssthresh_) {
            CongAvoid *congAvoidState = new CongAvoid(context_);
            context_->changeState((State *) congAvoidState);
        }
    }
};

void timeoutBase(ReliableSender *context) {
    SlowStart *slowStartState = new SlowStart(context);
    context->changeState((State *) slowStartState);
    context->ssthresh_ = round(context->windowSize_ / 2) + 1;
    context->windowSize_ = MSS;
    context->dupACKCnt_ = 0;
    context->nextAction_ = resend;
}

void reliablyTransfer(char* hostname,
                      char* hostUDPport,
                      char* filename,
                      unsigned long long int bytesToTransfer) {
    // assume bytesToTransfer is equal the length of the target file
    // the above statement could be wrong
    struct addrinfo hints, *servinfo;
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */

    slen = sizeof (si_other);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(hostname, hostUDPport, &hints, &servinfo) != 0) {
        fprintf(stderr, "failed to getaddrinfo\n");
        return;
    }

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep(string("socket").c_str());

    ReliableSender sender(fp, bytesToTransfer, s, servinfo);
    SlowStart *initialState = new SlowStart(&sender);
    sender.changeState((State *) initialState);
    sender.working();

    freeaddrinfo(servinfo);
    printf("Closing the socket\n");
    fclose(fp);
    close(s);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], argv[2], argv[3], numBytes);

    return (EXIT_SUCCESS);
}
