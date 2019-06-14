/*
 * hyparview.c
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#include "hyparview.h"

#define MAX_SEQ_NUM 6666

#define MAX_BACKOFF_S 3600 //one hour

typedef struct _peer {
	IPAddr ip;
}peer;

typedef struct _shuffle_request {
	unsigned short seq_num;
	list* ka;
	list* kp;
}shuffle_request;


typedef struct __hyparview_state {

	short proto_id;


	int max_active; //param: maximum active nodes (degree of random overlay)
	int max_passive; //param: maximum passive nodes
	short ARWL; //param: active random walk length
	short PRWL; //param: passive random walk length

	short k_active; //param: number of active nodes to exchange on shuffle
	short k_passive; //param: number of passive nodes to exchange on shuffle

	list* active_view;
	list* passive_view;

	list* pending;

	unsigned short last_shuffle;
	list* active_shuffles;
	unsigned short shuffle_timeout;

	peer* self;


	YggTimer* shuffle;


	unsigned short original_backoff_s;
	long original_backoff_ns;

	unsigned short backoff_s;
	long backoff_ns;
}hyparview_state;

typedef enum __hyparview_msg_type {
	JOIN,
	JOINREPLY,
	FORWARDJOIN,
	DISCONNECT,
	SHUFFLE,
	SHUFFLEREPLY,
	HELLONEIGH,
	HELLONEIGHREPLY
}hyparview_msg_type;

static bool is_active_full(hyparview_state* state) {
	return (state->active_view->size + state->pending->size >= state->max_active);
}

static bool is_passive_full(hyparview_state* state) {
	return state->passive_view->size >= state->max_passive;
}

static void print_views(list* active, list* passive, peer* self) {

    printf("HyParView views: %s  %d\n", self->ip.addr, self->ip.port);
	printf("\t active\n");
	for(list_item* it = active->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;

		printf("\t\t %s  %d\n", p->ip.addr, p->ip.port);

	}
	printf("\t passive\n");
	for(list_item* it = passive->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;
		printf("\t\t %s  %d\n", p->ip.addr, p->ip.port);
	}

}

static void printf_shuffle_request(shuffle_request* sr, bool refs) {

    /*
	printf("shuffle number: %d\n", sr->seq_num);

	printf("\t active\n");
	for(list_item* it = sr->ka->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;

		printf("\t\t %s  %d", p->ip.addr, p->ip.port);

		if(refs)
			printf(" : ref  %d\n", p);
		else
			printf("\n");
	}


	printf("\t passive\n");
	for(list_item* it = sr->kp->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;
		printf("\t\t %s  %d", p->ip.addr, p->ip.port);
		if(refs)
			printf(" : ref  %d\n", p);
		else
			printf("\n");
	}
    */
}

static void send_notification(hyparview_events notification_id, short proto_id, peer* p) {
	YggEvent ev;
	YggEvent_init(&ev, proto_id, notification_id);
	YggEvent_addPayload(&ev, p->ip.addr, 16); //INET_ADDRLEN
	YggEvent_addPayload(&ev, &p->ip.port, sizeof(unsigned short));

	deliverEvent(&ev);
	YggEvent_freePayload(&ev);
}


static void destroy_peer(peer* p);

static void send_disconnect(peer* dest, peer* self, short proto_id);

static bool equal_shuffle_request(shuffle_request* sr, unsigned short seq_num) {
	return sr->seq_num == seq_num;
}

static bool equal_peer(peer* p1, peer* p2) {
	return (strcmp(p1->ip.addr, p2->ip.addr) == 0 && p1->ip.port == p2->ip.port);
}

static bool equal_peer_addr(peer* p1, IPAddr* ip) {
	return (strcmp(p1->ip.addr, ip->addr) == 0 && p1->ip.port == ip->port);
}

static unsigned short next_shuffle_num(unsigned short current_seq_num) {
	return (current_seq_num % MAX_SEQ_NUM) +1;
}

static bool shuffle_request_timeout(unsigned short old, shuffle_request* new, unsigned short shuffle_timeout) {
	unsigned short jumps = 0;
	while((old = next_shuffle_num(old)) != new->seq_num)
		jumps ++;

	return jumps > shuffle_timeout;
}

static void destroy_shuffle_request(shuffle_request* sr) {
	while(sr->ka->size > 0) {
		peer* p = list_remove_head(sr->ka);
		destroy_peer(p);
	}

	free(sr->ka);
	sr->ka = NULL;

	while(sr->kp->size > 0) {
		peer* p = list_remove_head(sr->kp);
		destroy_peer(p);
	}

	free(sr->kp);
	sr->kp = NULL;

	free(sr);
}

static uint16_t codify(hyparview_msg_type msg_type) {
	return htons(msg_type);
}

static hyparview_msg_type decodify(uint16_t code) {
	return ntohs(code);
}


static peer* get_random_peer(list* view) {
	if(view->size > 0) {
		int i = rand()%view->size;
		list_item* it = NULL;
		for(it = view->head; i > 0 && it != NULL; it = it->next, i--);
		if(it)
			return (peer*) it->data;
	}
	return NULL;
}


