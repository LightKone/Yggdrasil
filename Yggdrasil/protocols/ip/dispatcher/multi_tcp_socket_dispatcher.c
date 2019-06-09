/*
 * multi_socket_dispatcher.c
 *
 *  Created on: May 29, 2019
 *      Author: akos
 */

#include "multi_tcp_socket_dispatcher.h"


#define MAX_SYNC_DATA_SIZE 1024

typedef enum __connection_state {
	CONNECTING,
	CONNECTED,
	ERROR,
	CLOSED,
	READING,
	SENDING,
}connection_state;


typedef struct _connections{

	IPAddr ip;
	int sockid;

	//These may be useful for more complex variants
	connection_state conn_state;
	list* buffered;


}connection;

static YggMessage* next_buffered_msg(connection* c) {
	if(c && c->buffered)
		return list_remove_head(c->buffered);
	return NULL;
}

static void buffer_msg(connection* c, YggMessage* msg) {
	if(c) {
		if(!c->buffered)
			c->buffered = list_init();

		YggMessage* heap = malloc(sizeof(YggMessage));
		*heap = *msg;

		list_add_item_to_tail(c->buffered, heap);
	}
}

static connection*  new_connection(IPAddr* ip) {
	connection* c = malloc(sizeof(connection));
	c->buffered = NULL;
	c->conn_state = CLOSED;
	c->sockid = -1;
	c->ip.port = ip->port;
	bzero(c->ip.addr, 16);
	memcpy(c->ip.addr, ip->addr, 16);

	return c;
}

static void close_connection(connection* c) {
	if(c->sockid > 0) {
	    //printf("closing socket connection %s  %d\n", c->ip.addr, c->ip.port);
        close(c->sockid);
    }

	c->sockid = -1;
	c->conn_state = CLOSED;
}

static void destroy_connection(connection* c) {

	if(c->buffered) {
		YggMessage* msg;
		while((msg = next_buffered_msg(c)) != NULL) {
			YggMessage_freePayload(msg);
			free(msg);
		}
		free(c->buffered);
	}

	close_connection(c);
	free(c);
}


static bool equal_addr(connection* conn, IPAddr* ip) {
	return strcmp(conn->ip.addr, ip->addr) == 0 && conn->ip.port == ip->port;
}

typedef struct _simple_tcp_dispatcher_state{
	unsigned short proto_id; //PROTO_DISPATCH

	pthread_t* socket_manager;

	Channel* ch;

	//pthread_mutex_t in;
	//pthread_mutex_t out;

	list* inbound; //inbound connections
	list* outbound; //outbound connections


	int events[2]; //Yggdrasil Events pipe
	int connecting_pipe[2];

	int async_pipe[2];

}simple_tcp_dispatcher_state;


static void send_notification(tcp_dispatcher_notifications type, IPAddr* ip) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "notify: %d %s %d\n", type, ip->addr, ip->port);
	ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
	YggEvent ev;
	YggEvent_init(&ev, PROTO_DISPATCH, type);
	YggEvent_addPayload(&ev, ip->addr, 16);
	YggEvent_addPayload(&ev, &ip->port, sizeof(unsigned short));
	deliverEvent(&ev);
	YggEvent_freePayload(&ev);
}

static connection* accept_handshake(int sockid, simple_tcp_dispatcher_state* state) {

	IPAddr ip;


	//1. Write my_hostname //TODO skiped


	//2. Read ip
	struct in_addr addr;
	int r = recv(sockid, &addr, sizeof(struct in_addr), 0);
	uint16_t nport;
	r = recv(sockid, &nport, sizeof(uint16_t), 0);
	char* ip_addr = inet_ntoa(addr);
	bzero(ip.addr, 16);
	memcpy(ip.addr, ip_addr, strlen(ip_addr));
	ip.port = ntohs(nport);


	//3. Read hostname //TODO skiped

	//TODO: if read fails return NULL

	connection* c = new_connection(&ip);
	c->sockid = sockid;
	c->conn_state = CONNECTED;

	return c;
}

