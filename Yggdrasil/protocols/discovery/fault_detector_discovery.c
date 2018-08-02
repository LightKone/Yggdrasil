/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2018
 *********************************************************/

#include "fault_detector_discovery.h"

#define RETRY_TIME 30*60 //30 mins in s

typedef struct _fault_detector_discovery_state {
	list* blocked;
	list* quarentine;
	list* database;

	short threshold;
	unsigned long off;

	queue_t* dispatcher_queue;
	time_t announce_period_s;
	unsigned long announce_period_ns;
	unsigned short msg_lost_per_fault;
	unsigned long timeout_time;

	YggTimer* announce;
	YggTimer* gman;
	YggTimer* unblock;

	uuid_t myid;
	short proto_id;
	WLANAddr* bcastaddr;
}fdd_state;

typedef struct fault_detection_attr_ {
	time_t last_call;
	short faults;
}fault_detection_attr;

static void send_block_request(neighbour_item* nei, fdd_state* state){
	YggRequest req;
	YggRequest_init(&req, state->proto_id, PROTO_DISPATCH, REQUEST, DISPATCH_IGNORE_REQ);
	dispatcher_serializeIgReq(IGNORE, nei->addr, &req);
	deliverRequest(&req);
	YggRequest_freePayload(&req);

	char uuid_str[37];
	bzero(uuid_str,37);
	uuid_unparse(nei->id, uuid_str);
	ygg_log("FAULT DETECTOR DISCOVERY", "BLOCKED NODE", uuid_str);
}

static void send_unblock_request(neighbour_item* nei, fdd_state* state){
	YggRequest req;
	YggRequest_init(&req, state->proto_id, PROTO_DISPATCH, REQUEST, DISPATCH_IGNORE_REQ);
	dispatcher_serializeIgReq(NOT_IGNORE, nei->addr, &req);
	deliverRequest(&req);
	YggRequest_freePayload(&req);
}

static bool equal_neigh_id(neighbour_item* item, neighbour_item* to_compare) {
	return uuid_compare(item->id, to_compare->id) == 0;
}

static int check_try_again(neighbour_item* item){
	return (((fault_detection_attr*)item->attribute)->last_call + RETRY_TIME) - time(NULL);
}

static void check_blocked(fdd_state* state) {

	while(state->blocked->head != NULL && check_try_again((neighbour_item*) state->blocked->head->data) < 0){
		neighbour_item* item = list_remove_head(state->blocked);
		send_unblock_request(item, state);
		neighbour_item_destroy(item);
	}
	if(state->blocked->head != NULL){
		list_item* it = state->blocked->head;

		while(it->next != NULL){
			if(check_try_again((neighbour_item*) it->next->data) < 0){
				neighbour_item* item = list_remove(state->blocked, it);
				send_unblock_request(item, state);
				neighbour_item_destroy(item);
			}else{
				it = it->next;
			}

		}
	}
}

static void block_neigh(neighbour_item* nei, fdd_state* state) {

	if(state->blocked->size == 0){
		((fault_detection_attr*)nei->attribute)->last_call = time(NULL);
		neighbour_add_to_list(state->blocked, nei);
		send_block_request(nei, state);
	}else{
		neighbour_item* item = neighbour_find(state->blocked, nei->id);
		if(item == NULL) {
			((fault_detection_attr*)nei->attribute)->last_call = time(NULL);
			list_add_item_to_tail(state->blocked, (void*)nei);
			send_block_request(nei, state);
		} else {
			neighbour_item_destroy(nei);
		}
	}

}

static int check_time(neighbour_item* n, fdd_state* state){
	return (((fault_detection_attr*)n->attribute)->last_call + (state->off)) - time(NULL);
}

static void check_quarantine(fdd_state* state) {

	while(state->quarentine->head != NULL && check_time((neighbour_item*)state->quarentine->head->data, state) <  0){
		neighbour_item* item = list_remove_head(state->quarentine);

		if(state->threshold > 0 && ((fault_detection_attr*)item->attribute)->faults > state->threshold){
			block_neigh(item, state);
		}else{
			neighbour_item_destroy(item);
		}
	}
	if(state->quarentine->head != NULL){
		list_item* it = state->quarentine->head;

		while(it->next != NULL){
			neighbour_item* item = (neighbour_item*) it->next->data;
			if(check_time(item, state) < 0){
				item = list_remove(state->quarentine, it);

				if(state->threshold > 0 && ((fault_detection_attr*)item->attribute)->faults > state->threshold)
					block_neigh(item, state);
				else{
					neighbour_item_destroy(item);
				}
			}else{
				it = it->next;
			}
		}
	}
}

