#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "server.h"

#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

const char id_type[4][11] = {"INT\0", "SHORT_REAL\0", "FLOAT\0", "STRING\0"};

// sends a message containing the id to the server
// when connecting for the first time
int send_id(int sockfd, char* id) {
	int n = send(sockfd, id, MAX_ID_LEN, 0);
	return n;
}

void print_int(char* content) {
	uint8_t* sign = (uint8_t*) content;
	uint32_t* number = (uint32_t*) (content + sizeof(uint8_t));

	int n = ntohl(*number);
	if (*sign == 1) {
		n *= -1;
	}
	printf("%d\n", n);
}

void print_short_real(char* content) {
	uint16_t* n = (uint16_t*) content;
	float val = ((float) (ntohs(*n))) / 100;
	printf("%.2f\n", val);
}

void print_float(char* content) {
	uint8_t* sign = (uint8_t*) content;
	uint32_t* number = (uint32_t*) (content + sizeof(uint8_t));
	uint8_t* pw = (uint8_t*) (content + sizeof(uint8_t) + sizeof(uint32_t));

	float val = ((float) (ntohl(*number))) / pow(10, *pw);
	if (*sign == 1) {
		val *= -1;
	}

	printf("%.*f\n", *pw, val);
}

void print_string(char* content) {
	printf("%s\n", content);
}

void process_topic_news_msg(struct msg message) {
	char* topic = message.payload.topic;
	uint8_t type = message.payload.type;
	char* content = message.payload.payload;

	// print topics
	if (topic[MAX_TOPIC_SIZE] >= 'A' && topic[MAX_TOPIC_SIZE] <= 'z') {
		// topic has 50 characters so (printf("%s\n")) won't work
		printf("%.*s - ", MAX_TOPIC_SIZE, topic);
	} else {
		printf("%s - ", topic);
	}

	// print type
	printf("%s - ", id_type[type]);

	// print message content
	switch (type) {
		case INT:
			print_int(content);
			break;

		case SHORT_REAL:
			print_short_real(content);
			break;

		case FLOAT:
			print_float(content);
			break;

		case STRING:
			print_string(content);
			break;
	}
}

int process_recv_msg(struct msg message) {
	uint8_t type = message.type;

	switch(type) {
		case WRONG:
			return WRONG;
		case ACK:
			printf("%s", ((char*) &message) + sizeof(uint8_t));
			return ACK;
		case TOPIC_NEWS:
			process_topic_news_msg(message);
			return TOPIC_NEWS;
		case KILL:
			return KILL;
	}

	return -1;
}

int main(int argc, char* argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc != 4) {
		return 1;
	}

	char* id = argv[1];
	char* server_ip = argv[2];
	int port = atoi(argv[3]);

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	fd_set read_fds, tmp_fds;
	int fdmax;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		return 1;
	}

	int res = disable_nagle(sockfd);
	if (res < 0) {
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	inet_aton(server_ip, &serv_addr.sin_addr);

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if (ret < 0) {
		return 1;
	}

	// sending id to the server
	n = send_id(sockfd, id);
	if (n < 0) {
		close(sockfd);
		return 1;
	}

	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if (ret < 0) {
			return 1;
		}

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == 0) {
					// STDIN
					memset(buffer, 0, BUFLEN);
					read_buffer(buffer);

					// exit command
					if (!strcmp(buffer, "exit")) {
						close(sockfd);
						return 0;
					}

					// sub / unsub command
					n = send(sockfd, buffer, BUFLEN, 0);
					if (n < 0) {
						continue;
					}

					continue;
				}

				if (i == sockfd) {
					// read from socket
					struct msg message;
					n = recv(sockfd, &message, sizeof(message), 0);
					if (n < 0) {
						return 1;
					}
					res = process_recv_msg(message);
					if (res == KILL) {
						close(sockfd);
						return 0;
					}
				}
			}
		}
	}
	
	return 0;
}