static void connect_handshake(int sockid, simple_tcp_dispatcher_state* state) {

	//1. Read my_hostname //TODO skiped


	//2. Write ip
	struct in_addr addr;
	inet_aton(state->ch->ip.addr, &addr);
	int s = send(sockid, &addr, sizeof(struct in_addr), MSG_NOSIGNAL);
	uint16_t nport = htons(state->ch->ip.port);
	s = send(sockid, &nport, sizeof(uint16_t), MSG_NOSIGNAL);


	//3. Write hostname //TODO skiped

	//TODO: if when this fails ???????

}


static int connections_setfd(list* inbound, fd_set* reads, fd_set* except, int fd) {
	list_item* it = inbound->head;
	while(it) {
		connection* c = (connection*) it->data;

		if(c->conn_state == CONNECTED || c->conn_state == SENDING) {
            FD_SET(c->sockid, reads);
            FD_SET(c->sockid, except);

            //printf("adding socket connection %s  %d\n", c->ip.addr, c->ip.port);

            if (fd < c->sockid)
                fd = c->sockid;

        }
		it = it->next;
	}

	return fd;
}


static void acceptconnection(simple_tcp_dispatcher_state* state) {
	struct sockaddr_in address;
	unsigned int length = sizeof(address);
	int sockid = accept(state->ch->sockid, (struct sockaddr *) &address, &length);

	if(sockid > 0) {
		//TODO: CHECK SOCK OPT
		set_sock_opt(sockid, state->ch);

		connection* c = accept_handshake(sockid, state);

#if defined DEBUG | DEBUG_MULTI_DISPATCHER
		char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "performed accept to: %s  %d\n", c->ip.addr, c->ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif

		list_add_item_to_tail(state->inbound, c);

		//send_notification(CONNECTION_UP, &c->ip);

	} else {
		//fprintf(stderr, "Error on accept from %s  %d  :", it->ip.addr, it->ip.port);
		fflush(stdout);
		perror("Error on accept:");
	}
}

static int handle_read(simple_tcp_dispatcher_state* state, connection* c, YggMessage* msg) {
	uint16_t proto_id;
	int r = recv(c->sockid, &proto_id, sizeof(uint16_t), 0);
	if(r == sizeof(uint16_t)) {
		//everything ok
		uint32_t msg_size;
		r = recv(c->sockid, &msg_size, sizeof(uint32_t), 0);
		if(r == sizeof(uint32_t)) {
			//everything ok
			r = 0;
			msg->header.type = IP;
			bzero(msg->header.dst_addr.ip.addr, 16);
			bzero(msg->header.src_addr.ip.addr, 16);
			memcpy(msg->header.dst_addr.ip.addr, state->ch->ip.addr, 16);
			msg->header.dst_addr.ip.port = state->ch->ip.port;
			memcpy(msg->header.src_addr.ip.addr, c->ip.addr, 16);
			msg->header.src_addr.ip.port = c->ip.port;
			msg->Proto_id = ntohs(proto_id);
			msg->dataLen = ntohl(msg_size);
			msg->data = malloc(msg->dataLen);
			while(r < msg->dataLen && r >= 0) {
				int ret = recv(c->sockid, msg->data + r, msg->dataLen - r, 0);
				if(ret > 0)
					r += ret;
				else if(ret < 0) { //TODO: what do to if ret == 0 ???
				    perror("error on inbound read: ");
                    r = ret;
                }
				else {
				    r = ret;
				    break;
				}

			}

		}
	}

	return r;
}


static bool handle_send(connection* c, YggMessage* msg) {
	uint32_t msg_size = htonl(msg->dataLen);
	uint16_t proto_id = htons(msg->Proto_id);

	send(c->sockid, &proto_id, sizeof(uint16_t), MSG_NOSIGNAL); //the protocol header

	send(c->sockid, &msg_size, sizeof(uint32_t), MSG_NOSIGNAL); //how big is the message
	int sent = 0;
	int n;
	while(sent < msg->dataLen && sent >= 0) {
		if((n = send(c->sockid, msg->data + sent, msg->dataLen - sent, MSG_NOSIGNAL)) < 0) { //the msg itself;
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("Error on send: ");
            break;
        }

        sent += n;
	}

	return sent == msg->dataLen;
}