static peer* get_random_peer_different_addr(list* view, IPAddr* addr) {
	if(view->size > 0) {
		int i = rand()%view->size;
		list_item* it = NULL;
		list_item* prev = NULL;
		for(it = view->head; i > 0 && it != NULL; prev = it, it = it->next, i--);
		if(it) {
			peer* p = (peer*) it->data;
			if(equal_peer_addr(p, addr)) {
				if(it->next) {
					p = (peer*) it->next->data;
				}else {
					if(prev) {
						p = (peer*) prev->data;
					} else
						p = NULL; //no other possible peer
				}
			}
			return p;
		}
	}
	return NULL;
}

static peer* drop_random_peer(list* view) {
	if(view->size > 0) {
		int i = rand()%view->size;
		list_item* it = NULL;
		list_item* prev = NULL;

		for(it = view->head; i > 0 && it != NULL; prev = it, it = it->next, i--);

		if(it) {
			if(prev)
				return (peer*) list_remove(view, prev);
			else
				return (peer*) list_remove_head(view);
		}
	}
	return NULL;
}

static peer* create_peer(const char* ip_addr, unsigned short port) {
	peer* p = malloc(sizeof(peer));
	bzero(p->ip.addr, 16); //just to be safe
	memcpy(p->ip.addr, ip_addr, strlen(ip_addr));
	p->ip.port = port;
	return p;
}

static void destroy_peer(peer* p) {
	free(p);
}

static list* get_random_subset(list* view, short k) {

	list* subset = list_init();

	if(view->size > k) {
		int has[k];
		short curr = 0;

		while(curr < k) {
			int i = rand()%view->size;
			bool ok = true;
			for(int j = 0; j < curr; j ++) {
				if(has[j] == i) {
					ok = false;
					break;
				}

			}

			if(ok) {
				list_item* it = NULL;
				int j = i;
				for(it = view->head; j > 0 && it != NULL; it = it->next, j--);

				peer* p = create_peer(((peer*) it->data)->ip.addr, ((peer*) it->data)->ip.port);
				list_add_item_to_head(subset, p);
				has[curr] = i;
				curr ++;
			}
		}
	} else {
		for(list_item* it = view->head; it != NULL; it = it->next) {
			peer* p = create_peer(((peer*) it->data)->ip.addr, ((peer*) it->data)->ip.port);
			list_add_item_to_tail(subset, p);
		}
	}

	return subset;
}


static void send_close_request(IPAddr* ip, hyparview_state* state) {

	YggRequest r;
	YggRequest_init(&r, state->proto_id, PROTO_DISPATCH, REQUEST, CLOSE_CONNECTION);
	YggRequest_addPayload(&r, ip->addr, 16);
	YggRequest_addPayload(&r, &ip->port, sizeof(unsigned short));

	deliverRequest(&r);
	YggRequest_freePayload(&r);
}

//invariant p != self
static bool add_to_passive_if_not_exists(list* active_view, list* passive_view, peer* p, list_item** next_to_drop, peer* self, int max_passive) {

	if(equal_peer(p, self))
		return false;

	peer* a = list_find_item(active_view, (equal_function) equal_peer, p);

	if(a) //p already exists in active view
		return false;

	a = list_find_item(passive_view, (equal_function) equal_peer, p);
	if(a)
		return false;

	if(passive_view->size >= max_passive) { //just in case
		peer* n = NULL;
		if(!next_to_drop)
			n = drop_random_peer(passive_view);
		else {
			while(!n && *next_to_drop != NULL) {
				n = list_remove_item(passive_view, (equal_function) equal_peer, (*next_to_drop)->data);
				*next_to_drop = (*next_to_drop)->next;
			}

			if(!n) {
				n = drop_random_peer(passive_view);
			}

		}

		if(n) {
			destroy_peer(n);
		}
	}

	list_add_item_to_tail(passive_view, p);

	return true;
}

static void drop_random_from_active_view(hyparview_state* state, list* active_view, list* pending, list* passive_view, peer* self, int max_passive, short proto_id) {

	peer* p;
	if(pending->size > 0) {
		p = drop_random_peer(pending);
	}
	else {
		p = drop_random_peer(active_view);
		if(p) {
			send_disconnect(p, self,proto_id);
			send_notification(OVERLAY_NEIGHBOUR_DOWN, proto_id, p);
			//send_close_request(&p->ip, state);
		}
	}

	if(!p) {
		ygg_log("HYPARVIEW", "WARNING",  "no peer got dropped from active view");
		return;
	}

	bool ok = add_to_passive_if_not_exists(active_view, passive_view, p, NULL, self, max_passive);
	if(!ok)
		destroy_peer(p);

}

