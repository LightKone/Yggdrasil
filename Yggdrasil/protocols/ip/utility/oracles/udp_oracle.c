/*
 * simple_udp_oracle.c
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#include "udp_oracle.h"

#define MAX_SEQ_NUM 5555

typedef enum _msg_types{
	PING,
	PONG
}oracle_msg_types;

typedef struct _request {
	YggRequest* req;
	double* ptr_to_rtt;
}request;

static request* create_request(YggRequest* req, void* ptr) {
	request* r = malloc(sizeof(request));
	r->req = req;
	r->ptr_to_rtt = (double*) ptr;
	return r;
}

static bool equal_request(request* r, YggRequest* req) {
	return r->req->proto_dest == req->proto_dest;
}

static void destroy_request(request* r) {
	YggRequest_freePayload(r->req);
	free(r->req);
	free(r);
}

typedef struct _to_measure{
	list* requests;

	IPAddr ip;

	unsigned short seq_num;
	YggTimer timer;

	//rtt = end - start;
	struct timespec start;
	struct timespec end;
}to_measure;

static void destroy_measurement(to_measure* m) {
	YggTimer_freePayload(&m->timer);
	free(m->requests);
	free(m);
}

static bool equal_seq_num(to_measure* m, unsigned short seq_num) {
	return m->seq_num == seq_num;
}

static bool equal_peer(to_measure* m, IPAddr* addr) {
	return (strcmp(m->ip.addr, addr->addr) == 0 && m->ip.port == addr->port);
}


typedef struct __udp_oracle_state {

	unsigned short proto_id;

	list* measurements;

	unsigned short current_seq_num;
	int period;

	IPAddr self;

	int localpipe[2];
	int socket;

}udp_oracle_state;


static void deliver_replies(udp_oracle_state* state, to_measure* m, double rtt) {
	for(list_item* it = m->requests->head; it != NULL; it=it->next) {
		request* req = (request*)it->data;
		*(req->ptr_to_rtt) = rtt;
		deliverReply(req->req);
	}
}

static void add_to_measure(udp_oracle_state* state, YggRequest* req) {

	IPAddr ip;

	req->request = REPLY;
	req->proto_dest = req->proto_origin;
	req->proto_origin = state->proto_id;

	double rtt = INFINITY;
	YggRequest_addPayload(req, &rtt, sizeof(double));

	void* ptr = YggRequest_readPayload(req, NULL, ip.addr, 16);
	ptr = YggRequest_readPayload(req, ptr, &ip.port, sizeof(unsigned short));

	to_measure* m = list_find_item(state->measurements, equal_peer, &ip);

	if(!m) {
		m = malloc(sizeof(to_measure));
		m->requests = list_init();
		memcpy(m->ip.addr, ip.addr, 16);
		m->ip.port = ip.port;

		request* r = create_request(req, ptr);
		list_add_item_to_tail(m->requests, r);

		list_add_item_to_tail(state->measurements, m);

		YggTimer_init(&m->timer, state->proto_id, state->proto_id);
		YggTimer_set(&m->timer, 0, 5000000, state->period, 0); //first notification now (or almost now)
		YggTimer_addPayload(&m->timer, m->ip.addr, 16);
		YggTimer_addPayload(&m->timer, &m->ip.port, sizeof(unsigned short));
		setupTimer(&m->timer);
	} else {
		request* r = list_find_item(m->requests, equal_request, req);
		if(!r) {
			r = create_request(req, ptr);
			list_add_item_to_tail(m->requests, r);
		}
	}

}

static void cancel_measurent(udp_oracle_state* state, YggRequest* req) {
	IPAddr ip;

	void* ptr = YggRequest_readPayload(req, NULL, ip.addr, 16);
	ptr = YggRequest_readPayload(req, ptr, &ip.port, sizeof(unsigned short)); //this needs to be the udp port

	to_measure* m = list_find_item(state->measurements, equal_peer, &ip);
	if(m) {
		req->proto_dest = req->proto_origin;
		request* r = list_remove_item(m->requests, equal_request, req);
		if(r) {
			destroy_request(r);
		}
		if(m->requests->size == 0) {
			m = list_remove_item(state->measurements, equal_peer, &ip);
			cancelTimer(&m->timer);
			destroy_measurement(m);
		}
	}

	YggRequest_freePayload(req);
}


static void send_ping(udp_oracle_state* state, to_measure* dest) {


	dest->seq_num = (state->current_seq_num++)%MAX_SEQ_NUM;
	uint16_t ping = htons(PING);
	uint16_t seq_num = htons(dest->seq_num);

	uint16_t* buffer = malloc(sizeof(uint16_t)*2);
	buffer[0]=ping;
	buffer[1]=seq_num;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	inet_aton(dest->ip.addr, &addr.sin_addr);
	addr.sin_port = htons(dest->ip.port);

	clock_gettime(CLOCK_MONOTONIC, &dest->start);

	sendto(state->socket, (void*) buffer, sizeof(uint16_t)*2, 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in));
	free(buffer);
	//printf("===============>  sent ping  %s  %d\n", dest->ip.addr, dest->ip.port);
}

static void send_pong(udp_oracle_state* state, uint16_t* buffer, struct sockaddr_in* addr) {

	buffer[0] = htons(PONG);
	sendto(state->socket, (void*) buffer, sizeof(uint16_t)*2, 0, (struct sockaddr*) addr, sizeof(struct sockaddr_in));

	//printf("===============>  sent pong\n");
}


static void do_measurement(udp_oracle_state* state, uint16_t* buffer) {

	unsigned short seq_num = ntohs(buffer[1]);

	to_measure* m = list_find_item(state->measurements, equal_seq_num, seq_num);
	if(!m) {
		ygg_log("ORACLE", "WARNING", "no measurement for the given message");
		return;
	}

	clock_gettime(CLOCK_MONOTONIC, &m->end);

	//you diff start and end, and return that
	double timeElapsed = ((double)(m->end.tv_nsec - m->start.tv_nsec))/1000000.0;
	double rtt_msec = (m->end.tv_sec- m->start.tv_sec) * 1000.0 + timeElapsed;


	deliver_replies(state, m, rtt_msec);


}

static void handle_read(udp_oracle_state* state) {

	//printf("===============>  received msg\n");

	struct sockaddr_in addr;
	int addr_len = sizeof(struct sockaddr_in);

	uint16_t* buffer = malloc(sizeof(uint16_t)*2);
	recvfrom(state->socket, (void*) buffer, sizeof(uint16_t)*2, 0, (struct sockaddr*)&addr, &addr_len);
	oracle_msg_types m_t = ntohs(buffer[0]);
	if(m_t == PING) {
		send_pong(state, buffer, &addr);
	} else {
		do_measurement(state, buffer);
	}

	free(buffer);
}

static void handle_pipe(udp_oracle_state* state) {

	queue_t_elem_type t;
	read(state->localpipe[0], &t, sizeof(queue_t_elem_type));
	if(t == YGG_REQUEST) {
		YggRequest* req = malloc(sizeof(YggRequest));
		read(state->localpipe[0], req, sizeof(YggRequest));
		if(req->request_type == MEASURE_REQUEST) {
			add_to_measure(state, req);
			//send_ping(state, m);
		} else if(req->request_type == CANCEL_MEASURE_REQUEST) {

			cancel_measurent(state, req);
			YggRequest_freePayload(req);
			free(req);
		}
	} else if( t ==YGG_TIMER) {
		YggTimer timer;
		read(state->localpipe[0], &timer, sizeof(YggTimer));
		IPAddr ip;
		void* ptr = YggTimer_readPayload(&timer, NULL, ip.addr, 16);
		YggTimer_readPayload(&timer, ptr, &ip.port, sizeof(unsigned short));
		to_measure* m = list_find_item(state->measurements, equal_peer, &ip);
		if(m) {
			send_ping(state, m);
		}

		YggTimer_freePayload(&timer);
	}


}


static void udp_oracle_worker(udp_oracle_state* state) {

	fd_set reads;

	while(true) {
		//add to fd set
		FD_ZERO(&reads);
		FD_SET(state->socket, &reads);
		FD_SET(state->localpipe[0], &reads);
		int largest_fd = (state->socket > state->localpipe[0] ? state->socket : state->localpipe[0]);

		select(largest_fd+1, &reads, NULL, NULL, NULL);

		if(FD_ISSET(state->socket, &reads) != 0) {
			handle_read(state);
		}

		if(FD_ISSET(state->localpipe[0], &reads) != 0) {
			handle_pipe(state);
		}
	}

}

static void udp_oracle_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	udp_oracle_state* state = (udp_oracle_state*)args->state;

	pthread_t worker;
	pthread_attr_t patribute;
	pthread_attr_init(&patribute);

	pthread_create(&worker, &patribute, &udp_oracle_worker, (void*) state);

	while(true) {
		queue_t_elem e;
		queue_pop(inBox, &e);
		switch(e.type) {
		case YGG_REQUEST:
			write(state->localpipe[1], &e.type, sizeof(queue_t_elem_type));
			write(state->localpipe[1], &e.data.request, sizeof(YggRequest));
			break;
		case YGG_TIMER:
			write(state->localpipe[1], &e.type, sizeof(queue_t_elem_type));
			write(state->localpipe[1], &e.data.timer, sizeof(YggTimer));
			break;
		default:
			break;
		}
	}

}

static void create_socket(udp_oracle_state* state) {
	struct sockaddr_in addr;

	if( (state->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("UDP oracle socket");
		//some error code/msg
	}

	bzero(&addr, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	inet_aton(state->self.addr, &addr.sin_addr);
	addr.sin_port = htons(state->self.port);

	if( bind(state->socket, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) < 0) {
		perror("UDP oracle bind");
		//some error code/msg
	}

}

proto_def* udp_oracle_init(void* args) {

	udp_oracle_args* a = (udp_oracle_args*) args;
	udp_oracle_state* state = malloc(sizeof(udp_oracle_state));
	state->proto_id = PROTO_UDP_ORACLE;
	getIpAddr(&state->self);

	state->current_seq_num = 0;
	state->period = a->period_s;

	state->measurements = list_init();
	pipe(state->localpipe);

	create_socket(state);


	proto_def* oracle = create_protocol_definition(state->proto_id, "UDP Oracle", state, NULL);
	proto_def_add_protocol_main_loop(oracle, udp_oracle_main_loop);


	return oracle;
}

udp_oracle_args* udp_oracle_args_init(int period_s) {
	udp_oracle_args* args = malloc(sizeof(udp_oracle_args));
	args->period_s = period_s;
	return args;
}
void udp_oracle_args_destroy(udp_oracle_args* args) {
	free(args);
}
