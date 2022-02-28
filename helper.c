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

#define BUFLEN 2000
#define MAX_ID_LEN 10

int max(int a, int b) {
	if (a > b) {
		return a;
	}
	return b;
}

void remove_newline(char* buffer) {
	char* s = strchr(buffer, '\n');
	if (s != NULL) {
		s[0] = '\0';
	}
}

void read_buffer(char* buffer) {
	fgets(buffer, BUFLEN - 1, stdin);
	remove_newline(buffer);
}

/* 	
	this function is used when we want to free a cell in a linked list
 	but we don't want to free the information it points to;
 	this function is passed as parameter to a function which removes 
 	a cell from a list 
 */
void dont_free(void* info) {
	// do nothing
	return;
}

int disable_nagle(int sockfd) {
	int flag = 1;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
}