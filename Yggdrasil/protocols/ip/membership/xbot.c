/*
 * xbot.c
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */


#include "xbot.h"

#define MAX_SEQ_NUM 6666

#define MAX_BACKOFF_S 3600 //one hour


typedef struct _peer {
	IPAddr ip;
	int score; //lower is better (normalized by THIS protocol 0-100)
}peer;

static int compare_peer_natural_score(peer* p1, peer* p2) {
	return p1->score - p2->score;
}

static int compare_peer_inverted_score(peer* p1, peer* p2) {
	return compare_peer_natural_score(p2, p1);
}

//static bool isBetter(peer* old, peer* candidate) {
//	return compare_peer_natural_score(old, candidate) > 0;
//}



static peer* create_peer(const char* ip_addr, unsigned short port) {
	peer* p = malloc(sizeof(peer));
	bzero(p->ip.addr, 16); //just to be safe
	memcpy(p->ip.addr, ip_addr, strlen(ip_addr));
	p->ip.port = port;
	p->score = -1;
	return p;
}

static void destroy_peer(peer* p) {
	free(p);
}

static bool equal_peer(peer* p1, peer* p2) {
	return (strcmp(p1->ip.addr, p2->ip.addr) == 0 && p1->ip.port == p2->ip.port);
}

static bool equal_peer_addr(peer* p1, IPAddr* ip) {
	return (strcmp(p1->ip.addr, ip->addr) == 0 && p1->ip.port == ip->port);
}

typedef struct _shuffle_request {
	unsigned short seq_num;
	list* ka;
	list* kp;
}shuffle_request;

static bool equal_shuffle_request(shuffle_request* sr, unsigned short* seq_num) {
	return sr->seq_num == *seq_num;
}

typedef struct __xbot_state {

	short proto_id;


	int max_active; //param: maximum active nodes (degree of random overlay)
	int max_passive; //param: maximum passive nodes
	short ARWL; //param: active random walk length
	short PRWL; //param: passive random walk length

	short k_active; //param: number of active nodes to exchange on shuffle
	short k_passive; //param: number of passive nodes to exchange on shuffle

	ordered_list* active_view;

	list* passive_view;
	list* pending;

	unsigned short last_shuffle;
	list* active_shuffles;
	unsigned short shuffle_timeout;

	peer* self;


	YggTimer* shuffle; //can also perform optimization upon shuffle
	//YggTimer* optimization;

	unsigned short original_backoff_s;
	long original_backoff_ns;

	unsigned short backoff_s;
	long backoff_ns;


	int passive_scan_lenght;
	int unbiased_neighs;

	//oracle dependent values
	unsigned short oracle_id;

	double oracle_max;
	double oracle_min;
	double opt_threshold;

	//for disconnect waits
	int waiting;
	list* optimizing;

}xbot_state;

static bool isBetter(peer* old, peer* candidate, xbot_state* state) {
	return candidate->score > -1 && compare_peer_natural_score(old, candidate) > 0 && ((double)compare_peer_natural_score(old, candidate)/old->score) >= state->opt_threshold;
}

typedef struct _opt_peer {
	peer* p;
	time_t tag;
}opt_peer;

static bool equal_opt_peer_addr(opt_peer* p, IPAddr* ip) {
	return equal_peer_addr(p->p, ip);
}

static bool equal_opt_peer(opt_peer* p1, peer* p2) {
	return equal_peer(p1->p, p2);
}

static void add_peer_to_opt(peer* p, xbot_state* state) {
	opt_peer* op = malloc(sizeof(opt_peer));
	op->p = p;
	op->tag = time(NULL);
	list_add_item_to_tail(state->optimizing, op);
}

static void destroy_opt_peer(opt_peer* p) {
	if(p->p)
		destroy_peer(p->p);
	free(p);
}

static bool expired_opt(opt_peer* p, xbot_state* state, time_t now) {
	return p->tag + state->shuffle_timeout < now;
}

static void clean_up_optimizing(xbot_state* state) {

	time_t now = time(NULL);
	while(state->optimizing->size > 0) {
		opt_peer* p = list_remove_head(state->optimizing);
		if(!expired_opt(p, state, now)) {
			list_add_item_to_head(state->optimizing, p);
			break;
		}
		destroy_opt_peer(p);
	}

}

static void decrement_waiting(xbot_state* state) {
	int waiting = state->waiting - 1;
	state->waiting = waiting < 0 ? 0 : waiting;
}

static void send_monitor_request(xbot_state* state, peer* p) {
	YggRequest req;
	YggRequest_init(&req, state->proto_id, state->oracle_id, REQUEST, MEASURE_REQUEST);
	YggRequest_addPayload(&req, p->ip.addr, 16);
	YggRequest_addPayload(&req, &p->ip.port, sizeof(unsigned short));

	deliverRequest(&req);
	YggRequest_freePayload(&req);
}

static void send_cancel_monitor_request(xbot_state* state, peer* p) {
	YggRequest req;
	YggRequest_init(&req, state->proto_id, state->oracle_id, REQUEST, CANCEL_MEASURE_REQUEST);
	YggRequest_addPayload(&req, p->ip.addr, 16);
	YggRequest_addPayload(&req, &p->ip.port, sizeof(unsigned short));

	deliverRequest(&req);
	YggRequest_freePayload(&req);
}

typedef enum __xbot_msg_type {
	JOIN,
	JOINREPLY,
	FORWARDJOIN,
	DISCONNECT,
	SHUFFLE,
	SHUFFLEREPLY,
	HELLONEIGH,
	HELLONEIGHREPLY,
	OPTIMIZATION,
	OPTIMIZATIONREPLY,
	REPLACE,
	REPLACEREPLY,
	SWITCH,
	SWITCHREPLY,
	DISCONNECTWAIT
}xbot_msg_type;

static bool is_active_full(xbot_state* state) {
	return (state->active_view->size + state->pending->size + state->waiting) >= state->max_active;
}

static bool is_passive_full(xbot_state* state) {
	return state->passive_view->size >= state->max_passive;
}

static void print_views(ordered_list* active, list* passive, peer* self) {

	printf("%s  %d\n", self->ip.addr, self->ip.port);

	printf("\t active\n");
	for(ordered_list_item* it = active->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;

		printf("\t\t %s  %d ::  %d\n", p->ip.addr, p->ip.port, p->score);

	}
	printf("\t passive\n");
	for(list_item* it = passive->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;
		printf("\t\t %s  %d ::  %d\n", p->ip.addr, p->ip.port, p->score);
	}

}