static bool add_to_pending_if_not_exists(list* pending, list* active_view, list* passive_view, peer* p, peer* self) {
	if(equal_peer(p, self))
		return false;

	peer* a = list_find_item(pending, (equal_function) equal_peer, p);
	if(a)
		return false;
	a = list_find_item(active_view, (equal_function) equal_peer, p);
	if(a)
		return false;

	a = list_remove_item(passive_view, (equal_function) equal_peer, p);
	if(a)
		destroy_peer(a);

	list_add_item_to_tail(pending, p);
	return true;
}

static bool add_to_active_if_not_exists(hyparview_state* state, list* active_view, list* pending, list* passive_view, peer* p, peer* self, int max_active, int max_passive, short proto_id) {
	if(equal_peer(p, self))
		return false;

	peer* a = list_find_item(active_view, (equal_function) equal_peer, p);
	if(a)
		return false;


	if(active_view->size + pending->size >= max_active)
		drop_random_from_active_view(state, active_view, pending, passive_view, self, max_passive, proto_id);

	a = list_find_item(passive_view, (equal_function) equal_peer, p);
	if(a) {
		peer* c = list_remove_item(passive_view, (equal_function) equal_peer, a);
		if(a != c)
			ygg_log("HYPARVIEW", "WARNING", "did not remove the one that should have been removed (add_to_active), corrupted list ?");
		destroy_peer(c);
	}

	list_add_item_to_tail(active_view, p);

	return true;

}

static void add_peer_list_to_msg(YggMessage* msg, list* peers) {

	uint16_t size = htons(peers->size);
	YggMessage_addPayload(msg, &size, sizeof(uint16_t));

	for(list_item* it = peers->head; it != NULL; it = it->next) {
		struct in_addr addr;
		if(!inet_aton(((peer*)it->data)->ip.addr, &addr))
			printf("Error on inet_aton (add peer list to msg) %s\n", ((peer*)it->data)->ip.addr);
		YggMessage_addPayload(msg, &addr, sizeof(struct in_addr));
		uint16_t port = htons(((peer*)it->data)->ip.port);
		YggMessage_addPayload(msg, &port, sizeof(uint16_t));
	}
}

static void init_msg_header(YggMessage* msg, hyparview_msg_type type, short proto_id, peer* dest, peer* self) {
	YggMessage_initIp(msg, proto_id, dest->ip.addr, dest->ip.port);
	uint16_t code = codify(type);
	YggMessage_addPayload(msg, &code, sizeof(uint16_t));
}

static void send_shuffle_reply(peer* dest, peer* self, unsigned short seq_num, list* k_p, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending shuffle reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	YggMessage msg;
	init_msg_header(&msg, SHUFFLEREPLY, proto_id, dest, self);

	uint16_t seq = htons(seq_num);
	YggMessage_addPayload(&msg, &seq, sizeof(uint16_t));

	add_peer_list_to_msg(&msg, k_p);

	dispatch(&msg);
	YggMessage_freePayload(&msg);

}

//new time to leave must have been evaluated and changed accordingly before call
static void forward_shuffle(YggMessage* shuffle_msg, peer* dest) {

#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "forwarding shuffle to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	memcpy(shuffle_msg->header.dst_addr.ip.addr, dest->ip.addr, 16);
	shuffle_msg->header.dst_addr.ip.port = dest->ip.port;
	dispatch(shuffle_msg);
	YggMessage_freePayload(shuffle_msg);
}

//dest is random; time_to_live == PRWL
static void send_shuffle(peer* dest, peer* self, shuffle_request* sr, short time_to_live, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending shuffle to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, SHUFFLE, proto_id, dest, self);

	uint16_t ttl = htons(time_to_live);
	YggMessage_addPayload(&msg, &ttl, sizeof(uint16_t));

	struct in_addr addr;
	inet_aton(self->ip.addr, &addr);

	YggMessage_addPayload(&msg, &addr, sizeof(struct in_addr));
	uint16_t port = htons(self->ip.port);
	YggMessage_addPayload(&msg, &port, sizeof(uint16_t));

	uint16_t seq = htons(sr->seq_num);
	YggMessage_addPayload(&msg, &seq, sizeof(uint16_t));

	add_peer_list_to_msg(&msg, sr->ka);
	add_peer_list_to_msg(&msg, sr->kp);

	dispatch(&msg);
	YggMessage_freePayload(&msg);


}

