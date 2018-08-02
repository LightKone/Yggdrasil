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

#include "reliable.h"

#define TIME_TO_LIVE 5
#define TIME_TO_RESEND 2
#define TIMER 2 //2 seconds

#define THRESHOLD 5 //five times resend

#define PANIC -1
#define DUPLICATE 0
#define OK 1

typedef enum types_{
	MSG,
	ACK
}types;

typedef struct _pending_msg{
	unsigned short sqn;
	YggMessage* msg;
	unsigned short resend;
	time_t time;
}pending_msg;

typedef struct _destination{
	time_t first;
	WLANAddr destination;
	list* msg_list;
}destination;

typedef struct meta_info_{
	unsigned short sqn;
	unsigned short type;
}meta_info;


typedef struct _reliable_state{
	short proto_id;
	unsigned short sqn;

	list* outbound_msgs;
	list* inbound_msgs;

	WLANAddr bcastAddr;
	queue_t* dispatcher_queue;
	YggTimer* garbage_collect;
}p2p_reliable_state;

static void* serialize_meta_info(meta_info* info){
	void* buffer = malloc(sizeof(meta_info));

	memcpy(buffer, &info->sqn, sizeof(unsigned short));
	memcpy(buffer + sizeof(unsigned short), &info->type, sizeof(unsigned short));

	return buffer;
}

static void deserialize_meta_info(void* buffer, meta_info* info){
	memcpy(&info->sqn, buffer, sizeof(unsigned short));
	memcpy(&info->type, buffer + sizeof(unsigned short), sizeof(unsigned short));
}

static bool equal_destination(destination* dest, WLANAddr* addr) {
	return memcmp(dest->destination.data, addr->data, WLAN_ADDR_LEN) == 0;
}

static void store_outbond(YggMessage* msg, p2p_reliable_state* state){
	//simple store
	destination* dest = list_find_item(state->outbound_msgs, (comparator_function) equal_destination, &msg->destAddr);
	if(!dest) {
		dest = malloc(sizeof(destination));
		memcpy(dest->destination.data, msg->destAddr.data, WLAN_ADDR_LEN);
		dest->first = time(NULL);
		list_add_item_to_tail(state->outbound_msgs, dest);
		dest->msg_list = list_init();
	}


	//add msg to list
	pending_msg* m = malloc(sizeof(pending_msg));
	m->msg = malloc(sizeof(YggMessage));
	memcpy(m->msg, msg, sizeof(YggMessage));
	m->sqn = state->sqn;
	m->resend = 0;
	m->time = time(NULL);

	list_add_item_to_head(dest->msg_list, m);
}

static bool equal_sqn(pending_msg* msg, unsigned short* sqn) {
	return msg->sqn == *sqn;
}

static short store_inbond(unsigned short recv_sqn, YggMessage* msg, p2p_reliable_state* state){
	//check if it and store it

	destination* dest = list_find_item(state->inbound_msgs, (comparator_function) equal_destination, &msg->srcAddr);

	if(!dest) {
		dest = malloc(sizeof(destination));
		memcpy(dest->destination.data, msg->srcAddr.data, WLAN_ADDR_LEN);
		dest->msg_list = list_init();
		list_add_item_to_tail(state->inbound_msgs, dest);
	}

	pending_msg* m = list_find_item(dest->msg_list, (comparator_function) equal_sqn, &recv_sqn);
	if(m){
		return DUPLICATE;
	}else {
		m = malloc(sizeof(pending_msg));
		m->sqn = recv_sqn;
		m->resend = 0;
		m->msg = NULL;
		m->time = time(NULL);
		list_add_item_to_tail(dest->msg_list, m);
	}

	return OK;

}

static short rm_outbond(unsigned short ack_sqn, WLANAddr* addr, p2p_reliable_state* state){

	destination* dest = list_find_item(state->outbound_msgs, (comparator_function) equal_destination, addr);

	if(dest == NULL || dest->msg_list->size == 0){
		return PANIC;
	}

	pending_msg* msg = list_remove_item(dest->msg_list, (comparator_function) equal_sqn, &ack_sqn);

	if(msg) {
		free(msg->msg);
		free(msg);
		return OK;
	}

	return PANIC;
}

static short rm_inbond(p2p_reliable_state* state){
	list_item* it = state->inbound_msgs->head;
	while(it != NULL){
		destination* dest = it->data;
		if(dest->first != 0 &&  dest->first + TIME_TO_LIVE <  time(NULL)){
			list_item* it2 = dest->msg_list->head;
			while(it2 != NULL) {
				pending_msg* m = it2->data;
				if(m->time + TIME_TO_LIVE < time(NULL)){
					pending_msg* torm = list_remove_head(dest->msg_list);
					if(dest->msg_list->head != NULL) {
						m = dest->msg_list->head->data;
						dest->first = m->time;
					}
					else{
						dest->first = 0;
					}

					free(torm);
				} else
					break;
			}
		}

		it = it->next;
	}

	return OK;
}

static void prepareAck(YggMessage* msg, unsigned short recv_sqn, WLANAddr* dest, p2p_reliable_state* state){
	YggMessage_init(msg, dest->data, state->proto_id);

	meta_info info;
	info.sqn = recv_sqn;
	info.type = ACK;

	void* buffer = serialize_meta_info(&info);

	pushPayload(msg, (char *) buffer, sizeof(meta_info), state->proto_id, dest);

	free(buffer);
}


static void notify_failed_delivery(YggMessage* msg, p2p_reliable_state* state){

	void* buffer = malloc(sizeof(meta_info));
	popPayload(msg, (char *) buffer, sizeof(meta_info));
	free(buffer);

	YggEvent ev;
	YggEvent_init(&ev, state->proto_id, FAILED_DELIVERY);
	YggEvent_addPayload(&ev, msg, sizeof(YggMessage));

	deliverEvent(&ev);

	YggEvent_freePayload(&ev);
}

