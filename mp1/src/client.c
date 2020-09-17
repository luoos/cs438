/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define MAX_DATA_SIZE 4096 // max number of bytes we can get at once

#define MAX_DOMAIN_SIZE 100
#define MAX_PATH_SIZE 100
#define MAX_SENDLINE 256

#define OUTPUT_FILE_NAME "output"

#define DELIMITER "\r\n\r\n"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAX_DATA_SIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	int match_cnt;
	char domain[MAX_DOMAIN_SIZE], path[MAX_PATH_SIZE], port[10];
	char sendline[MAX_SENDLINE];
	FILE* fp;

	if (argc != 2) {
		fprintf(stderr,"usage: client http://hostname[:port]/path/to/file\n");
		fprintf(stderr, "Example:\n");
		fprintf(stderr, "./client http://illinois.edu/index.html\n");
		fprintf(stderr, "./client http://12.34.56.78:8888/somefile.txt\n");
		exit(1);
	}

	// try to match domain, port, and path
	match_cnt = sscanf(argv[1], "%*[^:]%*[:/]%[^:]:%[0-9]%s", domain, port, path);
	if (match_cnt != 3) {
		// try to match domain and path. Assume the port is 80
		match_cnt = sscanf(argv[1], "%*[^:]%*[:/]%[^/]%s", domain, path);
		if (match_cnt != 2) {
			fprintf(stderr, "invalid URL, please check URL: %s\n", argv[1]);
			exit(1);
		}
		strcpy(port, "80"); // use default port 80
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(domain, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	sprintf(sendline, "GET %s HTTP/1.0\r\n\r\n", path);

	if (write(sockfd, sendline, strlen(sendline)) == strlen(sendline)) {
		fp = fopen(OUTPUT_FILE_NAME, "w");
		if (!fp) {
			fprintf(stderr, "file unable to be open");
			close(sockfd);
			exit(1);
		}

		if ((numbytes = recv(sockfd, buf, sizeof buf, 0)) > 0) {
			// this points to the "\r\n\r\n..."
			char* header_tail = strstr(buf, DELIMITER);
			if (header_tail) {  // found the DELIMITER
				// skip the "\r\n\r\n"
				char* content_start = header_tail + strlen(DELIMITER);
				int header_len = content_start - buf;  // including the delimiter
				fwrite(content_start, 1, numbytes - header_len, fp);

				while ((numbytes = recv(sockfd, buf, sizeof buf, 0)) > 0) {
					fwrite(buf, 1, numbytes, fp);
				}
			}
		}
		fclose(fp);
		printf("wrote content to file: %s\n", OUTPUT_FILE_NAME);
	} else {
		printf("failed to send request\n");
	}

	close(sockfd);

	return 0;
}

