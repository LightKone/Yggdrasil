/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * André Rosa (af.rosa@campus.fct.unl.pt)
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#include "push_gossip.h"

#define OFF 60

typedef struct _seen_message {
	uuid_t msg_id;
	time_t rcv_time;

	//unsigned int counter;
	WLANAddr original_parent;

} seen_message;

typedef struct _pending_message {
	uuid_t uuid;
	YggMessage* message;
} p_msg;

// Global Variables
typedef struct _push_gossip_state {
	short proto_id;

	list* seen_msgs;
	list* pending_msgs;

	bool avoid_bcast_strom;
	unsigned long default_timeout;

	WLANAddr bcast_addr;

	YggTimer* gMan;
}push_gossip_state;


static bool equal_seen_msg_id(seen_message* msg, uuid_t id) {
	return uuid_compare(msg->msg_id, id) == 0;
}

static bool haveIseenThis(uuid_t uuid,  push_gossip_state* state) {
	return list_find_item(state->seen_msgs, (comparator_function) equal_seen_msg_id, uuid) != NULL;
}

static void addMessage(uuid_t uuid, push_gossip_state* state) {
	seen_message* u = malloc(sizeof(seen_message));
	memcpy(u->msg_id, uuid, sizeof(uuid_t));
	u->rcv_time = time(NULL);

	list_add_item_to_head(state->seen_msgs, u);
}


static unsigned long getDelay(push_gossip_state* state) {

	unsigned long max_delay = roundl((long double) getRandomProb() * state->default_timeout);

	return max_delay;
}

static int checkTime(seen_message* m) {
	return (m->rcv_time + OFF) - time(NULL);
}

static void garbageManExecution(push_gossip_state* state) {
	list_item* it = state->seen_msgs->head;
	while (it != NULL && checkTime(it->data) < 0) {
		seen_message* tmp = list_remove_head(state->pending_msgs);
		free(tmp);
	}
	if (it != NULL) {
		while (it->next != NULL) {
			seen_message* tmp = it->next->data;
			if (checkTime(tmp) < 0) {
				tmp = list_remove(state->pending_msgs, it);
				free(tmp);
			} else {
				it = it->next;
			}

		}
	}
}

static bool equal_pending_msg_id(p_msg* msg, uuid_t id) {
	return uuid_compare(msg->uuid, id) == 0;
}

static void dispatchPendingMessage(uuid_t msg_id, push_gossip_state* state) {

	p_msg* pending_msg = list_remove_item(state->pending_msgs, (comparator_function) equal_pending_msg_id, msg_id);
	if(pending_msg) {
		dispatch(pending_msg->message);
		free(pending_msg->message);
		free(pending_msg);
	} else {
		ygg_log("GOSSIP", "ERROR", "No message retransmitted");
	}
}


static short processYgg_Request(YggRequest* request, push_gossip_state* state) {

	if (request->request == REQUEST && request->request_type == PG_REQ) {

		uuid_t mid;
		genUUID(mid);

		YggMessage msg;
		YggMessage_initBcast(&msg, request->proto_origin);
		YggMessage_addPayload(&msg, request->payload, request->length);

		// Release memory of request payload
		YggRequest_freePayload(request);

		// Get my address
		WLANAddr parent;
		memcpy(parent.data, getMyWLANAddr()->data, WLAN_ADDR_LEN);

		addMessage(mid, state);
		deliver(&msg);

		pushPayload(&msg, (char*) mid, sizeof(uuid_t), state->proto_id, &state->bcast_addr);

		dispatch(&msg);

	} else {
		ygg_log("PUSH_GOSSIP", "WARNING", "Got unexpected event");
	}

	return SUCCESS;
}

static short processYgg_Timer(YggTimer* timer, push_gossip_state* state) {

	if (uuid_compare(state->gMan->id, timer->id) == 0) {
		garbageManExecution(state);
	} else {
#ifdef DEBUG
		ygg_log("GOSSIP", "DEBUG", "Received timer to retransmit");
#endif
		dispatchPendingMessage(timer->id, state);
	}

	return SUCCESS;
}