static void handle_inbound_reads(simple_tcp_dispatcher_state* state, fd_set* reads, fd_set* excepts) {

	list_item* it = state->inbound->head;
	list_item* prev = NULL;
	while(it) {
		connection* c = (connection*) it->data;
		bool ok = true;

		if(c->sockid > 0 && FD_ISSET(c->sockid, reads) != 0 && FD_ISSET(c->sockid, excepts) == 0) {

			YggMessage msg;
			msg.data = NULL;
			int ret = handle_read(state, c, &msg);

			if(ret == 0) {
				ok = false; //received close
			} else if (ret < 0 ) {
				fflush(stdout);
				perror("Error on read: ");
				ok = false;
			} else if (ret == msg.dataLen){ //deliver
#ifdef DEBUG
				char debug_msg[200];
				bzero(debug_msg, 200);
				sprintf(debug_msg,"delivering message for protocol %d  to source %s  %d\n", msg.Proto_id, msg.header.src_addr.ip.addr, msg.header.src_addr.ip.port);
				ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
				deliver(&msg);
			} else { //TODO: different error
				char error_msg[100];
				bzero(error_msg, 100);
				sprintf(error_msg, "error on read from connection %s  %d\n", c->ip.addr, c->ip.port);
				ygg_log("TCP_DISPATCHER", "DEBUG", error_msg);
				//TODO: should we close socket ?
			}

			if(msg.data != NULL)
				free(msg.data);


		} else if(c->sockid > 0 && FD_ISSET(c->sockid, excepts) != 0){
			//PANIC
			fflush(stdout);
			perror("Error on read: ");
			ok = false;
		}

		if(!ok) { //close connection, remove it;
			it = it->next;
			if(!prev) {
				list_remove_head(state->inbound);
			} else {
				list_remove(state->inbound, prev);
			}
			//send_notification(TCP_DISPATCHER_CONNECTION_DOWN, &c->ip);
#if defined DEBUG | DEBUG_MULTI_DISPATCHER
            char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "closed inbound to: %s  %d\n", c->ip.addr, c->ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
			destroy_connection(c);
		} else {
			prev = it;
			it = it->next;
		}
	}
}

typedef struct _async_read_args {

    int sockid;
    IPAddr ip;
    simple_tcp_dispatcher_state* state;

}async_read_args;

typedef struct _async_status {
    IPAddr ip;
    bool read;
    connection_state state;
}async_status;


static void handle_async_read(async_read_args* args) {

    YggMessage msg;
    async_status status;
    status.read = true;
    status.state = ERROR;
    status.ip = args->ip;

    uint16_t proto_id;
    int r = recv(args->sockid, &proto_id, sizeof(uint16_t), 0);
    if(r == sizeof(uint16_t)) {
        //everything ok
        uint32_t msg_size;
        r = recv(args->sockid, &msg_size, sizeof(uint32_t), 0);
        if(r == sizeof(uint32_t)) {
            //everything ok
            r = 0;
            msg.header.type = IP;
            bzero(msg.header.dst_addr.ip.addr, 16);
            bzero(msg.header.src_addr.ip.addr, 16);
            memcpy(msg.header.dst_addr.ip.addr, args->state->ch->ip.addr, 16);
            msg.header.dst_addr.ip.port = args->state->ch->ip.port;
            memcpy(msg.header.src_addr.ip.addr, args->ip.addr, 16);
            msg.header.src_addr.ip.port = args->ip.port;
            msg.Proto_id = ntohs(proto_id);
            msg.dataLen = ntohl(msg_size);
            msg.data = malloc(msg.dataLen);
            while(r < msg.dataLen && r >= 0) {
                int ret = recv(args->sockid, msg.data + r, msg.dataLen - r, 0);
                if(ret > 0)
                    r += ret;
                else if(ret < 0) { //TODO: what do to if ret == 0 ???
                    r = ret;
                    perror("Error on async read: ");
                }else {
                    r = ret;
                    break;
                }
            }

            if(r == msg.dataLen) {
                deliver(&msg);
                status.state = CONNECTED;
            }

            YggMessage_freePayload(&msg);
        }
    }


    write(args->state->async_pipe[1], &status, sizeof(async_status));
    free(args);
}