static void put_in_quarantine(neighbour_item* nei, fdd_state* state) {
	char uuid_str[37];
	bzero(uuid_str,37);
	uuid_unparse(nei->id, uuid_str);

	if(state->quarentine->size == 0){
		neighbour_add_to_list(state->quarentine, nei);
		((fault_detection_attr*)nei->attribute)->faults = ((fault_detection_attr*)nei->attribute)->faults + 1;

		ygg_log("FAULT DETECTOR DISCOVERY", "QUARANTINED NODE", uuid_str);

	}else{
		list_item* it = state->quarentine->head;
		neighbour_item* item = (neighbour_item*)it->data;
		if(equal_neigh_id(item, nei)){
			((fault_detection_attr*)item->attribute)->faults = ((fault_detection_attr*)item->attribute)->faults + 1;


			//Was duplicated in the process can be freed;
			neighbour_item_destroy(nei);

			if(state->threshold > 0 && ((fault_detection_attr*)item->attribute)->faults > state->threshold){
				neighbour_item* toBlock = list_remove_head(state->quarentine);
				block_neigh(toBlock, state);
			}
			return;
		}else{
			while(it->next != NULL){
				neighbour_item* item = (neighbour_item*)it->next->data;
				if(equal_neigh_id(item, nei)){
					//got into quarantine and back alive
					((fault_detection_attr*)item->attribute)->faults ++;

					//Was duplicated in the process can be freed;
					neighbour_item_destroy(nei);

					if(state->threshold > 0 && ((fault_detection_attr*)item->attribute)->faults > state->threshold){
						neighbour_item* toBlock = list_remove(state->quarentine, it);
						block_neigh(toBlock, state);
					}
					break;
				}
				it = it->next;
			}
		}

		if(it->next == NULL) {
			((fault_detection_attr*)nei->attribute)->faults = ((fault_detection_attr*)nei->attribute)->faults + 1;
			list_add_item_to_tail(state->quarentine, (void*)nei);

			char uuid_str[37];
			bzero(uuid_str,37);
			uuid_unparse(nei->id, uuid_str);
			ygg_log("FAULT DETECTOR DISCOVERY", "QUARANTINED NODE", uuid_str);
		}
	}
}

static void send_discovery_msg(fdd_state* state) {
	queue_t_elem msg;
	msg.type = YGG_MESSAGE;
	YggMessage_initBcast(&msg.data.msg, state->proto_id);

	pushPayload(&msg.data.msg, (char*) state->myid, sizeof(uuid_t), state->proto_id, state->bcastaddr);

	queue_push(state->dispatcher_queue, &msg);
}

static void process_announcement(YggTimer* timer, fdd_state* state) {

	send_discovery_msg(state);

	//Check which neighbors are suspected.
	if(state->timeout_time > 0){
		long now = time(NULL);

		while(state->database->head != NULL &&
				((fault_detection_attr*)((neighbour_item*)state->database->head->data)->attribute)->
				last_call + (state->timeout_time) < now) {

			neighbour_item* toRemove = (neighbour_item*)list_remove_head(state->database);

			send_event_neighbour_down(state->proto_id, toRemove->id);

			char uuid_str[37];
			bzero(uuid_str,37);
			uuid_unparse(toRemove->id, uuid_str);
			ygg_log("FAULT DETECTOR DISCOVERY", "SUSPECT NODE", uuid_str);

			if(state->quarentine)
				put_in_quarantine(toRemove, state);
			else{
				neighbour_item_destroy(toRemove);
			}
		}

		if(state->database->head != NULL) {

			list_item* it = state->database->head;
			while(it->next != NULL) {

				char uuid_str[37];
				bzero(uuid_str,37);
				//TODO: timeout_converted to seconds
				if(((fault_detection_attr*)((neighbour_item*)it->next->data)->attribute)->last_call + (state->timeout_time) < now) { //Timeout

					neighbour_item* toRemove = list_remove(state->database, it);

					send_event_neighbour_down(state->proto_id, toRemove->id);

					uuid_unparse(toRemove->id, uuid_str);
					ygg_log("FAULT DETECTOR DISCOVERY", "SUSPECT NODE", uuid_str);

					if(state->quarentine)
						put_in_quarantine(toRemove, state);
					else{
						neighbour_item_destroy(toRemove);
					}

				} else {
					it = it->next;
				}
			}
		}
	}
}

static void process_up_stream_msg(YggMessage* msg, fdd_state* state) {
	uuid_t id;
	WLANAddr addr;

	unsigned short read = popPayload(msg, (char*) id, sizeof(uuid_t));
	memcpy(addr.data, msg->srcAddr.data, WLAN_ADDR_LEN);

	if(read == sizeof(uuid_t)) {

		neighbour_item* nei = neighbour_find(state->database, id);
		if(nei == NULL) { //new nei
			fault_detection_attr fault_attr;
			fault_attr.last_call = time(NULL);
			fault_attr.faults = 0;
			nei = new_neighbour(id, addr, &fault_attr, sizeof(fault_detection_attr), NULL);
			neighbour_add_to_list(state->database, nei);

			char uuid_str[37];
			bzero(uuid_str, 37);
			uuid_unparse(nei->id, uuid_str);

			ygg_log("FAULT DETECTOR DISCOVERY", "NEW NODE", uuid_str);

			send_event_neighbour_up(state->proto_id, id, &addr);
		} else { //update
			((fault_detection_attr*)nei->attribute)->last_call = time(NULL);
		}

	}

	if(msg->Proto_id != state->proto_id) {
		filterAndDeliver(msg);
	}
}

