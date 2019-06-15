/*
 * simple_tls_dispatcher.c
 *
 *  Created on: May 26, 2019
 *      Author: akos
 */

#include "simple_tls_dispatcher.h"


typedef enum __connection_state {
	CONNECTING,
	CONNECTED,
	ERROR,
	CLOSING,
	CLOSED
}connection_state;

typedef struct _connections{

	IPAddr ip;
	int sockid;

	SSL* ssl;

	//These may be useful for more complex variants
	connection_state conn_state;
	list* buffered;

}connection;

static bool equal_addr(connection* conn, IPAddr* ip) {
	return strcmp(conn->ip.addr, ip->addr) == 0 && conn->ip.port == ip->port;
}


typedef struct _simple_tcp_dispatcher_state{
	unsigned short proto_id; //PROTO_DISPATCH

	pthread_t* receiver;

	Channel* ch;

	SSL_CTX * sslctx;

	list* inbound;
	list* outbound;

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

static void ShowCerts(SSL* ssl)
{   X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);       /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);       /* free the malloc'ed string */
        X509_free(cert);     /* free the malloc'ed certificate copy */
    }
    else
        printf("Info: No client certificates configured.\n");
}


static void ShutdownSSL(SSL* ssl)
{
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

static connection* accept_handshake(int sockid, simple_tcp_dispatcher_state* state) {

	connection* c = malloc(sizeof(connection));
	c->sockid = sockid;

	printf("accepting\n");
	c->ssl = SSL_new(state->sslctx);
	SSL_set_fd(c->ssl, c->sockid);

	int ret;
	if((ret = SSL_accept(c->ssl)) <= 0) {
		//printf("Error: %s\n", ERR_reason_error_string(ERR_get_error()));
		ERR_print_errors_fp(stderr);
	}
	printf("verify %d\n", SSL_get_verify_result(c->ssl));
	printf("Connected with %s encryption\n", SSL_get_cipher(c->ssl));
	ShowCerts(c->ssl);
	printf("ssl acceptted\n");

	//1. Write my_hostname //TODO skiped


	//2. Read ip
	struct in_addr addr;
	SSL_read(c->ssl, &addr, sizeof(struct in_addr));
	//int r = recv(c->sockid, &addr, sizeof(struct in_addr), 0);
	uint16_t nport;
	SSL_read(c->ssl, &nport, sizeof(uint16_t));
	//r = recv(c->sockid, &nport, sizeof(uint16_t), 0);
	char* ip_addr = inet_ntoa(addr);
	bzero(c->ip.addr, 16);
	memcpy(c->ip.addr, ip_addr, strlen(ip_addr));
	c->ip.port = ntohs(nport);

	printf("ssl read\n");
	//3. Read hostname //TODO skiped

	//TODO: if read fails return NULL

	return c;
}

static void connect_handshake(connection* c, simple_tcp_dispatcher_state* state) {

	printf("connecting\n");
	c->ssl = SSL_new(state->sslctx);
	SSL_set_fd(c->ssl, c->sockid);

	int ret;
	if((ret =SSL_connect(c->ssl)) <= 0) {
		//printf("Error: %s\n", ERR_reason_error_string(ERR_get_error()));
		 ERR_print_errors_fp(stderr);
	}
	printf("verify %d\n", SSL_get_verify_result(c->ssl));
	printf("Connected with %s encryption\n", SSL_get_cipher(c->ssl));
	ShowCerts(c->ssl);
	printf("ssl connected\n");

	//1. Read my_hostname //TODO skiped


	//2. Write ip
	struct in_addr addr;
	inet_aton(state->ch->ip.addr, &addr);
	SSL_write(c->ssl, &addr, sizeof(struct in_addr));
	//int s = send(c->sockid, &addr, sizeof(struct in_addr), MSG_NOSIGNAL); //this is ASCII
	uint16_t nport = htons(state->ch->ip.port);
	SSL_write(c->ssl, &nport, sizeof(uint16_t));
	//s = send(c->sockid, &nport, sizeof(uint16_t), MSG_NOSIGNAL); //this is ASCII

	printf("ssl writted\n");

	//3. Write hostname //TODO skiped

	//TODO: if when this fails ???????

}


static int connections_setfd(list* inbound, fd_set* reads, fd_set* except, int fd) {
	list_item* it = inbound->head;
	while(it) {
		connection* c = (connection*) it->data;

		FD_SET(c->sockid, reads);
		FD_SET(c->sockid, except);

		if(fd < c->sockid)
			fd = c->sockid;

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
#ifdef DEBUG
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
	int r = SSL_read(c->ssl, &proto_id, sizeof(uint16_t));
	//int r = recv(c->sockid, &proto_id, sizeof(uint16_t), 0);
	if(r == sizeof(uint16_t)) {
		//everything ok
		uint32_t msg_size;
		r = SSL_read(c->ssl, &msg_size, sizeof(uint32_t));
		//r = recv(c->sockid, &msg_size, sizeof(uint32_t), 0);
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

			while(r < msg->dataLen && r >= 0 /*&& SSL_pending(c->ssl)*/) {
				int ret = SSL_read(c->ssl, msg->data +r, msg->dataLen -r);
				//int ret = recv(c->sockid, msg->data + r, msg->dataLen - r, 0);
				if(ret > 0)
					r += ret;
				else if(ret < 0) //TODO: what do to if ret == 0 ???
					r = ret;
			}

		}
	}

	return r;
}

static void simple_tcp_dispatcher_receiver(simple_tcp_dispatcher_state* state) {

	fd_set readfds;
	fd_set exceptionfds;

	while(1) {
		int largest_fd = -1;
		FD_ZERO(&readfds);
		FD_ZERO(&exceptionfds);

		FD_SET(state->ch->sockid, &readfds);
		FD_SET(state->ch->sockid, &exceptionfds);

		largest_fd = state->ch->sockid; //listen socket

		largest_fd = connections_setfd(state->inbound, &readfds, &exceptionfds, largest_fd);

		//block until available
		int ret = select(largest_fd+1, &readfds, NULL, &exceptionfds, NULL);

		if(ret < 0) {
			fflush(stdout);
			perror("Error on select:");
			continue; //go back to the top of the while (manipulated stuff, probably a close, that is the fdset)
		}

		if(FD_ISSET(state->ch->sockid, &readfds) != 0) {
			if(FD_ISSET(state->ch->sockid, &exceptionfds) != 0){
				//PANIC
			}else {
				acceptconnection(state);
			}

		} else if(FD_ISSET(state->ch->sockid, &exceptionfds) != 0){
			//PANIC

		}

		list_item* it = state->inbound->head;
		list_item* prev = NULL;
		while(it) {
			connection* c = (connection*) it->data;
			bool ok = true;

			if(FD_ISSET(c->sockid, &readfds) != 0 && FD_ISSET(c->sockid, &exceptionfds) == 0) {

				YggMessage msg;
				msg.data = NULL;
				ret = handle_read(state, c, &msg);

				if(ret == 0) {
					ok = false;
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


			} else if(FD_ISSET(c->sockid, &exceptionfds) != 0){
				//PANIC
				fflush(stdout);
				perror("Error on read: ");
				ok = false;
			}

			//TODO handle this correctly
			if(!ok) { //close connection, remove it;
				it = it->next;
				if(!prev) {
					list_remove_head(state->inbound);
				} else {
					list_remove(state->inbound, prev);
				}
				send_notification(TCP_DISPATCHER_CONNECTION_DOWN, &c->ip);
				ShutdownSSL(c->ssl);
				close(c->sockid);
				free(c);
			} else {
				prev = it;
				it = it->next;
			}
		}

	}

}

static connection* establish_connection(simple_tcp_dispatcher_state* state, YggMessage* msg) {
	connection* c = NULL;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;

	int sockid = socket(AF_INET, SOCK_STREAM, 0);

	//TODO CHECK SOCK OPT
	set_sock_opt(sockid, state->ch);

	inet_pton(AF_INET, msg->header.dst_addr.ip.addr, &(addr.sin_addr));
	addr.sin_port = htons(msg->header.dst_addr.ip.port);

	if((connect(sockid, (struct sockaddr*) &addr,  sizeof(addr))) == 0) {
		c = malloc(sizeof(connection));
		c->sockid = sockid;
		c->ip = msg->header.dst_addr.ip;

		connect_handshake(c, state);

#ifdef VERIFY
		struct timeval tsp;
		gettimeofday(&tsp, NULL);
		printf("%ld Dispatcher OPEN %d to %d\n", (tsp.tv_sec*1000*1000) + tsp.tv_usec, state->ch->ip.port, msg->header.dst_addr.ip.port);
#endif
//#ifdef DEBUG
		char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "performed connect to: %s  %d\n", c->ip.addr, c->ip.port);
		ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
//#endif
		list_add_item_to_tail(state->outbound, c);
		send_notification(TCP_DISPATCHER_CONNECTION_UP, &c->ip);

	} else {
		fflush(stdout);
		fprintf(stderr, "Error on connect to %s  %d  :", msg->header.dst_addr.ip.addr, msg->header.dst_addr.ip.port);
		perror("");
		close(sockid);
	}
	return c;
}

static bool handle_send(connection* c, YggMessage* msg) {
	uint32_t msg_size = htonl(msg->dataLen);
	uint16_t proto_id = htons(msg->Proto_id);

	SSL_write(c->ssl, &proto_id, sizeof(uint16_t));
	//send(c->sockid, &proto_id, sizeof(uint16_t), MSG_NOSIGNAL); //the protocol header

	SSL_write(c->ssl, &msg_size, sizeof(uint32_t));
	//send(c->sockid, &msg_size, sizeof(uint32_t), MSG_NOSIGNAL); //how big is the message
	int sent = 0;
	while(sent < msg->dataLen && sent >= 0) {
		sent = SSL_write(c->ssl, msg->data + sent, msg->dataLen -sent);
		//sent = send(c->sockid, msg->data + sent, msg->dataLen - sent, MSG_NOSIGNAL); //the msg itself;
	}

	return sent == msg->dataLen;
}


static void send_msg(simple_tcp_dispatcher_state* state, YggMessage* msg) {

	if(msg->header.type != IP) {
		char warning_msg[200];
		bzero(warning_msg, 200);
		sprintf(warning_msg, "Protocol %d is trying to dispatch a mac address message. This dispatcher only supports IP messages", msg->Proto_id);
		ygg_log("TCP_DISPATCHER", "WARNING", warning_msg);
		return;
	}


	connection* c = list_find_item(state->outbound, (equal_function) equal_addr, &msg->header.dst_addr.ip);
	if(!c) {
		c = establish_connection(state, msg);
	}

	if(c) {
		bool ok = handle_send(c, msg);
		if(!ok) { //TODO: handle this correctly
			list_remove_item(state->outbound, (equal_function) equal_addr, &c->ip);
			//send_notification(CONNECTION_DOWN, &c->ip);
			ShutdownSSL(c->ssl);
			close(c->sockid);
			free(c);

		}
#ifdef DEBUG
		else {
			char debug_msg[200];
			bzero(debug_msg, 200);
			sprintf(debug_msg, "sent message for protocol %d  to destination %s  %d\n", msg->Proto_id, msg->header.dst_addr.ip.addr, msg->header.dst_addr.ip.port);
			ygg_log("TCP_DISPATCHER", "DEBUG", debug_msg);
		}
#endif

	}else
		send_notification(TCP_DISPATCHER_UNABLE_TO_CONNECT, &msg->header.dst_addr.ip);

	YggMessage_freePayload(msg);
}


static void close_connection_to(simple_tcp_dispatcher_state* state, IPAddr* ip) {
	connection* c = list_remove_item(state->outbound, (equal_function) equal_addr, ip);
	if(c) { //TODO: handle this correctly
		//send_notification(CONNECTION_DOWN, ip);
#ifdef VERIFY
		struct timeval tsp;
		gettimeofday(&tsp, NULL);
		printf("%ld Dispatcher CLOSE %d to %d\n", (tsp.tv_sec*1000*1000) + tsp.tv_usec, state->ch->ip.port, ip->port);
#endif
		ShutdownSSL(c->ssl);
		close(c->sockid);
		free(c);
	}else {
#ifdef DEBUG
		char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "connection to close %s  %d does not exist\n", ip->addr, ip->port);
		ygg_log("TCP_DISPATCHER", "DEBUB", debug_msg);
#endif
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

static void process_event(simple_tcp_dispatcher_state* state, YggEvent* ev) {
	if(ev->proto_origin == PROTO_DISPATCH && ev->notification_id == TCP_DISPATCHER_CONNECTION_DOWN) {
		IPAddr ip;
		bzero(ip.addr, 16);
		void* ptr = YggEvent_readPayload(ev, NULL, ip.addr, 16);
		YggEvent_readPayload(ev, ptr, &ip.port, sizeof(unsigned short));

		close_connection_to(state, &ip);
	}
	YggEvent_freePayload(ev);
}

static void* simple_tcp_dispatcher_main_loop(main_loop_args* args) {

	simple_tcp_dispatcher_state* state = (simple_tcp_dispatcher_state*) args->state;

	queue_t* inBox = args->inBox;

	state->receiver = malloc(sizeof(pthread_t));
	pthread_attr_t patribute;
	pthread_attr_init(&patribute);

	pthread_create(state->receiver, &patribute, &simple_tcp_dispatcher_receiver, (void*) state);

	while(1) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		switch(elem.type) {
		case YGG_MESSAGE:
			send_msg(state, &elem.data.msg);
			break;
		case YGG_TIMER:
			//no op
			break;
		case YGG_EVENT:
			process_event(state, &elem.data.event);
			break;
		case YGG_REQUEST:
			process_request(state, &elem.data.request);
			break;
		default:
			break;
		}

	}
}


static short destroy(simple_tcp_dispatcher_state* state) {

	if(state->receiver) {
		pthread_cancel(*state->receiver);
		free(state->receiver);
	}
	//at the end
	free(state);

	return SUCCESS;
}

static void InitializeSSL()
{

	SSL_library_init();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
}

static void DestroySSL()
{
    ERR_free_strings();
    EVP_cleanup();
}

void LoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_load_verify_locations(ctx, "/home/akos/Production/Security/TCert.pem", NULL);
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION);

}