static void printf_shuffle_request(shuffle_request* sr, bool refs) {

	printf("shuffle number: %d\n", sr->seq_num);

	printf("\t active\n");
	for(list_item* it = sr->ka->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;

		printf("\t\t %s  %d", p->ip.addr, p->ip.port);

		if(refs)
			printf(" : ref  %p\n", p);
		else
			printf("\n");
	}


	printf("\t passive\n");
	for(list_item* it = sr->kp->head; it != NULL; it = it->next) {
		peer* p = (peer*) it->data;
		printf("\t\t %s  %d", p->ip.addr, p->ip.port);
		if(refs)
			printf(" : ref  %p\n", p);
		else
			printf("\n");
	}

}

static void send_notification(hyparview_events notification_id, short proto_id, peer* p) {
	YggEvent ev;
	YggEvent_init(&ev, proto_id, notification_id);
	YggEvent_addPayload(&ev, p->ip.addr, 16); //INET_ADDRLEN
	YggEvent_addPayload(&ev, &p->ip.port, sizeof(unsigned short));

	deliverEvent(&ev);
	YggEvent_freePayload(&ev);
}

static void send_disconnect(peer* dest, peer* self, short proto_id);


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



static uint16_t codify(xbot_msg_type msg_type) {
	return htons(msg_type);
}

static xbot_msg_type decodify(uint16_t code) {
	return ntohs(code);
}



static peer* get_random_peer_from_list(list* view) {
	if(view->size > 0) {
		int i = rand()%view->size;
		list_item* it = NULL;
		for(it = view->head; i > 0 && it != NULL; it = it->next, i--);
		if(it)
			return (peer*) it->data;
	}
	return NULL;
}

static peer* get_random_peer_from_ordered_list(ordered_list* view) {
	if(view->size > 0) {
		int i = rand()%view->size;
		ordered_list_item* it = NULL;
		for(it = view->head; i > 0 && it != NULL; it = it->next, i--);
		if(it)
			return (peer*) it->data;
	}
	return NULL;
}


static peer* get_random_peer_different_addr(ordered_list* view, IPAddr* addr) {
	if(view->size > 0) {
		int i = rand()%view->size;
		ordered_list_item* it = NULL;
		ordered_list_item* prev = NULL;
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

static peer* drop_random_ordered_peer(ordered_list* view) {
	if(view->size > 0) {
		int i = rand()%view->size;
		ordered_list_item* it = NULL;
		for(it = view->head; i > 0 && it != NULL; it = it->next, i--);

		if(it) {
			return (peer*) ordered_list_remove(view, it);
		}
	}
	return NULL;
}

static list* get_random_list_subset(list* view, short k) {

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

static list* get_random_list_ref_subset(list* view, int k) {
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

				peer* p = (peer*) it->data;
				list_add_item_to_head(subset, p);
				has[curr] = i;
				curr ++;
			}
		}
	} else {
		for(list_item* it = view->head; it != NULL; it = it->next) {
			peer* p = (peer*) it->data;
			list_add_item_to_tail(subset, p);
		}
	}

	return subset;
}

static list* get_random_list_subset_from_ordered_list(ordered_list* view, short k) {

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
				ordered_list_item* it = NULL;
				int j = i;
				for(it = view->head; j > 0 && it != NULL; it = it->next, j--);

				peer* p = create_peer(((peer*) it->data)->ip.addr, ((peer*) it->data)->ip.port);
				list_add_item_to_head(subset, p);
				has[curr] = i;
				curr ++;
			}
		}
	} else {
		for(ordered_list_item* it = view->head; it != NULL; it = it->next) {
			peer* p = create_peer(((peer*) it->data)->ip.addr, ((peer*) it->data)->ip.port);
			list_add_item_to_tail(subset, p);
		}
	}

	return subset;
}


static void send_close_request(IPAddr* ip, xbot_state* state) {

	if(list_find_item(state->optimizing, (equal_function) equal_opt_peer_addr, ip)) //lets not close connections that are being optimized ok?
		return;

	YggRequest r;
	YggRequest_init(&r, state->proto_id, PROTO_DISPATCH, REQUEST, CLOSE_CONNECTION);
	YggRequest_addPayload(&r, ip->addr, 16);
	YggRequest_addPayload(&r, &ip->port, sizeof(unsigned short));

	deliverRequest(&r);
	YggRequest_freePayload(&r);
}

//invariant p != self
static bool add_to_passive_if_not_exists(peer* p, list_item** next_to_drop, xbot_state* state) {

	if(equal_peer(p, state->self))
		return false;

	peer* a = ordered_list_find_item(state->active_view, (equal_function) equal_peer, p);

	if(a) //p already exists in active view
		return false;

	a = list_find_item(state->passive_view, (equal_function) equal_peer, p);
	if(a)
		return false;

	if(state->passive_view->size >= state->max_passive) { //just in case
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

		}

		if(n) {
			send_cancel_monitor_request(state, n);
			destroy_peer(n);
		}
	}

	list_add_item_to_tail(state->passive_view, p);
	return true;
}

static void drop_random_from_active_view(xbot_state* state) {

	peer* p;
	if(state->pending->size > 0) {
		p = drop_random_peer(state->pending);
	}
	else {
		p = drop_random_ordered_peer(state->active_view);
		if(p) {
			send_disconnect(p, state->self, state->proto_id);
			send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, p);
		}
	}

	if(!p) {
		ygg_log("XBOT", "WARNING",  "no peer got dropped from active view");
		return;
	}

	bool ok = add_to_passive_if_not_exists(p, NULL, state);
	if(!ok)
		destroy_peer(p);

}

static bool add_to_pending_if_not_exists(peer** p, xbot_state* state) {
	if(equal_peer(*p, state->self))
		return false;

	peer* a = list_find_item(state->pending, (equal_function) equal_peer, *p);
	if(a)
		return false;
	a = ordered_list_find_item(state->active_view, (equal_function) equal_peer, *p);
	if(a)
		return false;

	a = list_remove_item(state->passive_view, (equal_function) equal_peer, *p);
	if(a && a != *p) {
		destroy_peer(*p);
		*p = a;
	} else
		send_monitor_request(state, *p);

	list_add_item_to_tail(state->pending, *p);
	return true;
}