static void handle_outbound_reads(simple_tcp_dispatcher_state* state, fd_set* reads, fd_set* excepts) {
    list_item* it = state->outbound->head;
    list_item* prev = NULL;
    while(it) {
        connection* c = (connection*) it->data;
        bool ok = true;

        if(c->sockid > 0 && FD_ISSET(c->sockid, reads) != 0 && FD_ISSET(c->sockid, excepts) == 0) {

            async_read_args* args = malloc(sizeof(async_read_args));
            args->sockid = c->sockid;
            args->state = state;
            args->ip = c->ip;

            c->conn_state = READING;
            pthread_t t;
            pthread_create(&t, NULL, handle_async_read, args);
            pthread_detach(t);

        } else if(c->sockid > 0 && FD_ISSET(c->sockid, excepts) != 0){
            //PANIC
            fflush(stdout);
            perror("Error on read: ");
            ok = false;
        }

        if(!ok) { //close connection, remove it;
             it = it->next;
            if(!prev) {
                list_remove_head(state->outbound);
            } else {
                list_remove(state->outbound, prev);
            }
            send_notification(TCP_DISPATCHER_CONNECTION_DOWN, &c->ip);
#if defined DEBUG | DEBUG_MULTI_DISPATCHER
            char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "closed outbound to: %s  %d\n", c->ip.addr, c->ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
            destroy_connection(c);
        } else {
            prev = it;
            it = it->next;
        }
    }
}


typedef struct _async_send_args {
    YggMessage* msg;
    int sockid;
    simple_tcp_dispatcher_state* state;
}async_send_args;

static void handle_async_send(async_send_args* args) { //this may not be safe...

    async_status status;
    status.read = false;
    status.state = CONNECTED;
    status.ip = args->msg->header.dst_addr.ip;

    printf("To send async large message of size: %d  to %s  %d\n", args->msg->dataLen, args->msg->header.dst_addr.ip.addr, args->msg->header.dst_addr.ip.port);

    uint32_t msg_size = htonl(args->msg->dataLen);
    uint16_t proto_id = htons(args->msg->Proto_id);

    send(args->sockid, &proto_id, sizeof(uint16_t), MSG_NOSIGNAL); //the protocol header

    send(args->sockid, &msg_size, sizeof(uint32_t), MSG_NOSIGNAL); //how big is the message
    int sent = 0;
    int n;
    while(sent < args->msg->dataLen && sent >= 0) {
        if((n = send(args->sockid, args->msg->data + sent, args->msg->dataLen - sent, MSG_NOSIGNAL)) < 0) { //the msg itself;
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("Error on async send: ");
            break;
        }

        sent += n;
    }



    YggMessage_freePayload(args->msg);
    free(args->msg);

    write(args->state->async_pipe[1], &status, sizeof(async_status));
    free(args);


}

static void handle_async_pipe(simple_tcp_dispatcher_state* state) {

    async_status status;
    read(state->async_pipe[0], &status, sizeof(async_status));

    if(status.read) {
        connection *c = list_find_item(state->outbound, (equal_function) equal_addr, &status.ip);
        if (c) {
            if (status.state == ERROR) {
                c = list_remove_item(state->outbound, (equal_function) equal_addr, &status.ip);
                send_notification(TCP_DISPATCHER_CONNECTION_DOWN, &c->ip);
                destroy_connection(c);
            } else {
                c->conn_state = status.state; //CONNECTED
                //list_add_item_to_head(state->outbound, c);
            }
        }
    } else {
        connection *c = list_find_item(state->inbound, (equal_function) equal_addr, &status.ip);
        if (c) {
            YggMessage* msg;
            if((msg = next_buffered_msg(c)) != NULL) {

                async_send_args *args = malloc(sizeof(async_send_args));
                args->msg = msg;
                args->sockid = c->sockid;
                args->state = state;

                pthread_t t;
                pthread_create(&t, NULL, handle_async_send, args);
                pthread_detach(t);
            } else
                c->conn_state = status.state; //CONNECTED
                //list_add_item_to_head(state->outbound, c);
        }
    }

}

