#include "server.h"

struct udp_header get_udp_header(struct sockaddr_in addr) {
	struct udp_header h;
	memset(&h, 0, sizeof(struct udp_header));
	h.port = addr.sin_port;
	strcpy(h.ip, inet_ntoa(addr.sin_addr));
	return h;
}

struct msg create_news_message(struct udp_msg m, struct sockaddr_in addr) {
	struct msg message;
	memset(&message, 0, sizeof(struct msg));
	message.type = TOPIC_NEWS;
	message.udp_hdr = get_udp_header(addr);
	message.payload = m;
	return message;
}

struct msg create_error_message() {
	struct msg message;
	memset(&message, 0, sizeof(struct msg));
	message.type = WRONG;
	return message;
}

struct msg create_ACK_message(char* buffer) {
	struct msg message;
	memset(&message, 0, sizeof(struct msg));
	message.type = ACK;
	char* m = (char *) &message;
	strcpy(m + sizeof(uint8_t), buffer);
	return message;
}

struct msg create_kill_message() {
	struct msg message;
	memset(&message, 0, sizeof(struct msg));
	message.type = KILL;
	return message;
}

struct topic* create_topic(char* name) {
	struct topic* t = malloc(sizeof(struct topic));
	if (!t) {
		return NULL;
	}

	strncpy(t->name, name, MAX_TOPIC_SIZE);
	t->clients = NULL;
	return t;
}

struct client* create_client(char* id, int sockfd) {
	struct client* c = malloc(sizeof(struct client));
	if (!c) {
		return NULL;
	}

	strcpy(c->id, id);
	c->socket = sockfd;
	c->status = ONLINE;
	c->topics = NULL;
	c->msg_not_sent = NULL;
	return c;
}

// checks if 2 clients have the same socket
int eq_sock_client(void* c1, void* c2) {
	if (*(int*) c1 == ((struct client_sf*)c2)->client->socket) {
		return 1;
	}
	return 0;
}

// compare 2 topics by name
int eq_topics(void* t1, void* t2) {
	char* n1 = ((struct topic*)t1)->name;
	char* n2 = ((struct topic*)t2)->name;
	if (!strncmp(n1, n2, MAX_TOPIC_SIZE)) {
		return 1;
	}
	return 0;
}

// compare 2 unsent messages
int eq_unsent_msgs(void* m1, void* m2) {
	if (memcmp((struct unsent_msg*) m1, (struct unsent_msg*) m2, sizeof(struct unsent_msg))) {
		return 1;
	}
	return 0;
}

void add_client(struct client* c, TList** client_list) {
	cons(c, client_list);
}

// searches for a client in a list by his socket
struct client* get_client_by_socket(int socket, TList* client_list) {
	while (client_list != NULL) {
		struct client* c = head(client_list);
		if (c->socket == socket) {
			return c;
		}
		client_list = client_list->next;
	}
	return NULL;
}

// searches for a client in a list by his ID
struct client* get_client_by_id(char* id, TList* client_list) {
	while (client_list != NULL) {
		struct client* c = head(client_list);
		if (!strcmp(c->id, id)) {
			return c;
		}
		client_list = client_list->next;
	}
	return NULL;
}

void add_topic(struct topic* t, TList** topic_list) {
	cons(t, topic_list);
}

// searches for a topic by it's name
struct topic* get_topic(char* topic_name, TList* topic_list) {
	while (topic_list != NULL) {
		struct topic* t = head(topic_list);
		if (!strncmp(t->name, topic_name, MAX_TOPIC_SIZE)) {
			return t;
		}
		topic_list = topic_list->next;
	}
	return NULL;
}

// returns pointer to the topic if the client is subscribed to that topic
struct topic* get_topic_from_client(char* topic_name, TList* topic_list) {
	while(topic_list != NULL) {
		struct topic* t = head(topic_list);
		if (!strncmp(topic_name, t->name, 50)) {
			return t;
		}
		topic_list = topic_list->next;
	}
	return NULL;
}