proto_def* simple_tls_dispatcher_init(Channel* ch, void* args) {

	simple_tcp_dispatcher_state* state = malloc(sizeof(simple_tcp_dispatcher_state));
	state->ch = ch;
	state->receiver = NULL;
	state->inbound = list_init();
	state->outbound = list_init();

	/*********** CONFIG SSL ***************/

	InitializeSSL();

	state->sslctx = SSL_CTX_new( SSLv23_method());
	//SSL_CTX_set_options(state->sslctx, SSL_OP_SINGLE_DH_USE);

	//TODO: to be configured by param
	LoadCertificates(state->sslctx, "/home/akos/Production/Security/Cert.pem", "/home/akos/Production/Security/Cert.pem");

	/*************************************/

	proto_def* dispatcher = create_protocol_definition(PROTO_DISPATCH, "simple tcp dispatcher", (void*) state, (destroy_function) destroy);

	proto_def_add_produced_events(dispatcher, 3/*4?*/); //conn up, conn down, unable to conn, failed to send? :/
	proto_def_add_consumed_event(dispatcher, PROTO_DISPATCH, TCP_DISPATCHER_CONNECTION_DOWN);

	proto_def_add_protocol_main_loop(dispatcher, simple_tcp_dispatcher_main_loop);

	return dispatcher;
}