static bool add_to_active_if_not_exists(peer** p, xbot_state* state) {

	if(equal_peer(*p, state->self))
		return false;

	peer* a = ordered_list_find_item(state->active_view, (equal_function) equal_peer, *p);
	if(a)
		return false;

	bool new_node = false;

	a = list_remove_item(state->pending, (equal_function) equal_peer, *p);
	if(a && a != *p) {
		destroy_peer(*p);
		*p = a;
	} else {
		a = list_remove_item(state->passive_view, (equal_function) equal_peer, *p);
		if(a && a != *p) {
			destroy_peer(*p);
			*p = a;
		}else
			new_node = true;
	}


	if(state->active_view->size + state->pending->size >= state->max_active)
		drop_random_from_active_view(state);

	ordered_list_add_item(state->active_view, *p);
	if(new_node)
		send_monitor_request(state, *p);

	return true;

}

static void add_peer_list_to_msg(YggMessage* msg, list* peers) {

	uint16_t size = htons(peers->size);
	YggMessage_addPayload(msg, (char*) &size, sizeof(uint16_t));

	for(list_item* it = peers->head; it != NULL; it = it->next) {
		struct in_addr addr;
		if(!inet_aton(((peer*)it->data)->ip.addr, &addr))
			printf("Error on inet_aton (add peer list to msg) %s\n", ((peer*)it->data)->ip.addr);
		YggMessage_addPayload(msg, (char*) &addr, sizeof(struct in_addr));
		uint16_t port = htons(((peer*)it->data)->ip.port);
		YggMessage_addPayload(msg, (char*) &port, sizeof(uint16_t));
	}
}

static void init_msg_header(YggMessage* msg, xbot_msg_type type, short proto_id, peer* dest, peer* self) {
	YggMessage_initIp(msg, proto_id, dest->ip.addr, dest->ip.port);
	uint16_t code = codify(type);
	YggMessage_addPayload(msg, (char*) &code, sizeof(uint16_t));
}


/***********************************************************
 *  HYPARVIEW SEND msgs
 **********************************************************/

static void send_shuffle_reply(xbot_state* state, peer* dest, peer* self, unsigned short seq_num, list* k_p, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending shuffle reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, SHUFFLEREPLY, proto_id, dest, self);

	uint16_t seq = htons(seq_num);
	YggMessage_addPayload(&msg, (char*) &seq, sizeof(uint16_t));

	add_peer_list_to_msg(&msg, k_p);

	dispatch(&msg);
	YggMessage_freePayload(&msg);

	if(!ordered_list_find_item(state->active_view, (equal_function) equal_peer_addr, &dest->ip))
		send_close_request(&dest->ip, state);

}

//new time to leave must have been evaluated and changed accordingly before call
static void forward_shuffle(YggMessage* shuffle_msg, peer* dest) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "forwarding shuffle to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif
	memcpy(shuffle_msg->header.dst_addr.ip.addr, dest->ip.addr, 16);
	shuffle_msg->header.dst_addr.ip.port = dest->ip.port;
	dispatch(shuffle_msg);
	YggMessage_freePayload(shuffle_msg);
}

//dest is random; time_to_live == PRWL ?
static void send_shuffle(peer* dest, peer* self, shuffle_request* sr, short time_to_live, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending shuffle to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif


	YggMessage msg;
	init_msg_header(&msg, SHUFFLE, proto_id, dest, self);

	uint16_t ttl = htons(time_to_live);
	YggMessage_addPayload(&msg, (char*) &ttl, sizeof(uint16_t));

	struct in_addr addr;
	inet_aton(self->ip.addr, &addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(self->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	uint16_t seq = htons(sr->seq_num);
	YggMessage_addPayload(&msg, (char*) &seq, sizeof(uint16_t));

	add_peer_list_to_msg(&msg, sr->ka);
	add_peer_list_to_msg(&msg, sr->kp);

	dispatch(&msg);
	YggMessage_freePayload(&msg);


}

static void send_hello_neighbour_reply(peer* dest, peer* self, bool reply, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending hello reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, HELLONEIGHREPLY, proto_id, dest, self);

	uint16_t rep = htons(reply);
	YggMessage_addPayload(&msg, (char*) &rep, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

//prio == true --> high; prio == false --> low
static void send_hello_neighbour(peer* dest, peer* self, bool prio, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending hello to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, HELLONEIGH, proto_id, dest, self);

	uint16_t pr = htons(prio); //booleans are just numbers...

	YggMessage_addPayload(&msg, (char*) &pr, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);

}

static void send_disconnect(peer* dest, peer* self, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending disconnect to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, DISCONNECT, proto_id, dest, self);

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void send_forward_join(peer* dest, peer* newNode, short time_to_live, peer* self, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending forward join to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, FORWARDJOIN, proto_id, dest, self);

	uint16_t ttl = htons(time_to_live);
	YggMessage_addPayload(&msg, (char*) &ttl, sizeof(uint16_t));
	struct in_addr addr;
	if(!inet_aton(newNode->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (forward join) %s\n", newNode->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(newNode->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void send_joinreply(peer* dest, peer* self, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending join reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, JOINREPLY, proto_id, dest, self);
	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

static void send_join(peer* dest, peer* self, short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending join to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, JOIN, proto_id, dest, self);

	dispatch(&msg);
	YggMessage_freePayload(&msg);

}

/***********************************************************
 *  HYPARVIEW PROCESS msgs
 **********************************************************/

static void process_join(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing join of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	peer* p = ordered_list_find_item(state->active_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	bool new_node = false;
	if(p)
		return; //report error?

	p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	if(is_active_full(state))
		drop_random_from_active_view(state);

	if(!p) //it was not in pending
		p = list_remove_item(state->passive_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(!p) { //it was not in passive
		p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
		new_node = true;
	}

	ordered_list_add_item(state->active_view, p);
	if(new_node)
		send_monitor_request(state, p);

	send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);

	//skip new node
	ordered_list_item* it = state->active_view->head->next;
	while(it != NULL) {
		send_forward_join(it->data, p, state->ARWL, state->self, state->proto_id);
		it = it->next;
	}

	send_joinreply(p, state->self, state->proto_id);
}

static void process_joinreply(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing join reply of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	//remove from pending, add to active, send notification
	peer* p = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
	if(p) {
		bool ok = add_to_active_if_not_exists(&p, state);
		if(!ok) {
			destroy_peer(p);
			return;
		}

		send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);
	} else {
		if(!ordered_list_find_item(state->active_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip)) { //if not in active, then send disconnect
			p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
			send_disconnect(p, state->self, state->proto_id);
			destroy_peer(p);
		}
	}
}

static void process_forward_join(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing forward join of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
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
		bool ok = add_to_pending_if_not_exists(&p, state);
		if(!ok) {
			destroy_peer(p);
		} else {
			send_hello_neighbour(p, state->self, state->active_view->size + state->pending->size + state->waiting == 1, state->proto_id);
		}
	} else {
		if(time_to_live == state->PRWL) {
			peer* p = create_peer(ip_addr, port_number);
			bool ok = add_to_passive_if_not_exists(p, NULL, state);
			if(!ok)
				destroy_peer(p);
			else
				send_monitor_request(state, p);
		}
		peer* new = create_peer(ip_addr, port_number);
		peer* p = get_random_peer_different_addr(state->active_view, &new->ip);
		if(p) {
			send_forward_join(p, new, time_to_live-1, state->self, state->proto_id);
		} else
			ygg_log("XBOT", "WARNING", "no destination to send forward join");

		destroy_peer(new);
	}

}

static void process_disconnect(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing disconnect of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif
	peer* p = ordered_list_remove_item(state->active_view, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);

	if(p) {
		send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, p);

		send_close_request(&p->ip, state);

		if(!add_to_passive_if_not_exists(p, NULL, state)) {
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

static bool make_space(xbot_state* state, peer* p, list_item** next_to_drop, int req_sapce) {


	bool remove_peer = false;

	if(ordered_list_find_item(state->active_view, (equal_function) equal_peer, p))
		remove_peer = true;
	if(!remove_peer && list_find_item(state->pending, (equal_function) equal_peer, p))
		remove_peer = true;
	if(!remove_peer && !list_find_item(state->passive_view, (equal_function) equal_peer, p)){
		if(state->passive_view->size + req_sapce > state->max_passive) {

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

				if(n) {
					send_cancel_monitor_request(state, n);
					destroy_peer(n);
				}
			}
		}
	}else
		remove_peer = true;

	return remove_peer;
}

static bool apply_shuffle_request(peer* shuffler, shuffle_request* sr, list* passive_subset, xbot_state* state) {

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
		add_to_passive_if_not_exists(p, NULL, state);
		send_monitor_request(state, p);
	}

	while(sr->kp->size > 0) {
		peer* p = list_remove_head(sr->kp);
		add_to_passive_if_not_exists(p, NULL, state);
		send_monitor_request(state, p);
	}

	if(!remove_shuffler) {
		add_to_passive_if_not_exists(shuffler, NULL, state);
		send_monitor_request(state, shuffler);
	}

	return !remove_shuffler;

}

static void destroy_passive_subset(list* passive_subset) {
	while(passive_subset->size > 0) {
		peer* p = list_remove_head(passive_subset);
		destroy_peer(p);
	}
	free(passive_subset);

}

static void process_shuffle(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing shuffle of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
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

#ifdef DEBUG
		printf_shuffle_request(sr, false);
#endif

		list* passive_subset = get_random_list_subset(state->passive_view, sr->ka->size+sr->kp->size);

		bool ok = apply_shuffle_request(temp_p, sr, passive_subset, state);

		send_shuffle_reply(state, temp_p, state->self, sr->seq_num, passive_subset, state->proto_id);

		if(!ok)
			destroy_peer(temp_p);

		destroy_shuffle_request(sr);
		destroy_passive_subset(passive_subset);
	}

}

static void process_shufflereply(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing shuffle reply of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	uint16_t seq;
	ptr = YggMessage_readPayload(msg, ptr, &seq, sizeof(uint16_t));

	unsigned short seq_h = ntohs(seq);

	shuffle_request* sr = list_remove_item(state->active_shuffles, (equal_function) equal_shuffle_request, &seq_h);

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

		if(!remove_peer) {
			add_to_passive_if_not_exists(p, NULL, state);
			send_monitor_request(state, p);
		}

		while(ps->size > 0) {
			peer* p = list_remove_head(ps);
			add_to_passive_if_not_exists(p, NULL, state);
			send_monitor_request(state, p);
		}

		free(ps);

#ifdef DEBUG
		printf_shuffle_request(sr, true);
#endif

		sr->ka->size = original_ka_size;
		sr->ka->tail->next = NULL;
		destroy_shuffle_request(sr);
	}

}