typedef struct __connect_args {
	simple_tcp_dispatcher_state* state;
	IPAddr connection_ip;
}connect_args;

typedef struct __connecting_status {
	IPAddr connection_ip;
	connection_state state;
	int sockid;
}connecting_status;

static void updateconnection(simple_tcp_dispatcher_state* state) {
	connecting_status status;
	read(state->connecting_pipe[0], &status, sizeof(connecting_status));

	connection* c = list_remove_item(state->outbound, (equal_function) equal_addr, &status.connection_ip);

	if(c) {
		c->conn_state = status.state;
		if(c->conn_state == CONNECTED) {
			send_notification(TCP_DISPATCHER_CONNECTION_UP, &c->ip);
			list_add_item_to_tail(state->outbound, c);
			c->sockid = status.sockid;
			YggMessage* msg;
			while((msg = next_buffered_msg(c)) != NULL) {
				if(!handle_send(c, msg)) {
					//TODO: ERROR CODE
					send_notification(TCP_DISPATCHER_CONNECTION_DOWN, &c->ip);
					list_remove_head(state->outbound);
					destroy_connection(c);
					break;
				}
				YggMessage_freePayload(msg);
				free(msg);
			}
		} else if(c->conn_state == ERROR) {
			send_notification(TCP_DISPATCHER_UNABLE_TO_CONNECT, &c->ip);
			destroy_connection(c);
		}

	}
}

static void handle_connect(connect_args* args) {

	simple_tcp_dispatcher_state* state = args->state;

	connecting_status status;
	status.connection_ip = args->connection_ip;


	struct sockaddr_in addr;
	addr.sin_family = AF_INET;

	int sockid = socket(AF_INET, SOCK_STREAM, 0);

	//TODO CHECK SOCK OPT
	set_sock_opt(sockid, state->ch);

	inet_pton(AF_INET, status.connection_ip.addr, &(addr.sin_addr));
	addr.sin_port = htons(status.connection_ip.port);

	if((connect(sockid, (struct sockaddr*) &addr,  sizeof(addr))) == 0) {

		connect_handshake(sockid, state);
		status.sockid = sockid;

#ifdef VERIFY
		struct timeval tsp;
		gettimeofday(&tsp, NULL);
		printf("%ld Dispatcher OPEN %d to %d\n", (tsp.tv_sec*1000*1000) + tsp.tv_usec, state->ch->ip.port, status.connection_ip.port);
#endif

#if defined DEBUG | DEBUG_MULTI_DISPATCHER
		char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "performed connect to: %s  %d\n", status.connection_ip.addr, status.connection_ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
		status.state = CONNECTED;
	} else {
		fflush(stdout);
		fprintf(stderr, "Error on connect to %s  %d  :", status.connection_ip.addr, status.connection_ip.port);
		perror("");
		close(sockid);
		status.state = ERROR;
	}

	write(state->connecting_pipe[1], &status, sizeof(connecting_status));
	free(args);
}

static void perform_async_connect(connection* c, simple_tcp_dispatcher_state* state) {

	connect_args* args = malloc(sizeof(connect_args));
	args->state = state;
	memcpy(args->connection_ip.addr, c->ip.addr, 16);
	args->connection_ip.port = c->ip.port;

	pthread_t t;
	pthread_create(&t, NULL, handle_connect, args);
	pthread_detach(t);

	c->conn_state = CONNECTING;

}