// checks if client c is subscribed to topic t
int is_subscribed(struct client* c, struct topic* t) {
	TList* clients = t->clients;
	while (clients != NULL) {
		struct client_sf* current_csf = head(clients);
		if (!strcmp(current_csf->client->id, c->id)) {
			return 1;
		}
		clients = clients->next;
	}
	return 0;
}

int subscribe(char* topic_name, uint8_t sf, struct client* c, TList* client_list, TList** topic_list) {
	if (!c) {
		return -2; // client does not exist
	}

	struct topic* t = get_topic(topic_name, *topic_list);
	if (!t) {
		t = create_topic(topic_name);
		cons(t, topic_list);
	}

	// check if the client is already subscribed
	if (is_subscribed(c, t)) {
		return -1;
	}

	// wrapper over client and sf
	struct client_sf* csf = malloc(sizeof(struct client_sf));
	csf->client = c;
	csf->sf = sf;

	cons(t, &(c->topics));
	cons(csf, &(t->clients));
	return 1;
}

int unsubscribe(char* topic_name, struct client* c, TList* client_list, TList* topic_list) {
	struct topic* t = get_topic_from_client(topic_name, c->topics);
	if (!c || !t) {
		return -1;
	}

	// remove client from topic list of clients
	remove_elem(&(t->clients), &c->socket, eq_sock_client, free);

	// remove topic from client's list
	remove_elem(&(c->topics), t, eq_topics, dont_free);
	return 1;
}

void send_msg(struct topic* t, struct udp_msg m, struct sockaddr_in client_addr, TList** to_be_sent) {
	struct msg message = create_news_message(m, client_addr);
	TList* clients = t->clients;

	struct unsent_msg* copy = calloc(1, sizeof(struct unsent_msg));
	if (!copy) {
		return;
	}

	memcpy(&copy->message, &message, sizeof(struct msg));

	while (clients != NULL) {
		struct client_sf* wrapper = head(clients);
		struct client* c = wrapper->client;
		if (c->status == ONLINE) {
			struct msg tmp = message;
			int n = send(c->socket, &tmp, sizeof(struct msg), 0);
			if (n < 0) {
				printf("Couldn't send message to client with id: %s\n", c->id);
			}
		} else {
			// CLIENT IS OFFLINE
			if (wrapper->sf == 1) {
				cons(copy, &c->msg_not_sent);
				copy->nr_clients++;
			}
		}
		clients = clients->next;
	}

	if (copy->nr_clients == 0) {
		// msg was sent to all subscribers
		// we do not need to store it
		free(copy);
	} else {
		// saving the message in a list of unsent messages
		cons(copy, to_be_sent);
	}
}

void send_unreceived_messages(struct client* c, TList** unreceived_msgs) {
	TList* unsent_list = c->msg_not_sent;
	c->msg_not_sent = NULL;
	while (unsent_list != NULL) {
		struct unsent_msg* m = head(unsent_list);
		struct msg tmp = m->message;
		int n = send(c->socket, &tmp, sizeof(struct msg), 0);
		if (n < 0) {
			printf("Could not send message\n");
			return;
		}
		struct unsent_msg temp;
		memcpy(&temp, m, sizeof(struct unsent_msg));
		m->nr_clients--;
		if (m->nr_clients == 0) {
			remove_elem(unreceived_msgs, &temp, eq_unsent_msgs, free);
		}
		TList* aux = unsent_list;
		unsent_list = unsent_list->next;
		aux->next = NULL;
		free(aux);
	}
}

/* when a client connects to the server for the first, he will
send a message to the server with his id
*/
int recv_client_id(int sockfd, char* id) {
	int n = recv(sockfd, id, MAX_ID_LEN, 0);
	return n;
}

void print_new_client_connected(struct client* c, struct sockaddr_in client_addr) {
	char* address = inet_ntoa(client_addr.sin_addr);
	uint16_t port = ntohs(client_addr.sin_port);

	printf("New client %s connected from %s:%hu.\n", c->id, address, port);
}