static void process_helloneigh(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing hello neighbour of %s  %d\n", msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	uint16_t prio;
	YggMessage_readPayload(msg, ptr, &prio, sizeof(uint16_t));
	bool is_prio = ntohs(prio);
	peer* p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	if(is_prio) {

		if(!add_to_active_if_not_exists(&p, state)) {
			destroy_peer(p);
			ygg_log("XBOT", "WARNING", "hello neighbour with high priority is already in my active view");
		} else {
			send_hello_neighbour_reply(p, state->self, true, state->proto_id);
			send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);
		}
	} else {

		if(!is_active_full(state)) {

			if(!add_to_active_if_not_exists(&p, state)) {
				destroy_peer(p);
				ygg_log("XBOT","WARNING", "hello neighbour with low priority is already in my active view");
			} else {
				send_hello_neighbour_reply(p, state->self, true, state->proto_id);
				send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, p);
			}
		} else {

			send_hello_neighbour_reply(p, state->self, false, state->proto_id);
			destroy_peer(p);
		}
	}

}

static void process_helloneighreply(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing hello neighbour reply of %s  %d", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	uint16_t reply;
	YggMessage_readPayload(msg, ptr, &reply, sizeof(uint16_t));
	bool is_true = ntohs(reply);


	peer* n = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

	if(is_true) {

		state->backoff_s = state->original_backoff_s;
		state->backoff_ns = state->original_backoff_ns;

		if(add_to_active_if_not_exists(&n, state))
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

		peer* c = list_remove_item(state->pending, (equal_function) equal_peer_addr, &msg->header.src_addr.ip);
		if(c) {
			destroy_peer(n);
			n = c;
		}

		//maybe we should close this connection
		send_close_request(&n->ip, state); //btw this could go horribly wrong if there is shuffle taking place.

		if(!add_to_passive_if_not_exists(n, NULL, state))
			destroy_peer(n);
	}
}


/***********************************************************
 *  X-BOT SEND msgs
 **********************************************************/

//dest = candidate; self = initiator
static void send_optimization(peer* dest, peer* old, peer* self, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending optimization to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, OPTIMIZATION, proto_id, dest, self);

	struct in_addr addr;
	if(!inet_aton(old->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send optimization) %s\n", old->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(old->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);

}

//dest = initiator; self = candidate
static void send_optimization_reply(peer* dest, bool answer, peer* old, peer* disconnected, peer* self, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending optimization reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif
	YggMessage msg;
	init_msg_header(&msg, OPTIMIZATIONREPLY, proto_id, dest, self);

	uint16_t answer_t = htons(answer);
	YggMessage_addPayload(&msg, (char*) &answer_t, sizeof(uint16_t));


	struct in_addr addr;
	if(!inet_aton(old->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send optimization reply) %s\n", old->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(old->ip.port);
	YggMessage_addPayload(&msg, (char* )&port, sizeof(uint16_t));

	if(disconnected) {
		if(!inet_aton(disconnected->ip.addr, &addr)) //this could already be converted..
			printf("Error on inet_aton (send optimization reply) %s\n", disconnected->ip.addr);

		YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
		port = htons(disconnected->ip.port);
		YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));
	}


	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