static void process_small_msg(simple_tcp_dispatcher_state* state, YggMessage* msg ) {
    connection* c = list_find_item(state->outbound, (equal_function) equal_addr, &msg->header.dst_addr.ip);

    if(!c) {
        c = new_connection(&msg->header.dst_addr.ip);
        list_add_item_to_tail(state->outbound, c);

        buffer_msg(c, msg);
        perform_async_connect(c, state);
    } else if(c->conn_state == CONNECTING) {
        buffer_msg(c, msg);
    } else { //assume connected state ?
        if(!handle_send(c, msg)) {
            //TODO: ERROR CODE
            send_notification(TCP_DISPATCHER_CONNECTION_DOWN, &c->ip);
            list_remove_item(state->outbound, (equal_function) equal_addr, &c->ip);
            destroy_connection(c);
        }
        YggMessage_freePayload(msg);
    }
}

static void process_large_msg(simple_tcp_dispatcher_state* state, YggMessage* msg) {
       connection* c = list_find_item(state->inbound, (equal_function) equal_addr, &msg->header.dst_addr.ip);

       if(!c) { //Ignore for now
           //SOME error or force otherside to perform connect ?????
           printf("No connection available to send large message. To destination %s %d\n", msg->header.dst_addr.ip.addr, msg->header.dst_addr.ip.port);
       } else {
           if(c->conn_state == SENDING) {
               buffer_msg(c, msg);
           } else {
               c->conn_state = SENDING;

               async_send_args *args = malloc(sizeof(async_send_args));
               args->msg = malloc(sizeof(YggMessage));
               *(args->msg) = *msg;
               args->sockid = c->sockid;
               args->state = state;

               pthread_t t;
               pthread_create(&t, NULL, handle_async_send, args);
               pthread_detach(t);
           }
       }

}

static void process_msg(simple_tcp_dispatcher_state* state, YggMessage* msg) {

    if(msg->dataLen < 1024)
        process_small_msg(state, msg);
    else
        process_large_msg(state, msg);



}

static void close_connection_to(simple_tcp_dispatcher_state* state, IPAddr* ip) {
	connection* c = list_remove_item(state->outbound, (equal_function) equal_addr, ip);
	if(c) {
		send_notification(TCP_DISPATCHER_CONNECTION_DOWN, ip);
#if defined DEBUG | DEBUG_MULTI_DISPATCHER
        char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "closed outbound to: %s  %d\n", c->ip.addr, c->ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
		destroy_connection(c);
	}

	c = list_remove_item(state->inbound, (equal_function) equal_addr, ip);
	if(c) {
#if defined DEBUG | DEBUG_MULTI_DISPATCHER
        char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "closed inbound to: %s  %d\n", c->ip.addr, c->ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
#endif
        destroy_connection(c);

    }
}

static void process_request(simple_tcp_dispatcher_state* state, YggRequest* req) {
	if(req->request == REQUEST && req->request_type == CLOSE_CONNECTION) {
		IPAddr ip;
		bzero(ip.addr, 16);
		void* ptr = YggRequest_readPayload(req, NULL, ip.addr, 16);
		YggRequest_readPayload(req, ptr, &ip.port, sizeof(unsigned short));
		close_connection_to(state, &ip);
	}

	YggRequest_freePayload(req);
}

static void handle_events(simple_tcp_dispatcher_state* state) {

	queue_t_elem elem;
	read(state->events[0], &elem, sizeof(queue_t_elem));

	switch(elem.type){
	case YGG_MESSAGE:
		process_msg(state, &elem.data.msg);
		break;
	case YGG_TIMER: //noop
		break;
	case YGG_EVENT: //noop
		break;
	case YGG_REQUEST:
		process_request(state, &elem.data.request);
		break;
	}

}

