/*
 * plumtree.c
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#include "plumtree.h"


#define GC_TIME_S 600 //maybe this should be a parameter

typedef enum __plumtree_msg_types {
	GOSSIP,
	PRUNE,
	IHAVE,
	GRAFT,
	ANNOUNCEMENT
}plumtree_msg_types;

typedef struct __peer {
	IPAddr ip;
}peer;

static bool equal_peer_addr(peer* p, IPAddr* ip) {
	return  (strcmp(p->ip.addr, ip->addr) == 0 && p->ip.port == ip->port);
}

static bool equal_peer(peer* p1, peer* p2) {
	return (strcmp(p1->ip.addr, p2->ip.addr) == 0 && p1->ip.port == p2->ip.port);
}

static peer* create_empty_peer() {
	peer* p = malloc(sizeof(peer));
	bzero(p->ip.addr, 16);
	return p;
}

static void destroy_peer(peer* p) {
	free(p);
}


typedef struct __lazy_queue_item {
	peer* node;
	int round;
	int mid; //mid can be calculate through hash functions
}lazy_queue_item;

static bool equal_mid(lazy_queue_item* lqi, int* mid) {
	return lqi->mid == *mid;
}

static bool equal_node(lazy_queue_item* lqi, peer* p) {
	return equal_peer(lqi->node, p);
}

static lazy_queue_item* create_lazy_queue_item(peer* node, int mid, int round) {
	lazy_queue_item* i = malloc(sizeof(lazy_queue_item));
	i->node = node;
	i->mid = mid;
	i->round = round;
	return i;
}


typedef struct __mid {
	int hash;
	unsigned requestor_id; //need this if it is to be resend
	void* msg_payload;
	unsigned short msg_payload_size;
	struct timespec received;
}message_item;

static bool equal_message_item(message_item* m, int* mid){
	return m->hash == *mid;
}

static message_item* create_msg_item(int mid, unsigned short requestor_id, void* msg_payload, unsigned short msg_length) {
	message_item* m = malloc(sizeof(message_item));
	m->hash = mid;
	m->requestor_id = requestor_id;
	m->msg_payload = malloc(msg_length);
	memcpy(m->msg_payload, msg_payload, msg_length);
	m->msg_payload_size = msg_length;
	clock_gettime(CLOCK_MONOTONIC, &m->received);
	return m;
}

static void destroy_message_item(message_item* m) {
	free(m->msg_payload);
}

static bool expired(message_item* m) { //TODO better this
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	return (now.tv_sec - m->received.tv_sec > GC_TIME_S);
}

typedef struct __timer {
	int mid;
	YggTimer* t;
}timeouts;

static bool equal_timer(timeouts* t, int* mid) {
	return t->mid == *mid;
}

typedef struct __plumtree_state {
	unsigned short proto_id;

	peer* self;

	list* received; // a list of message identifiers and messages...

	list* eager_push_peers; // a list of neighbours that belong to the spanning tree
	list* lazy_push_peers; // a list of neighbours that dont belong to the spanning tree

	list* lazy_queue; // a list of tuples {mId, node, round}
	list* missing;

	list* timers; //a list of message identifiers

	int fanout;

	unsigned short timeout_s;
	long timeout_ns;
}plumtree_state;

static void print_tree(plumtree_state* state) {

    printf("PlumTree neighbors:\n");
	printf("\t eager push:\n");
	for(list_item* it = state->eager_push_peers->head; it != NULL; it=it->next) {
		peer* p = (peer*)it->data;
		printf("\t\t %s  %d\n", p->ip.addr, p->ip.port);
	}

	printf("\t lazy push:\n");
	for(list_item* it = state->lazy_push_peers->head; it != NULL; it=it->next) {
		peer* p = (peer*)it->data;
		printf("\t\t %s  %d\n", p->ip.addr, p->ip.port);
	}

}

static void cancel_timer(int mid, plumtree_state* state) {
	timeouts* t = list_remove_item(state->timers, (equal_function) equal_timer, &mid);
	if(t) {
		cancelTimer(t->t);
		YggTimer_freePayload(t->t);
		free(t->t);
		free(t);
	}
}

static void set_timer(int mid, plumtree_state* state) {
	YggTimer* t = malloc(sizeof(YggTimer));
	YggTimer_init(t, state->proto_id, state->proto_id);
	YggTimer_set(t, state->timeout_s, state->timeout_ns, 0, 0);
	YggTimer_addPayload(t, &mid, sizeof(int));

	timeouts* tms = malloc(sizeof(timeouts));
	tms->t = t;
	tms->mid = mid;
	list_add_item_to_tail(state->timers, tms);

	setupTimer(t);
}

static void deliver_msg(unsigned short proto_dest, void* msg_contents, unsigned short msg_size, IPAddr* dest_ip, IPAddr* src_ip) {

	YggMessage m;
	YggMessage_initIp(&m, proto_dest, dest_ip->addr, dest_ip->port);
	memcpy(m.header.src_addr.ip.addr, src_ip->addr, strlen(src_ip->addr));
	m.header.src_addr.ip.port = src_ip->port;
	m.data = msg_contents;
	m.dataLen = msg_size;

	deliver(&m);
}

static plumtree_msg_types decodify(uint16_t type) {
	return ntohs(type);
}

static uint16_t codify(plumtree_msg_types type) {
	return htons(type);
}

static int hash(char* to_hash, unsigned short to_hash_len) {
	return DJBHash(to_hash, to_hash_len);
}


static int generate_mid(void* msg_payload, unsigned short msg_payload_size, unsigned short requestor_id, peer* self) {
	unsigned short to_hash_len = msg_payload_size + sizeof(unsigned short)*2 + 16 + sizeof(time_t);
	char* to_hash = malloc(to_hash_len);
	memcpy(to_hash, msg_payload, msg_payload_size);
	int off = msg_payload_size;
	memcpy(to_hash+off, &requestor_id, sizeof(unsigned short));
	off += sizeof(unsigned short);
	memcpy(to_hash+off, self->ip.addr, 16);
	off += 16;
	memcpy(to_hash+off, &self->ip.port, sizeof(unsigned short));
	off += sizeof(unsigned short);
	time_t t = time(NULL);
	memcpy(to_hash+off, &t, sizeof(time_t));

	int mid = hash(to_hash, to_hash_len);

	free(to_hash);

	return mid;
}

static void init_msg_header(YggMessage* msg, plumtree_msg_types type, peer* dest, unsigned proto_id) {
	YggMessage_initIp(msg, proto_id, dest->ip.addr, dest->ip.port);
	uint16_t msg_type = codify(type);
	YggMessage_addPayload(msg, (char*) &msg_type, sizeof(uint16_t));
}

static void send_gossip_msg(peer* dest, peer* self, unsigned short requester_id, int mid, int round, void* contents, unsigned short contents_lenght, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending gossip message with mid: %d to: %s %d\n", mid, dest->ip.addr, dest->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, GOSSIP, dest, proto_id);

	uint32_t mid_t = htonl(mid);
	uint16_t round_t = htons(round);

	YggMessage_addPayload(&msg, (char*) &mid_t, sizeof(uint32_t));
	YggMessage_addPayload(&msg, (char*) &round_t, sizeof(uint16_t));

	uint16_t req_id = htons(requester_id);
	YggMessage_addPayload(&msg, (char*) &req_id, sizeof(uint16_t));

	uint16_t size = htons(contents_lenght);
	YggMessage_addPayload(&msg, (char*) &size, sizeof(uint16_t));
	YggMessage_addPayload(&msg, contents, contents_lenght); //this could be optimized for passing references (but would need to have a dispatcher that could handle this)


	dispatch(&msg);

	YggMessage_freePayload(&msg);
}

static void send_prune_msg(peer* dest, peer* self, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending prune message to: %s %d\n", dest->ip.addr, dest->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, PRUNE, dest, proto_id);

	dispatch(&msg);

	YggMessage_freePayload(&msg);
}


static void send_graft_msg(peer* node, peer* self, int mid, int round, unsigned short proto_id) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending graft message: for mid: %d to: %s %d", mid, node->ip.addr, node->ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, GRAFT, node, proto_id);

	uint32_t mid_t = htonl(mid);
	uint16_t round_t = htons(round);

	YggMessage_addPayload(&msg, (char*) &mid_t, sizeof(uint32_t));
	YggMessage_addPayload(&msg, (char*) &round_t, sizeof(uint16_t));

	dispatch(&msg);

	YggMessage_freePayload(&msg);

}

static void build_ihave_entry(YggMessage* msg, lazy_queue_item* item) {
	uint32_t mid_t = htonl(item->mid);
	uint16_t round = htons(item->round);

	YggMessage_addPayload(msg, (char*) &mid_t, sizeof(uint32_t));
	YggMessage_addPayload(msg, (char*) &round, sizeof(uint16_t));
}

static list* policy(plumtree_state* state) {
	list* l = list_init();

	while(state->lazy_queue->size > 0)
		list_add_item_to_tail(l, list_remove_head(state->lazy_queue));

	return l;
}

static void announce_ihave(list* announcements, short proto_id) {

	lazy_queue_item* i = NULL;
	while((i = list_remove_head(announcements)) != NULL) {

		YggMessage msg;
		init_msg_header(&msg, IHAVE, i->node, proto_id);
		build_ihave_entry(&msg, i);
		free(i);
		dispatch(&msg);
		YggMessage_freePayload(&msg);
	}

}

static void dispatch_announcements(plumtree_state* state) {
	list* announcements = policy(state);

	announce_ihave(announcements, state->proto_id);

	free(announcements);
}

static void eager_push(void* msg_payload, unsigned short msg_size, unsigned short requester_id, int mid, int round, peer* sender, plumtree_state* state) {
	for(list_item* it = state->eager_push_peers->head; it != NULL; it = it->next) {
		peer* p = (peer*)it->data;
		if(!sender)
			send_gossip_msg(p, state->self, requester_id, mid, round, msg_payload, msg_size, state->proto_id);
		else if(!equal_peer(p, sender))
			send_gossip_msg(p, state->self, requester_id, mid, round, msg_payload, msg_size, state->proto_id);
	}
}

static void lazy_push(void* msg_payload, unsigned short msg_size, int mid, int round, peer* sender, plumtree_state* state) {
	for(list_item* it = state->lazy_push_peers->head; it != NULL; it = it->next) {
		peer* p = (peer*)it->data;
		if(!sender) {
			lazy_queue_item* lqi = create_lazy_queue_item(p, mid, round);
			list_add_item_to_tail(state->lazy_queue, lqi);
		}else if(!equal_peer(p, sender)) {
			lazy_queue_item* lqi = create_lazy_queue_item(p, mid, round);
			list_add_item_to_tail(state->lazy_queue, lqi);
		}
	}

	dispatch_announcements(state);
}

static void process_gossip(YggMessage* msg, void* ptr, plumtree_state* state) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing gossip from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

	uint32_t mid_t;
	ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));
	int mid = ntohl(mid_t);
	message_item* m = NULL;
	if((m = list_find_item(state->received, (equal_function) equal_message_item, &mid)) == NULL) {
		uint16_t round_t;
		uint16_t req_id_t;
		uint16_t size;

		ptr = YggMessage_readPayload(msg, ptr, &round_t, sizeof(uint16_t));
		ptr = YggMessage_readPayload(msg, ptr, &req_id_t, sizeof(uint16_t));
		ptr = YggMessage_readPayload(msg, ptr, &size, sizeof(uint16_t));

		int round = ntohs(round_t);
		unsigned short req_id = ntohs(req_id_t);
		unsigned short msg_size = ntohs(size);

		deliver_msg(req_id, ptr, msg_size, &state->self->ip, &msg->header.src_addr.ip);

		m = create_msg_item(mid, req_id, ptr, msg_size);
		list_add_item_to_head(state->received, m);

		lazy_queue_item* lqi = NULL;
		bool cancel = false;
		while((lqi = list_remove_item(state->missing, (equal_function) equal_mid, &mid)) != NULL) {
			free(lqi);
			cancel = true;
		}
		if(cancel)
			cancel_timer(mid, state);

		peer* p = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
		if(p) {
			list_add_item_to_tail(state->eager_push_peers, p);
		} else
			p = list_find_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

		if(!p) {
			char warning_msg[100];
			bzero(warning_msg, 100);
			sprintf(warning_msg, "processing new message, but not know the peer %s  %d, being optimistic\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
			ygg_log("PLUMTREE", "WARNING", warning_msg);
		}
		eager_push(ptr, msg_size, req_id, mid, round+1, p, state);
		lazy_push(ptr, msg_size, mid, round+1, p, state);


		//TODO: optimization


	}else {
		peer* p = list_remove_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
		if(p)
			list_add_item_to_tail(state->lazy_push_peers, p);
		else
			p = list_find_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

		if(p)
			send_prune_msg(p, state->self, state->proto_id);
		else {
			char warning_msg[100];
			bzero(warning_msg, 100);
			sprintf(warning_msg, "I do not know node %s  %d to send prune message, ignoring\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
			ygg_log("PLUMTREE", "WARNING", warning_msg);
		}

	}

	YggMessage_freePayload(msg);

}

static void process_prune(YggMessage* msg, void* ptr, plumtree_state* state) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing prune from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

	peer* p = list_remove_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(p)
		list_add_item_to_tail(state->lazy_push_peers, p);

	YggMessage_freePayload(msg);
}

static void process_ihave(int mid, int round, peer* sender, plumtree_state* state) {

	if(!list_find_item(state->received, (equal_function) equal_message_item, &mid)) {
		if(!list_find_item(state->timers, (equal_function) equal_timer, &mid)) {
			set_timer(mid, state);
		}
		lazy_queue_item* lqi = create_lazy_queue_item(sender, mid, round);
		list_add_item_to_tail(state->missing, lqi);
	}
}

static void process_ihave_msg(YggMessage* msg, void* ptr, plumtree_state* state) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing ihave from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif

	uint32_t mid_t;
	uint16_t round_t;

	ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));
	ptr = YggMessage_readPayload(msg, ptr, &round_t, sizeof(uint16_t));

	int mid = ntohl(mid_t);
	unsigned short round = ntohs(round_t);

	peer* p = list_find_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(!p)
		p = list_find_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	if(!p) {
		char warning_msg[100];
		bzero(warning_msg, 100);
		sprintf(warning_msg, "I do not know this peer %s  %d  on receive Ihave msg, ignoring\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
		ygg_log("PLUMTREE", "WARNING", warning_msg);
	}
	else
		process_ihave(mid, round, p, state);
	YggMessage_freePayload(msg);

}

static void process_announcement(YggMessage* msg, void* ptr, plumtree_state* state) {
	//do nothing for now.
	YggMessage_freePayload(msg);
}

static void process_graft(YggMessage* msg, void* ptr, plumtree_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing graft from %s %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif
	uint32_t mid_t;
	uint16_t round_t;

	ptr = YggMessage_readPayload(msg, ptr, &mid_t, sizeof(uint32_t));
	ptr = YggMessage_readPayload(msg, ptr, &round_t, sizeof(uint16_t));

	int mid = ntohl(mid_t);
	unsigned short round = ntohs(round_t);

	peer* p = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(p) {
		list_add_item_to_tail(state->eager_push_peers, p);
	} else
		p = list_find_item(state->eager_push_peers, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	message_item* m = NULL;
	if(!p) {
		char warning_msg[100];
		bzero(warning_msg, 100);
		sprintf(warning_msg, "I do not known graft message originator  %s %d, ignoring", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
		ygg_log("PLUMTREE", "WARNING", warning_msg);
	}
	else if((m = list_find_item(state->received, (equal_function) equal_message_item, &mid)) != NULL) {
		send_gossip_msg(p, state->self, m->requestor_id, mid, round, m->msg_payload, m->msg_payload_size, state->proto_id);
	}

	YggMessage_freePayload(msg);
}

static void process_msg(YggMessage* msg, plumtree_state* state) {

	uint16_t type;
	void* ptr = YggMessage_readPayload(msg, NULL, &type, sizeof(uint16_t));
	plumtree_msg_types msg_type = decodify(type);

	switch(msg_type) {
	case GOSSIP:
		process_gossip(msg, ptr, state);
		break;
	case PRUNE:
		process_prune(msg, ptr, state);
		break;
	case IHAVE:
		process_ihave_msg(msg, ptr, state);
		break;
	case GRAFT:
		process_graft(msg, ptr, state);
		break;
	case ANNOUNCEMENT: //these message are not yet processed
		process_announcement(msg, ptr, state);
		break;
	}

}

static void garbage_collect(plumtree_state* state) {

#ifdef DEBUG
	print_tree(state);
#endif

	list_item* it = state->received->head;
	list_item* prev = NULL;
	while(it != NULL) {
		bool destroy = false;
		message_item* m = (message_item*)it->data;

		if(expired(m)) {
			destroy = true;
		}else {
			prev = it;
		}
		it = it->next;
		if(destroy) {
			if(prev) {
				m = list_remove(state->received, prev);
			} else
				m = list_remove_head(state->received);

			destroy_message_item(m);
			free(m);
		}

	}

}

static void process_timer(YggTimer* timer, plumtree_state* state) {

    if(timer->timer_type == 2)
        print_tree(state);
	else if(timer->timer_type == 1)
		garbage_collect(state);
	else {

		int mid;
		YggTimer_readPayload(timer, NULL, &mid, sizeof(int));
#ifdef DEBUG
		char debug_msg[100];
		bzero(debug_msg, 100);
		sprintf(debug_msg, "received timeout for mid: %d", mid);
		ygg_log("PLUMTREE", "DEBUG", debug_msg);
#endif
		lazy_queue_item* lqi = list_remove_item(state->missing, (equal_function) equal_mid, &mid);
		if(!lqi) {
			char warning_msg[100];
			bzero(warning_msg, 100);
			sprintf(warning_msg, "no item missing for mid: %d", mid);
			ygg_log("PLUMTREE", "WARNING", warning_msg);
			YggTimer_freePayload(timer);
			return;
		}
		peer* p = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer, lqi->node);
		if(p)
			list_add_item_to_tail(state->eager_push_peers, p);
		else
			p = list_find_item(state->eager_push_peers, (equal_function) equal_peer, lqi->node);
		if(p)
			send_graft_msg(p, state->self, mid, lqi->round, state->proto_id);
		else {
			char warning_msg[100];
			bzero(warning_msg, 100);
			sprintf(warning_msg, "no peer to send graft missing msg mid: %d", mid);
			ygg_log("PLUMTREE", "WARNING", warning_msg);
		}
		if(list_find_item(state->timers, (equal_function) equal_timer, &mid)) {
			YggTimer_set(timer, state->timeout_s-1, state->timeout_ns, 0, 0); //TODO: better set timeout 2: to an average roundtrip time
			setupTimer(timer);
		}
		YggTimer_freePayload(timer);
	}

}

static void process_neighbour_up(YggEvent* ev, plumtree_state* state) {
	peer* p = create_empty_peer();
	void* ptr = YggEvent_readPayload(ev, NULL, p->ip.addr, 16);
	YggEvent_readPayload(ev, ptr, &p->ip.port, sizeof(unsigned short));

	list_add_item_to_tail(state->eager_push_peers, p);
}

static void process_neighbour_down(YggEvent* ev, plumtree_state* state) {
	IPAddr ip;
	void* ptr = YggEvent_readPayload(ev, NULL, ip.addr, 16);
	YggEvent_readPayload(ev, ptr, &ip.port, sizeof(unsigned short));

	peer* p = NULL;
	p = list_remove_item(state->eager_push_peers, (equal_function) equal_peer_addr, &ip);

	peer* c = list_remove_item(state->lazy_push_peers, (equal_function) equal_peer_addr, &ip);
	if(c)
		p = c;

	while(list_remove_item(state->missing, (equal_function) equal_node, p));


	while(list_remove_item(state->lazy_queue, (equal_function) equal_node, p));

	destroy_peer(p);

}

static void process_event(YggEvent* ev, plumtree_state* state) {
	switch(ev->notification_id) {
	case OVERLAY_NEIGHBOUR_UP:
		process_neighbour_up(ev, state);
		break;
	case OVERLAY_NEIGHBOUR_DOWN:
		process_neighbour_down(ev, state);
		break;
	}

	YggEvent_freePayload(ev);
}

static void broadcast(YggRequest* req, plumtree_state* state) {

	int mid = generate_mid(req->payload, req->length, req->proto_origin, state->self);

	eager_push(req->payload, req->length, req->proto_origin, mid, 0, state->self, state);
	lazy_push(req->payload, req->length, mid, 0, state->self, state);

	//this is to maintain broadcast semantics
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg,"broadcasting msg: mid: %d round: %d", mid, 0);
	ygg_log("PLUMTREE", "DEBUG", debug_msg);


#endif
	deliver_msg(req->proto_origin, req->payload, req->length, &state->self->ip, &state->self->ip);

	message_item* m = create_msg_item(mid, req->proto_origin, req->payload, req->length);

	list_add_item_to_tail(state->received, m);

	YggRequest_freePayload(req);
}

static void process_request(YggRequest* req, plumtree_state* state) {

	if(req->request == REQUEST && req->request_type == PLUMTREE_BROADCAST_REQUEST) {
		broadcast(req, state);
	}
}

static void plumtree_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	plumtree_state* state = args->state;

	while(true) {
		queue_t_elem el;
		queue_pop(inBox, &el);

		switch(el.type) {
		case YGG_MESSAGE:
			process_msg(&el.data.msg, state);
			break;
		case YGG_TIMER:
			process_timer(&el.data.timer, state);
			break;
		case YGG_EVENT:
			process_event(&el.data.event, state);
			break;
		case YGG_REQUEST:
			process_request(&el.data.request, state);
			break;
		}
	}

}


proto_def* plumtree_init(void* args) {

	plumtree_args* a = (plumtree_args*)args;

	plumtree_state* state = malloc(sizeof(plumtree_state));

	state->received = list_init();
	state->eager_push_peers = list_init();
	state->lazy_push_peers = list_init();
	state->missing = list_init();
	state->lazy_queue = list_init();

	state->timers = list_init();

	state->timeout_s = a->timeout_s;
	state->timeout_ns = a->timeout_ns;
	state->fanout = a->fanout;

	state->self = create_empty_peer();
	getIpAddr(&state->self->ip);

	state->proto_id = PROTO_PLUMTREE;


	proto_def* plumtree = create_protocol_definition(PROTO_PLUMTREE, "PlumTree", state, NULL);
	proto_def_add_consumed_event(plumtree, a->membership_id, OVERLAY_NEIGHBOUR_UP);
	proto_def_add_consumed_event(plumtree, a->membership_id, OVERLAY_NEIGHBOUR_DOWN);
	proto_def_add_protocol_main_loop(plumtree, (Proto_main_loop) plumtree_main_loop);

	YggTimer gc;
	YggTimer_init(&gc, PROTO_PLUMTREE, PROTO_PLUMTREE);
	YggTimer_set(&gc, GC_TIME_S, 0, GC_TIME_S, 0);
	YggTimer_setType(&gc, 1); //lets say 1 == garbage collect

	setupTimer(&gc);

#ifdef DEBUG
    YggTimer debug;
    YggTimer_init(&debug, PROTO_PLUMTREE, PROTO_PLUMTREE);
    YggTimer_set(&debug, 5, 0, 5, 0);
    YggTimer_setType(&debug, 2); //lets say 2 == debug (print tables)

    setupTimer(&debug);
#endif
	return plumtree;
}

plumtree_args* plumtree_args_init(int fanout, unsigned short timeout_s, long timeout_ns, unsigned short membership_id) {

	plumtree_args* args = malloc(sizeof(plumtree_args));
	args->fanout = fanout; //this is not used
	args->timeout_s = timeout_s;
	args->timeout_ns = timeout_ns;
	args->membership_id = membership_id;


	return args;
}

void plumtree_args_destroy(plumtree_args* args) {
	free(args);
}