static short processYgg_Message(YggMessage* msg, push_gossip_state* state) {

	uuid_t uuid;
	popPayload(msg, (char*) uuid, sizeof(uuid_t));

	if (!haveIseenThis(uuid, state)) {

		addMessage(uuid, state);
		deliver(msg);

		pushPayload(msg, (char*) uuid, sizeof(uuid_t), state->proto_id, &state->bcast_addr);

		if (state->avoid_bcast_strom == 1) {

			// Set up timer
			p_msg* t = malloc(sizeof(p_msg));
			memcpy(&t->uuid, uuid, sizeof(uuid_t));
			t->message = malloc(sizeof(YggMessage));
			YggMessage_initBcast(t->message, msg->Proto_id);
			YggMessage_addPayload(t->message, msg->data, msg->dataLen);

			// Add to tail of pending
			list_add_item_to_tail(state->pending_msgs, t);

			YggTimer mt;
			YggTimer_init_with_uuid(&mt, uuid, state->proto_id, state->proto_id);
			unsigned long delay = getDelay(state) * 1000;
			unsigned long delay_s = delay / (1000*1000*1000);
			unsigned long delay_ns = delay - (delay_s * 1000 * 1000 * 1000);
			YggTimer_set(&mt, delay_s, delay_ns, 0 , 0);

#ifdef DEBUG
			ygg_log("PUSH_GOSSIP", "DEBUG", "Setting timer for later retransmission");
#endif
			setupTimer(&mt);
		} else {
			dispatch(msg);
		}
	}

	return SUCCESS;
}

static void* push_gossip_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	push_gossip_state* state = args->state;
	while (1) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if (elem.type == YGG_MESSAGE) {

			processYgg_Message(&elem.data.msg, state);

		} else if (elem.type == YGG_TIMER) {

			processYgg_Timer(&elem.data.timer, state);

		} else if (elem.type == YGG_REQUEST) {

			processYgg_Request(&elem.data.request, state);

		} else {
			ygg_log("PUSH_GOSSIP", "WARNING", "Got something unexpected");
		}
	}

	return NULL;
}

static void destroy_pending_msgs(list* pending) {
	while(pending->size > 0) {
		p_msg* p = list_remove_head(pending);
		free(p->message);
		free(p);
	}
}

static void destroy_seen_msgs(list* seen) {
	while(seen->size > 0) {
		seen_message* msg = list_remove_head(seen);
		free(msg);
	}
}

static void push_gossip_destroy(push_gossip_state* state) {
	if(state->gMan) {
		cancelTimer(state->gMan);
		free(state->gMan);
	}
	if(state->pending_msgs) {
		destroy_pending_msgs(state->pending_msgs);
		free(state->pending_msgs);
	}
	if(state->seen_msgs) {
		destroy_seen_msgs(state->seen_msgs);
		free(state->seen_msgs);
	}
	free(state);
}

proto_def* push_gossip_init(void* args) {

	push_gossip_args* myArgs = (push_gossip_args*) args;
	push_gossip_state* state = malloc(sizeof(push_gossip_state));
	state->seen_msgs = list_init();
	state->pending_msgs = list_init();

	state->proto_id = PROTO_PGOSSIP_BCAST;
	state->default_timeout = myArgs->default_timeout;
	state->avoid_bcast_strom = myArgs->avoid_BCastStorm;

	setBcastAddr(&state->bcast_addr);

	// Set up Garbage Collector
	if(myArgs->gManSchedule > 0) {
		state->gMan = malloc(sizeof(YggTimer));
		YggTimer_init(state->gMan, state->proto_id, state->proto_id);
		YggTimer_set(state->gMan, myArgs->gManSchedule, 0, myArgs->gManSchedule, 0);
		setupTimer(state->gMan);
	}

	proto_def* pg = create_protocol_definition(state->proto_id, "Push Gossip", state, (Proto_destroy) push_gossip_destroy);
	if(myArgs->run_on_executor) {
		proto_def_add_msg_handler(pg, (YggMessage_handler) processYgg_Message);
		proto_def_add_timer_handler(pg, (YggTimer_handler) processYgg_Timer);
		proto_def_add_request_handler(pg, (YggRequest_handler) processYgg_Request);
	} else
		proto_def_add_protocol_main_loop(pg, (Proto_main_loop) push_gossip_main_loop);


	return pg;
}


push_gossip_args* push_gossip_args_init(bool avoid_BCastStorm, unsigned long default_timeout, int gManSchedule, bool run_on_executor) {
	push_gossip_args* args = malloc(sizeof(push_gossip_args));
	args->avoid_BCastStorm = avoid_BCastStorm;
	args->run_on_executor = run_on_executor;
	args->default_timeout = default_timeout;
	args->gManSchedule = gManSchedule;

	return args;
}

void push_gossip_args_destroy(push_gossip_args* args) {
	free(args);
}