//dest = disconnected; self = candidate
static void send_replace(peer* dest, peer* old, peer* initiator, peer* self, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending replace to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, REPLACE, proto_id, dest, self);

	//add old
	struct in_addr addr;
	if(!inet_aton(old->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send replace) %s\n", old->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(old->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	//add initiator
	if(!inet_aton(initiator->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send replace) %s\n", initiator->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	port = htons(initiator->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

//dest = candidate; self = disconnected
static void send_replace_reply(peer* dest, bool answer, peer* initiator, peer* old, peer* self, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending replace reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif


	YggMessage msg;
	init_msg_header(&msg, REPLACEREPLY, proto_id, dest, self);

	uint16_t answer_t = htons(answer);
	YggMessage_addPayload(&msg, (char*) &answer_t, sizeof(uint16_t));


	//add initiator
	struct in_addr addr;
	if(!inet_aton(initiator->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send replace reply) %s\n", initiator->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(initiator->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));


	//add old
	if(!inet_aton(old->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send replace reply) %s\n", old->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	port = htons(old->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));



	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

//dest = old; self = disconnected
static void send_switch(peer* dest, peer* initiator, peer* candidate, peer* self, unsigned short proto_id) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending switch to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, SWITCH, proto_id, dest, self);

	//add initiator
	struct in_addr addr;
	if(!inet_aton(initiator->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send switch) %s\n", initiator->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(initiator->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	//add candidate
	if(!inet_aton(candidate->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send switch) %s\n", candidate->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	port = htons(candidate->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));

	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

//dest = disconnected; self = old
static void send_switch_reply(peer* dest, bool answer, peer* initiator, peer* candidate, peer* self, unsigned short proto_id) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending switch reply to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, SWITCHREPLY, proto_id, dest, self);

	uint16_t answer_t = htons(answer);
	YggMessage_addPayload(&msg, (char*) &answer_t, sizeof(uint16_t));

	//add initiator
	struct in_addr addr;
	if(!inet_aton(initiator->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send switch reply) %s\n", initiator->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	uint16_t port = htons(initiator->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));


	//add candidate
	if(!inet_aton(candidate->ip.addr, &addr)) //this could already be converted..
		printf("Error on inet_aton (send switch reply) %s\n", candidate->ip.addr);

	YggMessage_addPayload(&msg, (char*) &addr, sizeof(struct in_addr));
	port = htons(candidate->ip.port);
	YggMessage_addPayload(&msg, (char*) &port, sizeof(uint16_t));


	dispatch(&msg);
	YggMessage_freePayload(&msg);
}


static void send_disconnect_wait(peer* dest, peer* self, unsigned short proto_id) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "sending disconnect wait to %s  %d", dest->ip.addr,  dest->ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	YggMessage msg;
	init_msg_header(&msg, DISCONNECTWAIT, proto_id, dest, self);
	dispatch(&msg);
	YggMessage_freePayload(&msg);
}

/***********************************************************
 *  X-BOT PROCESS msgs
 **********************************************************/

static bool can_add_to_active_view(peer* p, xbot_state* state) {
	return (!equal_peer(p, state->self) && !ordered_list_find_item(state->active_view, (equal_function) equal_peer, p) && !list_find_item(state->pending, (equal_function) equal_peer, p));
}

//src = initiator; dest = candidate
static void process_optimization(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing optimization of %s  %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	struct in_addr old_addr;
	uint16_t old_port;

	ptr = YggMessage_readPayload(msg, ptr, &old_addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &old_port, sizeof(uint16_t));

	char* ip_addr = inet_ntoa(old_addr);
	peer* old = create_peer(ip_addr, ntohs(old_port));
	peer* initiator = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);

	if(!is_active_full(state)) {
		bool ok = add_to_active_if_not_exists(&initiator, state);
		if(!ok) { //this would be a tremendous error in the protocol if this ever run
			ygg_log("XBOT", "WARNING", "on optimization, initiator is already in my active view");
			destroy_peer(initiator);
		} else {
			send_optimization_reply(initiator, true, old, NULL, state->self, state->proto_id);
			send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, initiator);
		}
	} else {
		peer* disconnect = ordered_list_get_item_by_index(state->active_view, state->unbiased_neighs);

		if(!disconnect) {
			send_optimization_reply(initiator, false, old, NULL, state->self, state->proto_id);
			destroy_peer(initiator);
		} else {
			send_replace(disconnect, old, initiator, state->self, state->proto_id);
			add_peer_to_opt(initiator, state);
		}
	}
	destroy_peer(old);
	YggMessage_freePayload(msg);
}

//src = candidate; dest = initiator
static void process_optimizationreply(YggMessage* msg, void* ptr, xbot_state* state) {

	uint16_t answer_t;
	ptr = YggMessage_readPayload(msg, ptr, &answer_t, sizeof(uint16_t));
	bool answer = ntohs(answer_t);
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing optimization reply of %s  %d  answer: %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port, answer);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif
	opt_peer* candidate = list_remove_item(state->optimizing, (equal_function)equal_opt_peer_addr, &msg->header.src_addr.ip);
	if(!candidate) {
		candidate = malloc(sizeof(opt_peer));
		candidate->p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	}

	if(answer) {
		struct in_addr addr;
		uint16_t port;
		ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
		ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));
		char* ip_addr = inet_ntoa(addr);
		IPAddr ip;
		bzero(ip.addr, 16);
		memcpy(ip.addr, ip_addr, strlen(ip_addr));
		ip.port = ntohs(port);
		peer* old = NULL;
		if((old = ordered_list_remove_item(state->active_view, (equal_function)equal_peer_addr, &ip))) {
			ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
			if(ptr) {
				//send_disconnect_wait(old, state->self, state->proto_id); simply wait for disconnect wait of old
			}else
				send_disconnect(old, state->self, state->proto_id);

			if(!add_to_passive_if_not_exists(old, NULL, state))
				destroy_peer(old);
			else{
				send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, old);
			}
		}

		if(candidate && !add_to_active_if_not_exists(&candidate->p, state))
			destroy_peer(candidate->p);
		else if(candidate) {
			send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, candidate->p);
			decrement_waiting(state);
		}
	} else {
		if(candidate->p)
			destroy_peer(candidate->p);
		send_close_request(&msg->header.src_addr.ip, state);
	}
	if(candidate) {
		candidate->p = NULL;
		destroy_opt_peer(candidate);
	}
	YggMessage_freePayload(msg);
}

