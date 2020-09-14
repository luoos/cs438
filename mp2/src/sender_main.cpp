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

#include <iostream>

#define SENDER_BUF_SIZE 4096
#define RECV_BUF_SIZE 4096

/*
 * Packet structure: packet id + content size +    content
 *                    4 bytes       4 bytes        up to 4088 bytes
 *
 * Total size of a packet: up to 4096 bytes
 *
 * Receiver behavior:
 * 1. Whenever the receiver receives a packet, it send the packet id back to the sender.
 * 2. Write packets to file sequentially. If there is a packet lost, the after packets queue until
 *    the lost packet gets received.
 *
 * Sender behavior:
 * 1. Set num_packet to a inital value, like 16
 * 2. For each round
 *      2.1 read file and construct num_packet packets
 *      2.2 add all built packets to a map, the key is the packet_id
 *      2.3 send all packets in the map
 *      2.4 go to waiting state with a timeout. During this time, the sender should receive
 *          some packet_ids from receiver. For each packet_id received, remove corresponding
 *          item from the map.
 *      2.5 exit the waiting state if the map is empty or timeout
 *      2.6 if the map is empty, double the num_packet, otherwise halve it
 *      2.7 go to the next round
 */

struct sockaddr_in si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}


void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    int sent_bytes, recv_bytes;
    char send_buf[SENDER_BUF_SIZE] = "hello";  // TODO: remove the initial value
    char recv_buf[RECV_BUF_SIZE];
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage other_addr;
    socklen_t other_addr_len = sizeof other_addr;
    memset(recv_buf, 0, RECV_BUF_SIZE);
    //Open the file
    // FILE *fp;
    // fp = fopen(filename, "rb");
    // if (fp == NULL) {
    //     printf("Could not open file to send.");
    //     exit(1);
    // }

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
        diep("socket");

    /* Send data */
    if ((sent_bytes = sendto(s, send_buf, 5, 0,  // just send a "hello" for now
            servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
        perror("fail to send");
        exit(1);
    }

    /* Receive ack */
    if ((recv_bytes = recvfrom(s, recv_buf, RECV_BUF_SIZE, 0,
            (struct sockaddr *)&other_addr, &other_addr_len)) == -1) {
        perror("recv error");
        exit(1);
    }

    printf("%s\n", recv_buf);

    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    std::cout << "output test" << std::endl;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], argv[2], argv[3], numBytes);

    return (EXIT_SUCCESS);
}