static void send_hello_neighbour_reply(peer* dest, peer* self, bool reply, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending hello reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	YggMessage msg;
	init_msg_header(&msg, HELLONEIGHREPLY, proto_id, dest, self);

	uint16_t rep = htons(reply);
	YggMessage_addPayload(&msg, &rep, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

//prio == true -> high; prio == false --> low
static void send_hello_neighbour(peer* dest, peer* self, bool prio, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending hello to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	YggMessage msg;
	init_msg_header(&msg, HELLONEIGH, proto_id, dest, self);

	uint16_t pr = htons(prio); //booleans are just numbers...

	YggMessage_addPayload(&msg, &pr, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);

}

static void send_disconnect(peer* dest, peer* self, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending disconnect to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, DISCONNECT, proto_id, dest, self);

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void send_forward_join(peer* dest, peer* newNode, short time_to_live, peer* self, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending forward join to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, FORWARDJOIN, proto_id, dest, self);

	uint16_t ttl = htons(time_to_live);
	YggMessage_addPayload(&msg, &ttl, sizeof(uint16_t));
	struct in_addr addr;
	if(!inet_aton(newNode->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (forward join) %s\n", newNode->ip.addr);

	YggMessage_addPayload(&msg, &addr, sizeof(struct in_addr));
	uint16_t port = htons(newNode->ip.port);
	YggMessage_addPayload(&msg, &port, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void send_joinreply(peer* dest, peer* self, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending join reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	YggMessage msg;
	init_msg_header(&msg, JOINREPLY, proto_id, dest, self);
	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void send_join(peer* dest, peer* self, short proto_id) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending join to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, JOIN, proto_id, dest, self);
	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void process_join(YggMessage* msg, void* ptr, hyparview_state* state) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing join of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	peer* p = list_find_item(state->active_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	if(p)
		return; //report error?

	p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	if(is_active_full(state))
		drop_random_from_active_view(state, state->active_view, state->pending, state->passive_view, state->self, state->max_passive, state->proto_id);

	if(!p)
		p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

	list_add_item_to_head(state->active_view, p);

	send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);

	//skip new node
	list_item* it = state->active_view->head->next;
	while(it != NULL) {
		send_forward_join(it->data, p, state->ARWL, state->self, state->proto_id);
		it = it->next;
	}

	send_joinreply(p, state->self, state->proto_id);
}

static void process_joinreply(YggMessage* msg, void* ptr, hyparview_state* state) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing join reply of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	//remove from pending, add to active, send notification
	peer* p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(p) {
		bool ok = add_to_active_if_not_exists(state, state->active_view, state->pending, state->passive_view, p, state->self, state->max_active, state->max_passive, state->proto_id);
		if(!ok) {
			destroy_peer(p);
			return;
		}

		send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);
	} else {
		send_disconnect(p, state->self, state->proto_id);
	}
}

static void process_forward_join(YggMessage* msg, void* ptr, hyparview_state* state) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing forward join of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	uint16_t ttl;
	struct in_addr addr;
	uint16_t port;

	ptr = YggMessage_readPayload(msg, ptr, &ttl, sizeof(uint16_t));
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));

	short time_to_live = ntohs(ttl);
	char* ip_addr = inet_ntoa(addr);
	unsigned short port_number = ntohs(port);

	//invariant: state->active_view->size >= 1 (should be, if correct)
	if(time_to_live == 0 || state->active_view->size + state->pending->size <= 1) {
		peer* p = create_peer(ip_addr, port_number);
		bool ok = add_to_pending_if_not_exists(state->pending, state->active_view, state->passive_view, p, state->self);

		if(!ok) {
			destroy_peer(p);
		} else
			send_hello_neighbour(p, state->self, state->active_view->size + state->pending->size == 1, state->proto_id);
	} else {
		if(time_to_live == state->PRWL) {
			peer* p = create_peer(ip_addr, port_number);
			bool ok = add_to_passive_if_not_exists(state->active_view, state->passive_view, p, NULL, state->self, state->max_passive);
			if(!ok)
				destroy_peer(p);
		}
		peer* new = create_peer(ip_addr, port_number);
		peer* p = get_random_peer_different_addr(state->active_view, &new->ip);
		if(p) {
			send_forward_join(p, new, time_to_live-1, state->self, state->proto_id);
		} else
			ygg_log("HYPARVIEW", "WARNING", "no destination to send forward join");

		destroy_peer(new);
	}

}

static void process_disconnect(YggMessage* msg, void* ptr, hyparview_state* state) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing disconnect of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	peer* p = list_remove_item(state->active_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	if(p) {
		send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, p);

		send_close_request(&p->ip, state);

		if(!add_to_passive_if_not_exists(state->active_view, state->passive_view, p, NULL, state->self, state->max_passive)) {
			destroy_peer(p);
		}

		if(state->active_view->size == 0) {
			state->backoff_s = state->original_backoff_s;
			state->backoff_ns = state->original_backoff_ns;
		}

		if(!is_active_full(state)) {
			YggTimer t;
			YggTimer_init(&t, state->proto_id, state->proto_id);
			YggTimer_set(&t, state->backoff_s, state->backoff_ns, 0, 0);

			setupTimer(&t);
		}
	} else {
        char warning_msg[100];
        bzero(warning_msg, 100);
        sprintf(warning_msg, "processing disconnect of non-active neighbor %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
        ygg_log("HYPARVIEW", "WARNING", warning_msg);
	}

}

