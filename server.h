#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "helper.c"
#include "List.c"

#pragma pack(1)

#define MAX_CLIENTS 1000
#define MAX_TOPIC_SIZE 50
#define MAX_PAYLOAD_SIZE 1500
#define MAX_IP_LEN 16

#define OFFLINE 0
#define ONLINE 1


// define packet types
#define WRONG 0
#define ACK 1
#define TOPIC_NEWS 2
#define KILL 3

// UDP

struct udp_msg {
	char topic[MAX_TOPIC_SIZE];
	uint8_t type;
	char payload[MAX_PAYLOAD_SIZE];
};

struct udp_header {
	char ip[MAX_IP_LEN];
	uint16_t port;
};


// message format
struct msg {
	uint8_t type;
	struct udp_header udp_hdr;
	struct udp_msg payload;
};

// Client and topics

struct client {
	char id[MAX_ID_LEN];
	int socket;
	uint8_t status;
	TList* msg_not_sent;
	TList* topics;
};

struct topic {
	char name[MAX_TOPIC_SIZE];
	TList* clients;
};

struct client_sf {
	struct client* client;
	uint8_t sf;
};

struct unsent_msg {
	struct msg message;
	int nr_clients;
};

// adds a client to the list of clients
void add_client(struct client* c, TList** client_list);

// adds a new topic to the list of topics
void add_topic(struct topic* t, TList** topic_list);

// subscribe: adds a client to the list of clients subscribed to topic
// adds pointer to topic to the list of topics the client is subscribed to
int subscribe(char* topic_name, uint8_t sf, struct client* c, TList* client_list, TList** topic_list);

int unsubscribe(char* topic_name, struct client* c, TList* client_list, TList* topic_list);