//src = candidate; dest = disconnected
static void process_replace(YggMessage* msg, void* ptr, xbot_state* state) {

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing replace of %s  %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif
	struct in_addr addr;
	uint16_t port;
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));

	char* ip_addr = inet_ntoa(addr);


	peer* old = create_peer(ip_addr, ntohs(port));
	peer* p = list_find_item(state->passive_view, (equal_function)equal_peer, old);

	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));

	ip_addr = inet_ntoa(addr);

	peer* initiator = create_peer(ip_addr, ntohs(port));

	peer* candidate = ordered_list_find_item(state->active_view, (equal_function)equal_peer_addr, &msg->header.src_addr.ip);

	if(candidate && p) {
		if(!isBetter(candidate, p, state)) {
			send_replace_reply(candidate, false, initiator, old, state->self, state->proto_id);
			destroy_peer(old);
		} else {
			add_peer_to_opt(old, state);
			send_switch(old, initiator, candidate, state->self, state->proto_id);
		}
	}

	destroy_peer(initiator);
	YggMessage_freePayload(msg);
}

//src = disconnected; dest = candidate
static void process_replacereply(YggMessage* msg, void* ptr, xbot_state* state) {
	uint16_t answer_t;
	ptr = YggMessage_readPayload(msg, ptr, &answer_t, sizeof(uint16_t));
	bool answer = ntohs(answer_t);

#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing replace reply of %s  %d  answer: %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port, answer);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	struct in_addr addr;
	uint16_t port;
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));
	char* ip_addr = inet_ntoa(addr);

	IPAddr ip;
	bzero(ip.addr, 16);
	memcpy(ip.addr, ip_addr, strlen(ip_addr));
	ip.port = ntohs(port);

	opt_peer* initiator = list_remove_item(state->optimizing, (equal_function)equal_opt_peer_addr, &ip); //if this NULL ??? (should not be, if correct)

	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));
	ip_addr = inet_ntoa(addr);

	peer* old = create_peer(ip_addr, ntohs(port));

	peer* disconnected = NULL;
	bool up = false;

	if(answer) {
		disconnected = ordered_list_remove_item(state->active_view, (equal_function)equal_peer_addr, &msg->header.src_addr.ip);
		if(disconnected) {
			add_to_passive_if_not_exists(disconnected, NULL, state);
			send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, disconnected);
		}

		if(initiator && !add_to_active_if_not_exists(&initiator->p, state)) {
			destroy_peer(initiator->p);
		} else if(initiator) {
			up = true;
		}

	}
	if(!disconnected)
		disconnected = ordered_list_find_item(state->active_view, (equal_function)equal_peer_addr, &msg->header.src_addr.ip);

	if(initiator)
		send_optimization_reply(initiator->p, answer, old, disconnected, state->self, state->proto_id);

	if(up)
		send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, initiator->p);

	destroy_peer(old);
	if(!answer && initiator)
		destroy_peer(initiator->p);

	if(initiator) {
		initiator->p = NULL;
		destroy_opt_peer(initiator);
	}
	YggMessage_freePayload(msg);
}

//src = disconnect; dest = old
static void process_switch(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing switch of %s  %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	struct in_addr addr;
	uint16_t port;
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));

	char* ip_addr = inet_ntoa(addr);

	IPAddr ip;
	bzero(ip.addr, 16);
	memcpy(ip.addr, ip_addr, strlen(ip_addr));
	ip.port = ntohs(port);
	peer* initiator = NULL;
	bool destroy_initiator = false;
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));

	ip_addr = inet_ntoa(addr);

	peer* candidate = create_peer(ip_addr, ntohs(port));

	peer* disconnected = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	bool answer = false;

	if((can_add_to_active_view(disconnected, state) && (initiator = ordered_list_remove_item(state->active_view, (equal_function)equal_peer_addr, &ip)))) {

		send_disconnect_wait(initiator, state->self, state->proto_id);
		if(!add_to_passive_if_not_exists(initiator, NULL, state))
			destroy_initiator = true;
		else {
			send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, initiator);
		}
		if(add_to_active_if_not_exists(&disconnected, state)) {

			answer = true;
		} else
			destroy_peer(disconnected);

	}

	if(!initiator) {
		initiator = create_peer(ip.addr, ip.port);
		destroy_initiator = true;
	}

	send_switch_reply(disconnected, answer, initiator, candidate, state->self, state->proto_id);
	if(destroy_initiator)
		destroy_peer(initiator);

	if(!answer)
		destroy_peer(disconnected);
	else
		send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, disconnected);

	destroy_peer(candidate);

	YggMessage_freePayload(msg);
}

//src = old; dest = disconnect
static void process_switchreply(YggMessage* msg, void* ptr, xbot_state* state) {

	uint16_t answer_t;
	ptr = YggMessage_readPayload(msg, ptr, &answer_t, sizeof(uint16_t));
	bool answer = ntohs(answer_t);
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing switch reply of %s  %d answer: %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port, answer);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif

	struct in_addr addr;
	uint16_t port;
	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));
	char* ip_addr = inet_ntoa(addr);

	peer* initiator = create_peer(ip_addr, ntohs(port));

	ptr = YggMessage_readPayload(msg, ptr, &addr, sizeof(struct in_addr));
	ptr = YggMessage_readPayload(msg, ptr, &port, sizeof(uint16_t));
	ip_addr = inet_ntoa(addr);

	IPAddr ip;
	bzero(ip.addr, 16);
	memcpy(ip.addr, ip_addr, strlen(ip_addr));
	ip.port = ntohs(port);

	peer* candidate = NULL;

	//old should not be NULL
	opt_peer* old = list_remove_item(state->optimizing, (equal_function)equal_opt_peer_addr, &msg->header.src_addr.ip);
	if(!old) {
		old = malloc(sizeof(opt_peer));
		old->p = create_peer(msg->header.src_addr.ip.addr, msg->header.src_addr.ip.port);
	}

	bool down = false;

	if(answer) {
		candidate = ordered_list_remove_item(state->active_view, (equal_function)equal_peer_addr, &ip);
		if(candidate) {
			add_to_passive_if_not_exists(candidate, NULL, state);
			down = true;
		}
		if(old && add_to_active_if_not_exists(&old->p, state)) {
			send_notification(OVERLAY_NEIGHBOUR_UP, state->proto_id, old->p);
		}else if(old)
			destroy_peer(old->p);
	}
	if(!candidate)
		candidate = ordered_list_find_item(state->active_view, (equal_function)equal_peer_addr, &ip);

	if(candidate)
		send_replace_reply(candidate, answer, initiator, old->p, state->self, state->proto_id);
	else {
		char warning_msg[100];
		bzero(warning_msg, 100);
		sprintf(warning_msg, "I no longer known the candidate peer: %s  %d\n",ip.addr,ip.port); //send false ?
		ygg_log("XBOT", "WARNING", warning_msg);
	}
	if(down) {
		send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, candidate);
	}

	if(!answer && old)
		destroy_peer(old->p);
	destroy_peer(initiator);

	if(old) {
		old->p = NULL;
		destroy_opt_peer(old);
	}
	YggMessage_freePayload(msg);

}