static void process_down_stream_msg(YggMessage* msg, fdd_state* state) {
	pushPayload(msg, (char*) state->myid, sizeof(uuid_t), state->proto_id, state->bcastaddr);
}

static void* fd_discov_main_loop(main_loop_args* args) {
	fdd_state* state = args->state;
	queue_t* inBox = args->inBox;

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		YggMessage* m = NULL;
		YggTimer* t = NULL;
		switch(elem.type) {
		case YGG_MESSAGE:
			m = &elem.data.msg;
			if(m->Proto_id != state->proto_id){
				process_down_stream_msg(m, state);
				queue_push(state->dispatcher_queue, &elem);
			}
			else
				process_up_stream_msg(m, state);
			break;
		case YGG_TIMER:
			t = &elem.data.timer;
			if(state->announce && uuid_compare(t->id, state->announce->id) == 0) {
				process_announcement(t, state);
			} else if(state->gman && uuid_compare(t->id, state->gman->id) == 0) {
				check_quarantine(state);
			} else if(state->unblock && uuid_compare(t->id, state->unblock->id) == 0) {
				check_blocked(state);
			}
			break;
		default:
			queue_push(state->dispatcher_queue, &elem);
			break;
		}

		free_elem_payload(&elem);
	}

	return NULL;

}

static short fd_discov_destroy(void* state) {
	fdd_state* my_state = (fdd_state*) state;

	if(my_state->gman) {
		cancelTimer(my_state->gman);
		free(my_state->gman);
	}
	if(my_state->unblock) {
		cancelTimer(my_state->unblock);
		free(my_state->unblock);
	}
	if(my_state->quarentine) {
		neighbour_list_destroy(my_state->quarentine);
		free(my_state->quarentine);
	}
	if(my_state->blocked) {
		neighbour_list_destroy(my_state->blocked);
		free(my_state->quarentine);
	}
	if(my_state->announce) {
		cancelTimer(my_state->announce);
		free(my_state->announce);
		neighbour_list_destroy(my_state->database);
		free(my_state->database);
	}
	free(my_state->bcastaddr);
	free(state);

	return SUCCESS;
}

proto_def* fault_detector_discovery_init(void* args) {
	fault_detector_discovery_args* fdargs = (fault_detector_discovery_args*) args;
	fdd_state* state = malloc(sizeof(fdd_state));
	state->proto_id = PROTO_FAULT_DETECTOR_DISCOVERY_ID;
	state->dispatcher_queue = interceptProtocolQueue(PROTO_DISPATCH, state->proto_id);
	state->announce_period_s = fdargs->announce_period_s;
	state->announce_period_ns = fdargs->announce_period_ns;
	state->msg_lost_per_fault = fdargs->mgs_lost_per_fault;
	state->threshold = fdargs->black_list_links;
	state->timeout_time = fdargs->announce_period_s * state->msg_lost_per_fault;
	state->off = state->timeout_time * state->threshold * 2;

	proto_def* fd_discov = create_protocol_definition(state->proto_id, "Fault Detector Discovery", state, fd_discov_destroy);
	proto_def_add_protocol_main_loop(fd_discov, fd_discov_main_loop);
	proto_def_add_produced_events(fd_discov, 2); //NEIGHBOUR_UP, NEIGHBOUR_DOWN

	state->database = list_init();
	state->announce = malloc(sizeof(YggTimer));
	YggTimer_init(state->announce, state->proto_id, state->proto_id);
	YggTimer_set(state->announce, state->announce_period_s, state->announce_period_ns, state->announce_period_s, state->announce_period_ns);
	setupTimer(state->announce);

	if(state->off > 0) {
		state->quarentine = list_init();
		state->blocked = list_init();
		state->gman = malloc(sizeof(YggTimer));
		YggTimer_init(state->gman, state->proto_id, state->proto_id);
		YggTimer_set(state->gman, state->off, 0, state->off, 0);
		setupTimer(state->gman);

		state->unblock = malloc(sizeof(YggTimer));
		YggTimer_init(state->unblock, state->proto_id, state->proto_id);
		YggTimer_set(state->unblock, RETRY_TIME, 0, RETRY_TIME, 0);
		setupTimer(state->unblock);
	} else {
		state->quarentine = NULL;
		state->blocked = NULL;
		state->gman = NULL;
		state->unblock = NULL;
	}

	getmyId(state->myid);
	state->bcastaddr = getBroadcastAddr();
	return fd_discov;
}

fault_detector_discovery_args* fault_detector_discovery_args_init(time_t announce_period_s, unsigned long announce_period_ns, unsigned short mgs_lost_per_fault, unsigned short black_list_links) {
	fault_detector_discovery_args* fdargs = malloc(sizeof(fault_detector_discovery_args));
	fdargs->announce_period_s = announce_period_s;
	fdargs->announce_period_ns = announce_period_ns;
	fdargs->mgs_lost_per_fault = mgs_lost_per_fault;
	fdargs->black_list_links = black_list_links;

	return fdargs;
}

void fault_detector_discovery_args_destroy(fault_detector_discovery_args* fdargs) {
	free(fdargs);
}