static list* get_list_from_msg(YggMessage* msg, void** ptr) {

	list* l = list_init();
	uint16_t size;
	*ptr = YggMessage_readPayload(msg, *ptr, &size, sizeof(uint16_t));

	for(int i = ntohs(size); i > 0; i--) {
		struct in_addr addr;
		uint16_t port;
		*ptr = YggMessage_readPayload(msg, *ptr, &addr, sizeof(struct in_addr));
		*ptr = YggMessage_readPayload(msg, *ptr, &port, sizeof(uint16_t));
		peer* p = create_peer(inet_ntoa(addr), ntohs(port));
		list_add_item_to_tail(l, p);
	}

	return l;
}

static shuffle_request* create_shuffle_request_from_msg(YggMessage* msg, void* ptr) {
	shuffle_request* sr = malloc(sizeof(shuffle_request));

	uint16_t seq;
	ptr = YggMessage_readPayload(msg, ptr, &seq, sizeof(uint16_t));
	sr->seq_num = ntohs(seq);

	sr->ka = get_list_from_msg(msg, &ptr);
	sr->kp = get_list_from_msg(msg, &ptr);

	return sr;
}

static bool make_space(hyparview_state* state, peer* p, list_item** next_to_drop, int req_space) {


	bool remove_peer = false;

	if(list_find_item(state->active_view, (equal_function) equal_peer, p))
		remove_peer = true;
	if(!remove_peer && list_find_item(state->pending, (equal_function) equal_peer, p))
		remove_peer = true;
	if(!remove_peer && !list_find_item(state->passive_view, (equal_function) equal_peer, p)){
		if(state->passive_view->size + req_space > state->max_passive) {
			peer* n = NULL;

			if(!next_to_drop)
				n = drop_random_peer(state->passive_view);
			else {
				while(!n && *next_to_drop != NULL) {
					n = list_remove_item(state->passive_view, (equal_function) equal_peer, (*next_to_drop)->data);
					*next_to_drop = (*next_to_drop)->next;
				}

				if(!n) {
					n = drop_random_peer(state->passive_view);
				}

				if(n)
					destroy_peer(n);
			}
		}

	} else
		remove_peer = true;

	return remove_peer;
}

static bool apply_shuffle_request(peer* shuffler, shuffle_request* sr, list* passive_subset, hyparview_state* state) {

	list_item* next_to_drop = passive_subset->head;
	list_item* it = sr->ka->head;
	list_item* prev = NULL;
	while(it){
		bool remove_peer = false;
		if(next_to_drop)
			remove_peer = make_space(state, (peer*)it->data, &next_to_drop, sr->ka->size);
		else
			remove_peer = make_space(state, (peer*)it->data, NULL, sr->ka->size);

		if(remove_peer) {
			it = it->next;
			peer* p;
			if(prev) {
				p = list_remove(sr->ka, prev);
			}else {
				p = list_remove_head(sr->ka);
			}
			destroy_peer(p);
		} else {
			prev = it;
			it = it->next;
		}
	}

	it = sr->kp->head;
	prev = NULL;

	while(it){
		bool remove_peer = false;
		if(next_to_drop)
			remove_peer = make_space(state, (peer*)it->data, &next_to_drop, sr->kp->size);
		else
			remove_peer = make_space(state, (peer*)it->data, NULL, sr->kp->size);

		if(remove_peer) {
			it = it->next;
			peer* p;
			if(prev) {
				p = list_remove(sr->kp, prev);
			}else {
				p = list_remove_head(sr->kp);
			}
			destroy_peer(p);
		} else {
			prev = it;
			it = it->next;
		}
	}

	bool remove_shuffler = false;
	if(next_to_drop)
		remove_shuffler = make_space(state, shuffler, &next_to_drop, 1);
	else
		remove_shuffler = make_space(state, shuffler, NULL, 1);


	while(sr->ka->size > 0) {
		peer* p = list_remove_head(sr->ka);
		add_to_passive_if_not_exists(state->active_view, state->passive_view, p, NULL, state->self, state->max_passive);
	}

	while(sr->kp->size > 0) {
		peer* p = list_remove_head(sr->kp);
		add_to_passive_if_not_exists(state->active_view, state->passive_view, p, NULL, state->self, state->max_passive);
	}

	if(!remove_shuffler)
		add_to_passive_if_not_exists(state->active_view, state->passive_view, shuffler, NULL, state->self, state->max_passive);

	return !remove_shuffler;
}

static void destroy_passive_subset(list* passive_subset, list* passive_view) {
	while(passive_subset->size > 0) {
		peer* p = list_remove_head(passive_subset);
		destroy_peer(p);
	}
	free(passive_subset);

}

