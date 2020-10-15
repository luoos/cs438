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

struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    int recv_bytes, sent_bytes;
    char buf[RECV_BUF_SIZE];
    char tmp_buf[100] = "hey there";  // TODO: use a real send buf
    struct sockaddr_storage other_addr;
    socklen_t other_addr_len;
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    /* Receive data */
    other_addr_len = sizeof other_addr;
    if ((recv_bytes = recvfrom(s, buf, RECV_BUF_SIZE, 0,
            (struct sockaddr *)&other_addr, &other_addr_len)) == -1) {
        perror("recv error");
        exit(1);
    }
    printf("%s\n", buf);

    /* Decode data */

    /* Send ack */
    if (sendto(s, tmp_buf, strlen(tmp_buf), 0,  // just a demo. should send packet id
            (struct sockaddr *)&other_addr, other_addr_len) == -1) {
        perror("fail to send");
        exit(1);
    }

    close(s);
    printf("%s received.", destinationFile);
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

