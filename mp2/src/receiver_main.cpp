/*
 * File:   receiver_main.c
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

#define RECV_BUF_SIZE 8192
#define TCP_PACKET_SIZE 4096
#define RING_BUF_SIZE 20
#define BEGIN_SEQ_NUM 0

#define DEBUG 0

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

/**
 * Writes all consecutive packets beginning from LCP_ind in the ring buffer
 * into the dest_file location and erases ring buffer location after each write.
 * Returns ACK sequence number to send back
 */
unsigned int consecutiveWriteToFile(TCP_packet* ring_buf[], int LCP_ind, FILE* dest_file) {
    size_t bytes_written = 0;
    unsigned int send_back_ack_seq_no;
    while (ring_buf[LCP_ind] != NULL) {
        TCP_packet* packet = ring_buf[LCP_ind];
        send_back_ack_seq_no = packet->seq_no;
        bytes_written = fwrite(packet->data, 1, packet->data_size, dest_file);
        // memcpy(&ring_buf[LCP_ind], 0, TCP_PACKET_SIZE);
        ring_buf[LCP_ind] = NULL;
        LCP_ind++;
        printf("%ld bytes written\n", bytes_written);
    }
    return send_back_ack_seq_no;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    int recv_bytes;
    char buf[RECV_BUF_SIZE];
    char ack_buf[100];
    TCP_packet* ring_buf[RING_BUF_SIZE];

    struct sockaddr_storage other_addr;
    socklen_t other_addr_len;
    memset(ring_buf, 0, RING_BUF_SIZE);

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


    other_addr_len = sizeof other_addr;
    TCP_packet incoming_packet;
    FILE* dest_file;
    if ((dest_file = fopen(destinationFile, "wb")) == NULL) {
        diep("fopen");
    }

    bool last_packet_found = false;
    unsigned int last_packet_seq_no;
    // index of last consecutive packet in ring buffer
    int LCP_ind = 0;
    while (true) {
        // receive data
        if ((recv_bytes = recvfrom(s, buf, RECV_BUF_SIZE, 0,
                (struct sockaddr*) &other_addr, &other_addr_len)) == -1) {
            perror("recv error");
            exit(1);
        }
        // decode and store data
        memcpy(&incoming_packet, buf, TCP_PACKET_SIZE);
        #ifdef DEBUG
        printf("receive packet %d, content_size: %d\n",
                incoming_packet.seq_no, incoming_packet.data_size);
        #endif
        ring_buf[incoming_packet.seq_no % RING_BUF_SIZE] = &incoming_packet;

        // check for last packet
        if (incoming_packet.data_size == 0) {
            last_packet_found = true;
            last_packet_seq_no = incoming_packet.seq_no;
        }
        // write data to file
        unsigned int send_back_ack_seq_no;
        if (incoming_packet.seq_no == BEGIN_SEQ_NUM) { // write beginning packet, do not update LCP
            // write all consecutive in ring buffer to file and update sendBackAck
            send_back_ack_seq_no = consecutiveWriteToFile(ring_buf, LCP_ind, dest_file);
            // update LCP index
            LCP_ind = send_back_ack_seq_no % RING_BUF_SIZE;
        } else if (incoming_packet.seq_no != BEGIN_SEQ_NUM &&
                    incoming_packet.seq_no == LCP_ind + 1) { // write incoming packet if it is consecutive
            LCP_ind++;
            // write all consecutive in ring buffer to file and update sendBackAck
            send_back_ack_seq_no = consecutiveWriteToFile(ring_buf, LCP_ind, dest_file);
            // update LCP index
            LCP_ind = send_back_ack_seq_no % RING_BUF_SIZE;
        }

        // send ack
        memcpy(ack_buf, &send_back_ack_seq_no, sizeof send_back_ack_seq_no);
        if (sendto(s, ack_buf, sizeof send_back_ack_seq_no, 0,
                (struct sockaddr *)&other_addr, other_addr_len) == -1) {
            perror("fail to send");
            exit(1);
        }
        printf("ACK%d sent\n", send_back_ack_seq_no);

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