static void process_shuffle(YggMessage* msg, void* ptr, hyparview_state* state) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing shuffle of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	uint16_t* ttl_ptr = ptr;
	uint16_t ttl;
	ptr = YggMessage_readPayload(msg, ptr, &ttl, sizeof(uint16_t));

	short time_to_live = ntohs(ttl);
	time_to_live --;

	struct in_addr addr;
	uint16_t port;
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));

	char* ip_addr = inet_ntoa(addr);
	IPAddr ip;
	memcpy(ip.addr, ip_addr, strlen(ip_addr));
	ip.port = ntohs(port);
	if(time_to_live > 0 && state->active_view->size > 1) {
		peer* p = get_random_peer_different_addr(state->active_view, &msg->header.src_addr.ip);
		//if p == originator && active_view->size == 2 then shuffle could be applied and if no other possibilities
		*ttl_ptr = htons(time_to_live);
		forward_shuffle(msg, p);

	} else if(!equal_peer_addr(state->self, &ip)){

		peer* temp_p = create_peer(ip_addr, ntohs(port));

		shuffle_request* sr = create_shuffle_request_from_msg(msg, ptr);

#if defined DEBUG || defined DEBUG_HYPERVIEW
		printf_shuffle_request(sr, false);
#endif

		list* passive_subset = get_random_subset(state->passive_view, sr->ka->size+sr->kp->size);

		bool ok = apply_shuffle_request(temp_p, sr, passive_subset, state);

		send_shuffle_reply(temp_p, state->self, sr->seq_num, passive_subset, state->proto_id);

		if(!ok)
			destroy_peer(temp_p);

		destroy_shuffle_request(sr);
		destroy_passive_subset(passive_subset, state->passive_view);
	}

}

static void process_shufflereply(YggMessage* msg, void* ptr, hyparview_state* state) {
#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing shuffle reply of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif
	uint16_t seq;
	ptr = YggMessage_readPayload(msg, ptr, &seq, sizeof(uint16_t));

	shuffle_request* sr = list_remove_item(state->active_shuffles, (equal_function) equal_shuffle_request, ntohs(seq));

	if(sr) {

		list* ps = get_list_from_msg(msg, &ptr);
		list* tmp = sr->ka;
		tmp->tail->next = sr->kp->head;
		int original_ka_size = sr->ka->size;
		tmp->size += sr->kp->size;

		list_item* next_to_drop = tmp->head;

		peer* p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

		bool remove_peer;
		if(next_to_drop)
			remove_peer = make_space(state, p, &next_to_drop, 1);
		else
			remove_peer = make_space(state, p, NULL, 1);
		if(remove_peer) {
			destroy_peer(p);
		}

		list_item* it = ps->head;
		list_item* prev = NULL;
		while(it) {
			bool remove_peer = false;
			if(next_to_drop)
				remove_peer = make_space(state, (peer*)it->data, &next_to_drop, ps->size);
			else
				remove_peer = make_space(state, (peer*)it->data, NULL, ps->size);

			if(remove_peer) {
				it = it->next;
				peer* p;
				if(prev) {
					p = list_remove(ps, prev);
				}else {
					p = list_remove_head(ps);
				}
				destroy_peer(p);
			} else {
				prev = it;
				it = it->next;
			}
		}

		if(!remove_peer)
			add_to_passive_if_not_exists(state->active_view, state->passive_view, p, NULL, state->self, state->max_passive);

		while(ps->size > 0) {
			peer* p = list_remove_head(ps);
			add_to_passive_if_not_exists(state->active_view, state->passive_view, p, NULL, state->self, state->max_passive);
		}

		free(ps);

#if defined DEBUG || defined DEBUG_HYPERVIEW
		printf_shuffle_request(sr, true);
#endif
		sr->ka->size = original_ka_size;
		sr->ka->tail->next = NULL;
		destroy_shuffle_request(sr);
	}

	if(!list_find_item(state->active_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip))
		send_close_request(&msg->header.src_addr.ip, state);
}

static void process_helloneigh(YggMessage* msg, void* ptr, hyparview_state* state) {

#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing hello neighbour of %s  %d", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	uint16_t prio;
	YggMessage_readPayload(msg, ptr, &prio, sizeof(uint16_t));
	bool is_prio = ntohs(prio);
	if(is_prio) {

		peer* p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
		if(!p)
			p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

		if(!add_to_active_if_not_exists(state, state->active_view, state->pending, state->passive_view, p, state->self, state->max_active, state->max_passive, state->proto_id)) {
			destroy_peer(p);
			ygg_log("HYPARVIEW", "WARNING", "hello neighbour with high priority is already in my active view");
		} else {
			send_hello_neighbour_reply(p, state->self, true, state->proto_id);
			send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);
		}
	} else {
		peer* p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
		if(!is_active_full(state)) {
			if(!p)
				p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
			if(!add_to_active_if_not_exists(state, state->active_view, state->pending, state->passive_view, p, state->self, state->max_active, state->max_passive, state->proto_id)) {
				destroy_peer(p);
				ygg_log("HYPARVIEW","WARNING", "hello neighbour with low priority is already in my active view");
			} else {
				send_hello_neighbour_reply(p, state->self, true, state->proto_id);
				send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);
			}
		} else {
			if(!p)
				p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

			send_hello_neighbour_reply(p, state->self, false, state->proto_id);
			destroy_peer(p);
		}
	}

}

