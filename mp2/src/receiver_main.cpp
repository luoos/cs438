/*
 * File:   receiver_main.c
 * Author: Carl Guo
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

#include <unordered_map>

#define RECV_BUF_SIZE 4096
#define TCP_PACKET_SIZE 4096
#define BEGIN_SEQ_NUM 0

struct sockaddr_in si_me, si_other;
int s, slen;

typedef struct {
    unsigned int seq_no;
    unsigned int data_size;
    char data[4088];
} TCP_packet;

void diep(const char *s) {
    perror(s);
    exit(1);
}

unsigned int writeToFile(unsigned data_size, char data[], FILE* dest_file) {
    size_t bytes_written = 0;
    bytes_written = fwrite(data, 1, data_size, dest_file);
    return bytes_written;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    int recv_bytes;
    char buf[RECV_BUF_SIZE];
    char ack_buf[100];
    std::unordered_map<unsigned int, TCP_packet*> buffer;

    struct sockaddr_storage other_addr;
    socklen_t other_addr_len;

    slen = sizeof (si_other);
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        diep("socket");
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1) {
        diep("bind");
    }

    FILE* dest_file;
    if ((dest_file = fopen(destinationFile, "wb")) == NULL) {
        diep("fopen");
    }

    other_addr_len = sizeof other_addr;
    TCP_packet* incoming_packet;
    bool last_packet_found = false;
    unsigned int last_packet_seq_no = 0;
    // index of last consecutive packet in ring buffer
    int nextPacketId = 0;
    int send_back_ack_seq_no = nextPacketId - 1;
    while (true) {
        // receive data
        if ((recv_bytes = recvfrom(s, buf, RECV_BUF_SIZE, 0,
                (struct sockaddr*) &other_addr, &other_addr_len)) == -1) {
            perror("recv error");
            exit(1);
        }
        // decode and store data
        incoming_packet = (TCP_packet*) malloc(TCP_PACKET_SIZE);
        memcpy(incoming_packet, buf, TCP_PACKET_SIZE);

        if (incoming_packet->data_size == 0) {
            last_packet_found = true;
            last_packet_seq_no = incoming_packet->seq_no;
        }

        // write data to file
        if (incoming_packet->seq_no == nextPacketId) {
            // write to file
            int bytes_written = writeToFile(incoming_packet->data_size,
                                            incoming_packet->data, dest_file);
            // printf("%d bytes written to file\n", bytes_written);
            free(incoming_packet);
            incoming_packet = NULL;
            nextPacketId++;
            // check cached packet in map and write them to file
            while (buffer.find(nextPacketId) != buffer.end()) {
                TCP_packet* packet = buffer.at(nextPacketId);
                bytes_written = writeToFile(packet->data_size, packet->data, dest_file);
                // printf("%d bytes written to file\n", bytes_written);
                buffer.erase(nextPacketId);
                free(packet);
                nextPacketId++;
            }
            send_back_ack_seq_no = nextPacketId - 1;
        } else if (buffer.find(incoming_packet->seq_no) == buffer.end() &&
                    incoming_packet->seq_no > nextPacketId) {
            // cache this packet
            buffer.insert({incoming_packet->seq_no, incoming_packet});
        } else {
            free(incoming_packet);
            incoming_packet = NULL;
        }

        // send ack
        memcpy(ack_buf, &send_back_ack_seq_no, sizeof send_back_ack_seq_no);
        if (sendto(s, ack_buf, sizeof send_back_ack_seq_no, 0,
                (struct sockaddr *)&other_addr, other_addr_len) == -1) {
            perror("fail to send");
            exit(1);
        }
        // printf("ACK%d sent\n", send_back_ack_seq_no);

        // break from loop if last packet has been ACK'd
        if (last_packet_found && last_packet_seq_no == send_back_ack_seq_no) {
            break;
        }
    }

    fclose(dest_file);
    close(s);
    printf("%s received\n", destinationFile);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