void print_client_disconnected(struct client* c) {
	printf("Client %s disconnected.\n", c->id);
}

// computes max file descriptor from socket_udp, socket_tcp, and all other sockets
int compute_fdmax(int socket_tcp, int socket_udp, TList* clients) {
	int fd_max = max(socket_udp, socket_tcp);
	while (clients != NULL) {
		struct client* c = (struct client*) clients->info;
		if (c->status == ONLINE) {
			fd_max = max(fd_max, c->socket);
		}
		clients = clients->next;
	}
	return fd_max;
}

// checks if sf is valid
int check_sf(char* sf) {
	if (sf == NULL) {
		return 0;
	}
	if (sf[0] == '0' || sf[0] == '1') {
		return 1;
	}
	return 0;
}

void process_tcp_client_msg(char* buffer, struct client* c, TList* client_list, TList** topic_list) {
	// get msg type
	char* msg_type = strtok(buffer, " ");

	// get msg topic
	char* topic = strtok(NULL, " ");

	struct msg reply;
	int n;
	memset(&reply, 0, sizeof(struct msg));
	if (!strcmp(msg_type, "subscribe")) {
		char* sf_string = strtok(NULL, " ");
		if (!check_sf(sf_string)) {
			// wrong sf
			reply = create_error_message();
			send(c->socket, &reply, sizeof(reply), 0);
			return;
		}

		uint8_t sf = atoi(sf_string);
		n = subscribe(topic, sf, c, client_list, topic_list);
		if (n == -1) {
			reply = create_ACK_message("Already subscribed.\n");
		} else {
			reply = create_ACK_message("Subscribed to topic.\n");
		}
		n = send(c->socket, &reply, sizeof(reply), 0);
		if (n < 0) {
			printf("Could no send subscribe ack.\n");
		}
		return;
	}

	if (!strcmp(msg_type, "unsubscribe")) {
		n = unsubscribe(topic, c, client_list, *topic_list);
		if (n == -1) {
			reply = create_ACK_message("Not subscribed");
		}
		reply = create_ACK_message("Unsubscribed from topic.\n");
		int n = send(c->socket, &reply, sizeof(reply), 0);
		if (n < 0) {
			printf("Could no send unsubscribe ack.\n");
		}
	}
}

// closes all sockets and sends message to all connected clients to disconnect
void close_everything(int fd_max, fd_set* read_fd, int sockfd_udp, int sockfd_tcp) {
	for (int i = 0; i < fd_max + 1; i++) {
		if (FD_ISSET(i, read_fd) && i != sockfd_tcp && i != sockfd_udp) {
			struct msg m = create_kill_message();
			send(i, &m, sizeof(struct msg), 0);
			close(i);
		}
	}
	FD_ZERO(read_fd);
}

// Functions to free the memory

void free_client(void* c) {
	struct client* c1 = c;
	destroy(&c1->msg_not_sent, dont_free);
	destroy(&c1->topics, dont_free);
	free(c1);

}

void free_topic(void* t) {
	struct topic* t1 = t;
	destroy(&t1->clients, free);
	free(t1);
}

void destroy_clients(TList** client_list) {
	destroy(client_list, free_client);
}

void destroy_topics(TList** topic_list) {
	destroy(topic_list, free_topic);

}

void destroy_unsent_msgs(TList** list) {
	destroy(list, free);
}

void free_everything(TList** client_list, TList** topic_list, TList** unsent_msgs) {
	destroy_clients(client_list);
	destroy_topics(topic_list);
	destroy_unsent_msgs(unsent_msgs);
}