static void process_helloneighreply(YggMessage* msg, void* ptr, hyparview_state* state) {

#if defined DEBUG || defined DEBUG_HYPERVIEW
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing hello neighbour reply of %s  %d", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port);
	ygg_log("HYPARVIEW", "DEBUG", debug_msg);
#endif

	uint16_t reply;
	YggMessage_readPayload(msg, ptr, &reply, sizeof(uint16_t));
	bool is_true = ntohs(reply);

	peer* n = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(!n)
		n = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

	if(is_true) {
		state->backoff_s = state->original_backoff_s;
		state->backoff_ns = state->original_backoff_ns;

		if(add_to_active_if_not_exists(state, state->active_view, state->pending, state->passive_view, n, state->self, state->max_active, state->max_passive, state->proto_id))
			send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, n);
		else {
			destroy_peer(n);
		}

	} else {
		if(!is_active_full(state)) {
			//issue a new request
			YggTimer t;
			YggTimer_init(&t, state->proto_id, state->proto_id);
			YggTimer_set(&t, state->backoff_s, state->backoff_ns, 0, 0);

			setupTimer(&t);
		}

		if(!add_to_passive_if_not_exists(state->active_view, state->passive_view, n, NULL, state->self, state->max_passive))
			destroy_peer(n);
	}
}

static void process_msg(YggMessage* msg, hyparview_state* state) {

	uint16_t code;
	void* ptr = YggMessage_readPayload(msg, NULL, &code, sizeof(uint16_t));
	hyparview_msg_type msg_type = decodify(code);

	switch(msg_type) {
	case JOIN:
		process_join(msg, ptr, state);
		break;
	case JOINREPLY:
		process_joinreply(msg, ptr, state);
		break;
	case FORWARDJOIN:
		process_forward_join(msg, ptr, state);
		break;
	case DISCONNECT:
		process_disconnect(msg, ptr, state);
		break;
	case SHUFFLE:
		process_shuffle(msg, ptr, state);
		break;
	case SHUFFLEREPLY:
		process_shufflereply(msg, ptr, state);
		break;
	case HELLONEIGH:
		process_helloneigh(msg, ptr, state);
		break;
	case HELLONEIGHREPLY:
		process_helloneighreply(msg, ptr, state);
		break;
	}

	YggMessage_freePayload(msg); //if not destroyed, then should be destroyed

}

static shuffle_request* create_shuffle_request(hyparview_state* state) {
	shuffle_request* sr = malloc(sizeof(shuffle_request));

	sr->seq_num = next_shuffle_num(state->last_shuffle);
	sr->ka = get_random_subset(state->active_view, state->k_active);
	sr->kp = get_random_subset(state->passive_view, state->k_passive);

	if(state->active_shuffles->size > 0) {
		shuffle_request* r = (shuffle_request*) state->active_shuffles->head->data;
		if(shuffle_request_timeout(r->seq_num, sr, state->shuffle_timeout)) {
			list_remove_head(state->active_shuffles);
			destroy_shuffle_request(r);
		}
	}
	list_add_item_to_tail(state->active_shuffles, sr);

	return sr;
}

static void do_the_suffle(hyparview_state* state) {

#if defined DEBUG || defined DEBUG_HYPERVIEW
	print_views(state->active_view, state->passive_view, state->self);
#endif
	peer* p = get_random_peer(state->active_view);

	if(p) {

		shuffle_request* sr = create_shuffle_request(state);

#if defined DEBUG || defined DEBUG_HYPERVIEW
		printf_shuffle_request(sr, false);
#endif
		send_shuffle(p, state->self, sr ,state->PRWL, state->proto_id);
		state->last_shuffle = sr->seq_num;
	}

	if(state->active_view->size < state->max_active) {
		YggTimer t;
		YggTimer_init(&t, state->proto_id, state->proto_id);
		YggTimer_set(&t, state->backoff_s, state->backoff_ns, 0, 0);

		setupTimer(&t);
	}
}

static void do_another_hello_neighbour(hyparview_state* state) {
	if(!is_active_full(state)) {
		peer* p = drop_random_peer(state->passive_view);
		if(!p) {
			ygg_log("HYPARVIEW", "WARNING", "no passive peer to promote to active peer");
			return;
		}

		if(!add_to_pending_if_not_exists(state->pending, state->active_view, state->passive_view, p, state->self)){
			destroy_peer(p);
			return;
		}

		send_hello_neighbour(p, state->self, state->active_view->size + state->pending->size == 1, state->proto_id);
		state->backoff_s = (state->backoff_s*2 >= MAX_BACKOFF_S ? MAX_BACKOFF_S : state->backoff_s*2);

	}
}

static void process_timer(YggTimer* timer, hyparview_state* state) {

	if(uuid_compare(timer->id, state->shuffle->id) == 0) {
		do_the_suffle(state);
	} else {

		do_another_hello_neighbour(state);
	}

}