static void manage_sockets(simple_tcp_dispatcher_state* state) {

	pipe(state->connecting_pipe);
	pipe(state->async_pipe);


	fd_set readfds;
	fd_set exceptionfds;

	while(1) {
		int largest_fd = -1;
		FD_ZERO(&readfds);
		FD_ZERO(&exceptionfds);

		FD_SET(state->ch->sockid, &readfds);
		FD_SET(state->ch->sockid, &exceptionfds);

		largest_fd = state->ch->sockid; //listen socket

		FD_SET(state->connecting_pipe[0], &readfds);
		FD_SET(state->connecting_pipe[0], &exceptionfds);

		if(state->connecting_pipe[0] > largest_fd)
			largest_fd = state->connecting_pipe[0];


		FD_SET(state->events[0], &readfds);
		FD_SET(state->events[0], &exceptionfds);

		if(state->events[0] > largest_fd)
			largest_fd = state->events[0];


        FD_SET(state->async_pipe[0], &readfds);
        FD_SET(state->async_pipe[0], &exceptionfds);

        if(state->async_pipe[0] > largest_fd)
            largest_fd = state->async_pipe[0];


		largest_fd = connections_setfd(state->inbound, &readfds, &exceptionfds, largest_fd);
        largest_fd = connections_setfd(state->outbound, &readfds, &exceptionfds, largest_fd);

		//block until available
		int ret = select(largest_fd+1, &readfds, NULL, &exceptionfds, NULL);

		if(ret < 0) {
			fflush(stdout);
			perror("Error on select:");
			continue; //go back to the top of the while (manipulated stuff, probably a close, that is the fdset)
		}

		if(FD_ISSET(state->ch->sockid, &readfds) != 0 && FD_ISSET(state->ch->sockid, &exceptionfds) == 0) {
			acceptconnection(state);
		} else if(FD_ISSET(state->ch->sockid, &exceptionfds) != 0){
			//PANIC
		}

		if(FD_ISSET(state->connecting_pipe[0], &readfds) != 0 && FD_ISSET(state->connecting_pipe[0], &exceptionfds) == 0) {
			updateconnection(state);
		} else if(FD_ISSET(state->connecting_pipe[0], &exceptionfds) != 0){
			//PANIC
		}

		if(FD_ISSET(state->events[0], &readfds) != 0 && FD_ISSET(state->events[0], &exceptionfds) == 0) {
			handle_events(state);
		} else if(FD_ISSET(state->events[0], &exceptionfds) != 0){
			//PANIC
		}

        if(FD_ISSET(state->async_pipe[0], &readfds) != 0 && FD_ISSET(state->async_pipe[0], &exceptionfds) == 0) {
            handle_async_pipe(state);
        } else if(FD_ISSET(state->async_pipe[0], &exceptionfds) != 0){
            //PANIC
        }

		handle_inbound_reads(state, &readfds, &exceptionfds);

	    handle_outbound_reads(state, &readfds, &exceptionfds);
	}

}


static void* simple_tcp_dispatcher_main_loop(main_loop_args* args) {

	simple_tcp_dispatcher_state* state = (simple_tcp_dispatcher_state*) args->state;
	queue_t* inBox = args->inBox;

	state->socket_manager = malloc(sizeof(pthread_t));

	pthread_create(state->socket_manager, NULL, &manage_sockets, (void*) state);

	while(1) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);
		write(state->events[1], &elem, sizeof(queue_t_elem));
	}
}


static short destroy(simple_tcp_dispatcher_state* state) {

	if(state->socket_manager) {
		pthread_cancel(*state->socket_manager);
		free(state->socket_manager);
	}


	close(state->events[0]);
	close(state->events[1]);

	close(state->connecting_pipe[0]);
	close(state->connecting_pipe[1]);

	//at the end
	free(state);

	return SUCCESS;
}

proto_def* multi_tcp_socket_dispatcher_init(Channel* ch, void* args) {

	simple_tcp_dispatcher_state* state = malloc(sizeof(simple_tcp_dispatcher_state));
	state->ch = ch;
	state->socket_manager = NULL;
	state->inbound = list_init();
	state->outbound = list_init();

	pipe(state->events);

	//pthread_mutex_init(&state->in, NULL);
	//pthread_mutex_init(&state->out, NULL);

	proto_def* dispatcher = create_protocol_definition(PROTO_DISPATCH, "multi tcp dispatcher", (void*) state, (Proto_destroy) destroy);

	proto_def_add_produced_events(dispatcher, 3/*4?*/); //conn up, conn down, unable to conn, failed to send? :/

	proto_def_add_protocol_main_loop(dispatcher, simple_tcp_dispatcher_main_loop);

	return dispatcher;
}