int main(int argc, char* argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc != 2) {
		return 1;
	}

	char buffer[BUFLEN];

	int sockfd_tcp, sockfd_udp, port, res;
	struct sockaddr_in server_addr, client_addr;
	socklen_t len = sizeof(struct sockaddr_in);

	port = atoi(argv[1]);
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);

	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	res = disable_nagle(sockfd_tcp);
	if (res < 0) {
		return 1;
	}

	memset((char *) &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	fd_set read_fd, tmp_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&tmp_fd);

	int rez = bind(sockfd_udp, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
	if (rez < 0) {
		return 1;
	}

	rez = bind(sockfd_tcp, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
	if (rez < 0) {
		return 1;
	}

	listen(sockfd_tcp, MAX_CLIENTS);

	FD_SET(sockfd_udp, &read_fd);
	FD_SET(sockfd_tcp, &read_fd);
	FD_SET(0, &read_fd);
	int fd_max = max(sockfd_udp, sockfd_tcp);
	int n;

	TList* topics = NULL;
	TList* clients = NULL;
	TList* unsent_msg = NULL;

	while (1) {
		tmp_fd = read_fd; 
		
		rez = select(fd_max + 1, &tmp_fd, NULL, NULL, NULL);
		if (rez < 0) {
			return 1;
		}

		for (int i = 0; i < fd_max + 1; i++) {
			if (FD_ISSET(i, &tmp_fd)) {
				if (i == 0) {
					// check stdin command
					memset(buffer, 0, sizeof(buffer));
					read_buffer(buffer);

					if (!strcmp(buffer, "exit")) {
						close_everything(fd_max, &read_fd, sockfd_udp, sockfd_tcp);
						free_everything(&clients, &topics, &unsent_msg);
						return 0;
					}

					// wrong command
					continue;
				}

				// connection request from tcp client
				if (i == sockfd_tcp) {
					len = sizeof(client_addr);
					int newsockfd = accept(sockfd_tcp, (struct sockaddr *) &client_addr, &len);
					if (newsockfd < 0) {
						return 1;
					}
					res = disable_nagle(newsockfd);
					if (res < 0) {
						return 1;
					}

					char id[MAX_ID_LEN];
					n = recv_client_id(newsockfd, id);
					if (n < 0) {
						return 1;
					}

					struct client* c = get_client_by_id(id, clients);
					if (c == NULL) {
						// create client
						c = create_client(id, newsockfd);
						add_client(c, &clients);
					} else {
						// client exists
						if (c->status == ONLINE) {
							printf("Client %s already connected.\n", c->id);
							struct msg m = create_kill_message();
							n = send(newsockfd, &m, sizeof(struct msg), 0);
							continue;
						} else {
							// client who already exists comes back online
							c->socket = newsockfd;
							c->status = ONLINE;
							send_unreceived_messages(c, &unsent_msg);
						}
					}

					print_new_client_connected(c, client_addr);
					FD_SET(newsockfd, &read_fd);
					if (newsockfd > fd_max) {
						fd_max = newsockfd;
					}
					continue;
				}
				
				if (i == sockfd_udp) {
					// receive topic
					struct udp_msg m;
					memset(&m, 0, sizeof(struct udp_msg));
					n = recvfrom(i, &m, sizeof(struct udp_msg), 0, (struct sockaddr *) &client_addr, &len);
					if (n > 0) {
						// get topic
						struct topic* t = get_topic(m.topic, topics);
						if (!t) {
							t = create_topic(m.topic);
							add_topic(t, &topics);
						}

						// send msg to all clients subscribed to that topic
						send_msg(t, m, client_addr, &unsent_msg);
					}
					continue;
				}

				// else -> receive msg from tcp_client
				memset(buffer, 0, sizeof(buffer));
				n = recv(i, buffer, sizeof(buffer), 0);

				// Error recv
				if (n < 0) {
					return 1;
				}

				struct client* c = get_client_by_socket(i, clients);
				if (!c) {
					continue;
				}

				// connection closed
				if (n == 0) {
					print_client_disconnected(c);
					c->socket = -1;
					c->status = OFFLINE;
					close(i);
					FD_CLR(i, &read_fd);
					fd_max = compute_fdmax(sockfd_tcp, sockfd_udp, clients);
					continue;
				}

				process_tcp_client_msg(buffer, c, clients, &topics);
			}
		}
	}
	return 0;
}