static void process_failed(YggEvent* ev, hyparview_state* state) {
	IPAddr ip;
	bzero(ip.addr, 16);
	void* ptr = YggEvent_readPayload(ev, NULL, ip.addr, 16);
	ptr = YggEvent_readPayload(ev, ptr, &ip.port, sizeof(unsigned short));

	peer* p = list_remove_item(state->active_view, (equal_function) equal_peer_addr, &ip);

	if(p) {
		send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, p);
		destroy_peer(p);

		if(state->active_view->size >= state->max_active)
			return;

		p = drop_random_peer(state->passive_view);
		if(!p) {
			ygg_log("HYPARVIEW", "WARNING", "no passive peer to replace dead active peer");
			return;
		}

		if(add_to_pending_if_not_exists(state->pending, state->active_view, state->passive_view, p, state->self))
			send_hello_neighbour(p, state->self, state->active_view->size + state->pending->size == 1, state->proto_id);


		state->backoff_s = state->original_backoff_s;
		state->backoff_ns = state->original_backoff_ns;

	} else {
		p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &ip);
		if(p)
			destroy_peer(p);
	}
}

static void process_event(YggEvent* ev, hyparview_state* state) {

	if(ev->proto_origin == PROTO_DISPATCH) {
		switch(ev->notification_id) {
		case TCP_DISPATCHER_CONNECTION_DOWN:
			process_failed(ev, state);
			break;
		case TCP_DISPATCHER_UNABLE_TO_CONNECT:
			process_failed(ev, state);
			break;
		}
	}

	YggEvent_freePayload(ev);

}

static void hyparview_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	hyparview_state* state = (hyparview_state*) args->state;

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		switch(elem.type) {
		case YGG_MESSAGE:
			process_msg(&elem.data.msg, state);
			break;
		case YGG_TIMER:
			process_timer(&elem.data.timer, state);
			break;
		case YGG_EVENT:
			process_event(&elem.data.event, state);
			break;
		case YGG_REQUEST:
			break;
		}
	}

}

proto_def* hyparview_init(void* args) {

	hyparview_state* state = malloc(sizeof(hyparview_state));

	hyparview_args* h = (hyparview_args*) args;

	state->ARWL = h->ARWL;
	state->PRWL = h->PRWL;
	state->k_active = h->k_active;
	state->k_passive = h->k_passive;
	state->max_active = h->max_active;
	state->max_passive = h->max_passive;

	state->self = malloc(sizeof(peer));
	getIpAddr(&state->self->ip);

	state->active_view = list_init();
	state->passive_view = list_init();
	state->pending = list_init();
	state->active_shuffles = list_init();

	state->original_backoff_s = h->backoff_s;
	state->original_backoff_ns = h->backoff_ns;

	state->shuffle_timeout = state->PRWL + state->ARWL; //TODO find something more adequate

	state->backoff_s = state->original_backoff_s;
	state->backoff_ns = state->original_backoff_ns;

	state->proto_id = PROTO_HYPARVIEW;

	//I am not the contact of myself
	if(!equal_peer_addr(state->self, &h->contact)) {
		peer* contact = create_peer(h->contact.addr, h->contact.port);

		list_add_item_to_head(state->pending, contact);

		send_join(contact, state->self, state->proto_id);
	}

	//TODO: no destroy for now
	proto_def* hyparview = create_protocol_definition(state->proto_id, "HyParView", state, NULL);


	proto_def_add_protocol_main_loop(hyparview, hyparview_main_loop);

	proto_def_add_consumed_event(hyparview, PROTO_DISPATCH, TCP_DISPATCHER_CONNECTION_DOWN);
	proto_def_add_consumed_event(hyparview, PROTO_DISPATCH, TCP_DISPATCHER_UNABLE_TO_CONNECT);

	proto_def_add_produced_events(hyparview, 2); //NEIGH UP, NEIGH DOWN


	state->shuffle = malloc(sizeof(YggTimer));
	YggTimer_init(state->shuffle, state->proto_id, state->proto_id);
	YggTimer_set(state->shuffle, h->shuffle_period_s, h->shuffle_period_ns, h->shuffle_period_s, h->shuffle_period_ns);
	setupTimer(state->shuffle);

	return hyparview;

}


hyparview_args* hyparview_args_init(const char* contact, unsigned short contact_port, int max_active, int max_passive, short ARWL, short PRWL, short k_active, short k_passive, short shuffle_period_s, long shuffle_period_ns, short backoff_s, long backoff_ns) {
	hyparview_args* args = malloc(sizeof(hyparview_args));
	bzero(args->contact.addr, 16);
	memcpy(args->contact.addr, contact, strlen(contact));
	args->contact.port = contact_port;

	args->ARWL = ARWL;
	args->PRWL = PRWL;
	args->k_active = k_active;
	args->k_passive = k_passive;
	args->max_active = max_active;
	args->max_passive = max_passive;

	args->shuffle_period_s = shuffle_period_s;
	args->shuffle_period_ns = shuffle_period_ns;

	args->backoff_s = backoff_s;
	args->backoff_ns = backoff_ns;

	return args;

}

void hyparview_args_destroy(hyparview_args* args) {
	free(args);
}