static void process_disconnectwait(YggMessage* msg, void* ptr, xbot_state* state) {
#ifdef DEBUG
	char debug_msg[100];
	bzero(debug_msg, 100);
	sprintf(debug_msg, "processing disconnect wait of %s  %d\n", msg->header.src_addr.ip.addr,  msg->header.src_addr.ip.port);
	ygg_log("XBOT", "DEBUG", debug_msg);
#endif
	send_close_request(&msg->header.src_addr.ip, state);
	peer* p = ordered_list_remove_item(state->active_view, (equal_function)equal_peer_addr, &msg->header.src_addr.ip);
	if(p) {
		state->waiting ++;
		if(!add_to_passive_if_not_exists(p, NULL, state)) {
			destroy_peer(p);
		} else
			send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, p);
	}

	YggMessage_freePayload(msg);

}

/**********************************************************
 *  Yggdrasil stuff
 **********************************************************/

static void process_msg(YggMessage* msg, xbot_state* state) {

	uint16_t code;
	void* ptr = YggMessage_readPayload(msg, NULL, &code, sizeof(uint16_t));
	xbot_msg_type msg_type = decodify(code);

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
	case OPTIMIZATION:
		process_optimization(msg, ptr, state);
		break;
	case OPTIMIZATIONREPLY:
		process_optimizationreply(msg, ptr, state);
		break;
	case REPLACE:
		process_replace(msg, ptr, state);
		break;
	case REPLACEREPLY:
		process_replacereply(msg, ptr, state);
		break;
	case SWITCH:
		process_switch(msg, ptr, state);
		break;
	case SWITCHREPLY:
		process_switchreply(msg, ptr, state);
		break;
	case DISCONNECTWAIT:
		process_disconnectwait(msg, ptr, state);
		break;
	}

	YggMessage_freePayload(msg); //if not destroyed, then should be destroyed

}