static void * reliable_point2point_main_loop(main_loop_args* args){

	queue_t* inBox = args->inBox;
	p2p_reliable_state* state = args->state;

	while(1){
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_MESSAGE) {

			if(elem.data.msg.Proto_id != state->proto_id){
				//message from some protocol
				if(memcmp(elem.data.msg.destAddr.data, state->bcastAddr.data, WLAN_ADDR_LEN) != 0){
					//only keep track of point to point messages
					meta_info info;
					info.sqn = state->sqn;
					info.type = MSG;

					void* buffer = malloc(sizeof(meta_info));
					memcpy(buffer, &info.sqn, sizeof(unsigned short));
					memcpy(buffer + sizeof(unsigned short), &info.type, sizeof(unsigned short));

					pushPayload(&elem.data.msg,(char *) buffer, sizeof(meta_info), state->proto_id, &elem.data.msg.destAddr);

					store_outbond(&elem.data.msg, state);

					state->sqn ++;

					free(buffer);

				}

				queue_push(state->dispatcher_queue, &elem);

			}else{
				//message from the network

				void* buffer = malloc(sizeof(meta_info));

				popPayload(&elem.data.msg, (char *) buffer, sizeof(meta_info));

				meta_info info;

				deserialize_meta_info(buffer, &info);

				free(buffer);

				if(info.type == ACK){
					//check if is ack, and rm from outbond

					if(rm_outbond(info.sqn, &elem.data.msg.srcAddr, state) == PANIC){
						ygg_log("RELIABLE POINT2POINT", "PANIC", "Tried to remove non existing out bound message");
					}

				}else if(info.type == MSG){
					//process msg
					if(store_inbond(info.sqn, &elem.data.msg, state) == OK){
						deliver(&elem.data.msg);
					}

					prepareAck(&elem.data.msg, info.sqn, &elem.data.msg.srcAddr, state);

					queue_push(state->dispatcher_queue, &elem);
				}else{
					ygg_log("RELIABLE POINT2POINT", "WARNING", "Unknown message type");
				}

			}

		}else if(elem.type == YGG_TIMER) {

			//resend everything that is pending confirmation
			list_item* it = state->outbound_msgs->head;

			while(it != NULL){
				destination* dest  = it->data;
				list_item* it2 = dest->msg_list->head;
				list_item* prev = NULL;
				while(it2 != NULL){
					pending_msg* m = it2->data;
					if(m->resend > THRESHOLD){

						if(prev == NULL)
							m = list_remove_head(dest->msg_list);
						else {
							m = list_remove(dest->msg_list, prev);
							if(prev == dest->msg_list->tail)
								it2 = NULL;
							else
								it2 = prev->next;
						}

						notify_failed_delivery(m->msg, state);
						free(m->msg);
						free(m);
					}else if(m->time + TIME_TO_RESEND < time(NULL)){

						elem.type = YGG_MESSAGE;
						memcpy(&elem.data.msg, m->msg, sizeof(YggMessage));

						queue_push(state->dispatcher_queue, &elem);

						m->resend ++;
						m->time = time(NULL);

						prev = it2;
						it2 = it2->next;
					}else{
						prev = it2;
						it2 = it2->next;
					}
				}

				it = it->next;
			}

			//get some memory
			rm_inbond(state);

		}else{
			ygg_log("RELIABLE POINT2POINT", "WARNING", "Got something unexpected");
		}

		free_elem_payload(&elem);
	}

	return NULL;
}

static void destroy_msgs(list* msgs) {
	while(msgs->size > 0) {
		destination* dest = list_remove_head(msgs);
		while(dest->msg_list->size > 0) {
			pending_msg* m = list_remove_head(dest->msg_list);
			free(m->msg);
			free(m);
		}
		free(dest->msg_list);
		free(dest);
	}
}

static void reliable_p2p_destroy(p2p_reliable_state* state) {
	if(state->garbage_collect) {
		cancelTimer(state->garbage_collect);
		free(state->garbage_collect);
	}
	if(state->inbound_msgs) {
		destroy_msgs(state->inbound_msgs);
		free(state->inbound_msgs);
	}
	if(state->outbound_msgs) {
		destroy_msgs(state->outbound_msgs);
		free(state->outbound_msgs);
	}
	state->dispatcher_queue = NULL;
	free(state);
}

proto_def* reliable_point2point_init(void* args) {
	p2p_reliable_state* state = malloc(sizeof(p2p_reliable_state));
	state->inbound_msgs = list_init();
	state->outbound_msgs = list_init();
	state->proto_id = PROTO_P2P_RELIABLE_DELIVERY;
	state->dispatcher_queue = interceptProtocolQueue(PROTO_DISPATCH, state->proto_id);

	setBcastAddr(&state->bcastAddr);
	state->sqn = 1;

	proto_def* p2p_reliable = create_protocol_definition(state->proto_id, "Reliable P2P delivery", state, (Proto_destroy) reliable_p2p_destroy);
	proto_def_add_protocol_main_loop(p2p_reliable, (Proto_main_loop) reliable_point2point_main_loop);
	proto_def_add_produced_events(p2p_reliable, 1);

	state->garbage_collect = malloc(sizeof(YggTimer));
	YggTimer_init(state->garbage_collect, state->proto_id, state->proto_id);
	YggTimer_set(state->garbage_collect, TIMER, 0, TIMER, 0);
	setupTimer(state->garbage_collect);

	return p2p_reliable;

}