static shuffle_request* create_shuffle_request(xbot_state* state) {
	shuffle_request* sr = malloc(sizeof(shuffle_request));

	sr->seq_num = next_shuffle_num(state->last_shuffle);
	sr->ka = get_random_list_subset_from_ordered_list(state->active_view, state->k_active);
	sr->kp = get_random_list_subset(state->passive_view, state->k_passive);

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

static void do_the_suffle(xbot_state* state) {

	peer* p = get_random_peer_from_ordered_list(state->active_view);

	if(p) {

		shuffle_request* sr = create_shuffle_request(state);

#ifdef DEBUG
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

static void do_another_hello_neighbour(xbot_state* state) {
	if(!is_active_full(state)) {
		peer* p = drop_random_peer(state->passive_view);
		if(!p) {
			ygg_log("XBOT", "WARNING", "no passive peer to promote to active peer");
			return;
		}

		if(!add_to_pending_if_not_exists(&p, state)){
			destroy_peer(p);
			return;
		}

		send_hello_neighbour(p, state->self,  state->active_view->size + state->pending->size + state->waiting == 1, state->proto_id);
		state->backoff_s = (state->backoff_s*2 >= MAX_BACKOFF_S ? MAX_BACKOFF_S : state->backoff_s*2);
	}
}


//TODO: only allow one optimization round to take place?
static void do_optimization(xbot_state* state) {
	decrement_waiting(state);
	clean_up_optimizing(state);
	if(!is_active_full(state))
		return;

	list* candidates = get_random_list_ref_subset(state->passive_view, state->passive_scan_lenght);
	int i = 0;
	for(ordered_list_item* it = state->active_view->head; it != NULL; it = it->next, i++) {
		if(i >= state->unbiased_neighs) {
			peer* o = (peer*)it->data;
			while(candidates->size > 0) {
				peer* c = list_remove_head(candidates);
				if(isBetter(o,c, state)) {
					peer* to_optimize = create_peer(c->ip.addr, c->ip.port);
					add_peer_to_opt(to_optimize, state);
					send_optimization(c, o, state->self, state->proto_id);
					break;
				}
			}
		}
	}

}

static void process_timer(YggTimer* timer, xbot_state* state) {

	if(uuid_compare(timer->id, state->shuffle->id) == 0) {
#ifdef DEBUG
		print_views(state->active_view, state->passive_view, state->self);
#endif
		do_optimization(state);
		do_the_suffle(state);
	} else {
		do_another_hello_neighbour(state);
	}

}

static void process_failed(YggEvent* ev, xbot_state* state) {
	IPAddr ip;
	bzero(ip.addr, 16);
	void* ptr = YggEvent_readPayload(ev, NULL, ip.addr, 16);
	ptr = YggEvent_readPayload(ev, ptr, &ip.port, sizeof(unsigned short));

	peer* p = ordered_list_remove_item(state->active_view, (equal_function)equal_peer_addr, &ip);
	opt_peer* op = NULL;

	if(p) {
		send_cancel_monitor_request(state, p);
		send_notification(OVERLAY_NEIGHBOUR_DOWN, state->proto_id, p);
		destroy_peer(p);

		if(is_active_full(state))
			return;

		p = drop_random_peer(state->passive_view);
		if(!p) {
			ygg_log("XBOT", "WARNING", "no passive peer to replace dead active peer");
			return;
		}

		if(add_to_pending_if_not_exists(&p, state))
			send_hello_neighbour(p, state->self,  state->active_view->size + state->pending->size + state->waiting == 1, state->proto_id);


		state->backoff_s = state->original_backoff_s;
		state->backoff_ns = state->original_backoff_ns;

	} else if(ev->notification_id == TCP_DISPATCHER_UNABLE_TO_CONNECT && (op = list_remove_item(state->optimizing, (equal_function)equal_opt_peer_addr, &ip))) {

		destroy_opt_peer(op);
		p = list_remove_item(state->passive_view, (equal_function)equal_peer_addr, &ip);
		if(p) {
			send_cancel_monitor_request(state, p);
			destroy_peer(p);
		}

	}
	else if(ev->notification_id == TCP_DISPATCHER_UNABLE_TO_CONNECT) {
		p = list_remove_item(state->pending, (equal_function)equal_peer_addr, &ip);
		if(p) {
			send_cancel_monitor_request(state, p);
			destroy_peer(p);
		}

	}
}

static void process_event(YggEvent* ev, xbot_state* state) {

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
//Left to apply a biased synthetic oracle
#include "utils/hashfunctions.h"

static int hash(IPAddr* ip1, IPAddr *ip2) {
	unsigned short len = 32+sizeof(unsigned short)*2;
	char* hashable = malloc(len);
	int off = 0;
	memcpy(hashable+off, ip1->addr, 16);
	off+= 16;
	memcpy(hashable+off, &ip1->port, sizeof(unsigned short));
	off+=sizeof(unsigned short);
	memcpy(hashable+off, ip2->addr, 16);
	off+=16;
	memcpy(hashable+off, &ip2->port, sizeof(unsigned short));
	int res = DJBHash(hashable, len);
	return res < 0 ? res * -1 : res;
}

static void process_request(YggRequest* req, xbot_state* state) {

	if(req->request != REPLY && req->proto_origin != state->oracle_id) {
		YggRequest_freePayload(req);
		return;
	}


	IPAddr ip;
	double measurement;

	void* ptr = YggRequest_readPayload(req, NULL, ip.addr, 16);
	ptr = YggRequest_readPayload(req, ptr, &ip.port, sizeof(unsigned short));
	YggRequest_readPayload(req, ptr, &measurement, sizeof(double));

	int normalized = ((measurement - state->oracle_min)/(state->oracle_max - state->oracle_min))*100;

	//Left to apply a biased synthetic oracle
	if(ip.port < state->self->ip.port)
		normalized = hash(&ip, &state->self->ip)%100;
	else
		normalized = hash(&state->self->ip, &ip)%100;

	peer* p = ordered_list_find_item(state->active_view, (equal_function)equal_peer_addr, &ip);
	if(p) {

		p->score = normalized;
		ordered_list_update_item(state->active_view, (equal_function)equal_peer, p);
	} else {
		p = list_find_item(state->passive_view, (equal_function) equal_peer_addr, &ip);
		if(p) {

			p->score = normalized;
		}else {
			p = list_find_item(state->pending, (equal_function)equal_peer_addr, &ip);
			if(p) {

				p->score = normalized;
			}
		}
	}

	YggRequest_freePayload(req);

}

static void xbot_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	xbot_state* state = (xbot_state*) args->state;

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
			process_request(&elem.data.request, state);
			break;
		}
	}

}

proto_def* xbot_init(void* args) {

	xbot_state* state = malloc(sizeof(xbot_state));

	xbot_args* x = (xbot_args*) args;
	hyparview_args* h = x->hyparview_conf;

	state->ARWL = h->ARWL;
	state->PRWL = h->PRWL;
	state->k_active = h->k_active;
	state->k_passive = h->k_passive;
	state->max_active = h->max_active;
	state->max_passive = h->max_passive;

	state->self = malloc(sizeof(peer));
	getIpAddr(&state->self->ip);

	state->active_view = ordered_list_init((compare_function)compare_peer_inverted_score);
	state->passive_view = list_init();
	state->pending = list_init();

	state->active_shuffles = list_init();

	state->original_backoff_s = h->backoff_s;
	state->original_backoff_ns = h->backoff_ns;

	state->shuffle_timeout = state->PRWL + state->ARWL; //TODO find something more adequate

	state->backoff_s = state->original_backoff_s;
	state->backoff_ns = state->original_backoff_ns;

	state->proto_id = PROTO_XBOT;

	state->passive_scan_lenght = x->passive_scan_length;
	state->unbiased_neighs = x->unbiased_neighs;
	state->oracle_id = x->oracle_id;
	state->oracle_max = x->oracle_max_val;
	state->oracle_min = x->oracle_min_val;

	state->waiting = 0;
	state->optimizing = list_init();

	state->opt_threshold = x->optimization_threshold;

	//I am not the contact of myself
	if(!equal_peer_addr(state->self, &h->contact)) {
		peer* contact = create_peer(h->contact.addr, h->contact.port);

		list_add_item_to_head(state->pending, contact);

		//assume that contact is already up
		send_join(contact, state->self, state->proto_id);
	}

	//no destroy for now
	proto_def* xbot = create_protocol_definition(state->proto_id, "Xbot", state, NULL);

	proto_def_add_protocol_main_loop(xbot, (Proto_main_loop) xbot_main_loop);

	proto_def_add_consumed_event(xbot, PROTO_DISPATCH, TCP_DISPATCHER_CONNECTION_DOWN);
	proto_def_add_consumed_event(xbot, PROTO_DISPATCH, TCP_DISPATCHER_UNABLE_TO_CONNECT);

	proto_def_add_produced_events(xbot, 2); //NEIGH UP, NEIGH DOWN


	state->shuffle = malloc(sizeof(YggTimer));
	YggTimer_init(state->shuffle, state->proto_id, state->proto_id);
	YggTimer_set(state->shuffle, h->shuffle_period_s, h->shuffle_period_ns, h->shuffle_period_s, h->shuffle_period_ns);
	setupTimer(state->shuffle);

	return xbot;

}


xbot_args* xbot_args_init(const char* contact, unsigned short contact_port, int max_active, int max_passive, short ARWL, short PRWL, short k_active, short k_passive, short shuffle_period_s, long shuffle_period_ns, short backoff_s, long backoff_ns, unsigned short passive_scan_length,unsigned short unbiased_neighs, double oracle_min_val, double oracle_max_val, unsigned short oracle_id, double optimization_threshold) {
	xbot_args* args = malloc(sizeof(xbot_args));
	args->hyparview_conf = hyparview_args_init(contact, contact_port, max_active, max_passive, ARWL, PRWL, k_active, k_passive, shuffle_period_s, shuffle_period_ns, backoff_s, backoff_ns);
	args->oracle_max_val = oracle_max_val;
	args->oracle_min_val = oracle_min_val;

	args->passive_scan_length = passive_scan_length;
	args->unbiased_neighs = unbiased_neighs;
	args->oracle_id = oracle_id;
	args->optimization_threshold = optimization_threshold;

	return args;

}

void xbot_args_destroy(xbot_args* args) {
	hyparview_args_destroy(args->hyparview_conf);
	free(args);
}
