/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#include "dispatcher.h"


typedef struct ignore_list_ {
	WLANAddr src;
	struct ignore_list_* next;
}ignore_list;


typedef struct _dispatcher_state {
	Channel* channel;
	pthread_mutex_t* ig_list_lock;
	pthread_t* receiver;
	ignore_list* ig_lst;
}dispatcher_state;

static int addToIgnoreLst(WLANAddr src, dispatcher_state* state) {
	ignore_list* tmp = state->ig_lst;

	if(tmp == NULL){
		pthread_mutex_lock(state->ig_list_lock);
		tmp = malloc(sizeof(ignore_list));
		memcpy(tmp->src.data, src.data, WLAN_ADDR_LEN);
		tmp->next = NULL;
		state->ig_lst = tmp;
		pthread_mutex_unlock(state->ig_list_lock);
		return SUCCESS;
	}

	while(tmp->next != NULL) {
		if(memcmp(tmp->next->src.data, src.data, WLAN_ADDR_LEN) == 0)
			break;
		tmp = tmp->next;
	}
	if(tmp->next == NULL) {
		ignore_list* new = malloc(sizeof(ignore_list));
		pthread_mutex_lock(state->ig_list_lock);
		memcpy(new->src.data, src.data, WLAN_ADDR_LEN);
		tmp->next = new;
		new->next = NULL;
		pthread_mutex_unlock(state->ig_list_lock);
		return SUCCESS;
	}
	return FAILED;
}

static int remFromIgnoreLst(WLANAddr src, dispatcher_state* state) {
	ignore_list* tmp = state->ig_lst;

	if(tmp == NULL)
		return FAILED;

	if(memcmp(tmp->src.data, src.data, WLAN_ADDR_LEN) == 0) {
		pthread_mutex_lock(state->ig_list_lock);
		ignore_list* old = tmp;
		state->ig_lst = tmp->next;
		old->next = NULL;
		free(old);
		pthread_mutex_unlock(state->ig_list_lock);
		return SUCCESS;
	}
	while(tmp->next != NULL) {
		if(memcmp(tmp->next->src.data, src.data, WLAN_ADDR_LEN) == 0) {
			pthread_mutex_lock(state->ig_list_lock);
			ignore_list* old = tmp->next;
			tmp->next = old->next;
			old->next = NULL;
			free(old);
			pthread_mutex_unlock(state->ig_list_lock);
			return SUCCESS;
		}
		tmp = tmp->next;
	}
	return FAILED;
}

static int ignore(WLANAddr src, ignore_list* ig_lst, pthread_mutex_t* ig_list_lock){
	ignore_list* tmp = ig_lst;
	int ig = NOT_IGNORE;
	pthread_mutex_lock(ig_list_lock);
	int i = 0;
	while(tmp != NULL){
		if(memcmp(tmp->src.data, src.data, WLAN_ADDR_LEN) == 0){
			ig = IGNORE;
			break;
		}
		tmp = tmp->next;
		i++;
	}
	pthread_mutex_unlock(ig_list_lock);

	return ig;
}


static int serializeYggMessage(YggMessage* msg, char* buffer) {

	int len = 0;

	char* tmp = buffer;

	memcpy(tmp, &msg->Proto_id, sizeof(short));
	len += sizeof(short);
	tmp += sizeof(short);

	memcpy(tmp, msg->data, msg->dataLen);
	len += msg->dataLen;

	return len;
}

static void deserializeYggMessage(YggMessage* msg, char* buffer, short bufferlen) {

	short total = bufferlen;
	char* tmp = buffer;

	memcpy(&msg->Proto_id, tmp, sizeof(short));
	total -= sizeof(short);
	tmp += sizeof(short);

	memcpy(msg->data, tmp, total);
	msg->dataLen = total;

}

static void* dispatcher_receiver(void* args) {

	dispatcher_state* state = (dispatcher_state*) args;

	YggPhyMessage phymsg;
	YggMessage msg;
	while(1){
		while(chreceive(state->channel, &phymsg) == CHANNEL_RECV_ERROR);

		if(ignore(phymsg.phyHeader.srcAddr, state->ig_lst, state->ig_list_lock) == IGNORE)
			continue;

		deserializeYggMessage(&msg, phymsg.data, phymsg.dataLen);

		msg.destAddr = phymsg.phyHeader.destAddr;
		msg.srcAddr = phymsg.phyHeader.srcAddr;
#ifdef DEBUG
		char s[2000];
		char addr[33];
		memset(addr, 0, 33);
		memset(s, 0, 2000);
		sprintf(s, "Delivering msg from %s to proto %d", wlan2asc(&msg.srcAddr, addr), msg.Proto_id);
		ygg_log("DISPACTHER-RECEIVER", "ALIVE",s);
#endif
		deliver(&msg);
	}
	return NULL;
}

void dispatcher_serializeIgReq(dispatcher_ignore ignore, WLANAddr src, YggRequest* req){
	req->payload = malloc(sizeof(dispatcher_ignore) + WLAN_ADDR_LEN);
	void * tmp = req->payload;
	int len = 0;
	memcpy(tmp, &ignore, sizeof(dispatcher_ignore));
	len += sizeof(dispatcher_ignore);
	tmp += sizeof(dispatcher_ignore);
	memcpy(tmp, src.data, WLAN_ADDR_LEN);
	len += WLAN_ADDR_LEN;

	req->length = len;
}

static void dispatcher_deserializeIgReq(dispatch_ignore_req* ig_req, YggRequest* req){

	void * payload = req->payload;
	memcpy(&ig_req->ignore, payload, sizeof(dispatcher_ignore));
	payload += sizeof(dispatcher_ignore);

	memcpy(ig_req->src.data, payload, WLAN_ADDR_LEN);

	free(req->payload);

}

static void* dispatcher_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	dispatcher_state* state = (dispatcher_state*) args->state;

	state->ig_list_lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(state->ig_list_lock, NULL);

	state->receiver = malloc(sizeof(pthread_t));
	pthread_attr_t patribute;
	pthread_attr_init(&patribute);

	pthread_create(state->receiver, &patribute, &dispatcher_receiver, (void*) state);

	queue_t_elem elem;
	while(1){

		queue_pop(inBox, &elem);
		if(elem.type == YGG_MESSAGE){

			YggPhyMessage phymsg;
			initYggPhyMessage(&phymsg);
			int len = serializeYggMessage(&elem.data.msg, phymsg.data);
			phymsg.dataLen = len;

			while(chsendTo(state->channel, &phymsg, (char*) elem.data.msg.destAddr.data) == CHANNEL_SENT_ERROR);
#ifdef DEBUG
			char s[200];
			memset(s, 0, 200);
			sprintf(s, "Message sent to network from %d with seq number %d", elem.data.msg.Proto_id, mid);
			ygg_log("DISPACTHER-SENDER", "ALIVE",s);
#endif

		} else if(elem.type == YGG_REQUEST){
			YggRequest req = elem.data.request;
			if(req.proto_dest == PROTO_DISPATCH && req.request == REQUEST && req.request_type == DISPATCH_IGNORE_REQ){
				dispatch_ignore_req ig_req;
				dispatcher_deserializeIgReq(&ig_req, &req);

				if(ig_req.ignore == IGNORE){
#ifdef DEBUG
					char s[33];
					char msg[2000];
					memset(msg, 0, 2000);
					int r = addToIgnoreLst(ig_req.src, state);
					if(r == SUCCESS){
						sprintf(msg, "successfully added %s to ignore list", wlan2asc(&ig_req.src, s));
						ygg_log("DISPATCHER", "DEBUG", msg);
					} else if(r == FAILED){
						ygg_log("DISPATCHER", "DEBUG", "failed to add to ignore list, is it already there?");
					}
#else
					addToIgnoreLst(ig_req.src, state);
#endif
				}
				else if(ig_req.ignore == NOT_IGNORE){
#ifdef DEBUG
					int r = remFromIgnoreLst(ig_req.src, state);
					if(r == SUCCESS){
						ygg_log("DISPATCHER", "DEBUG", "successfully removed from ignore list");
					} else if(r == FAILED){
						ygg_log("DISPATCHER", "DEBUG", "failed to remove from ignore list, was it there?");
					}
#else
					remFromIgnoreLst(ig_req.src, state);
#endif
				}
				else{
					ygg_log("DISPATCHER", "PANIC", "something went terribly wrong when deserializing request");
				}
			} else if(req.proto_dest == PROTO_DISPATCH && req.request == REQUEST && req.request_type == DISPATCH_SHUTDOWN) {
				if(state->receiver) {
					pthread_cancel(*state->receiver);
					free(state->receiver);
				}
				close(state->channel->sockid);
				ygg_log("DISPATCHER", "REQUEST SHUTDOWN", "Going down...");
				return NULL;
			}
		}
		else {
			//ignore
		}
	}
}

static short dispatcher_destroy(void* state) {
	dispatcher_state* disp_state = (dispatcher_state*) state;
	ignore_list* lst = disp_state->ig_lst;
	while(lst != NULL) {
		ignore_list* torm = lst;
		lst = lst->next;
		free(torm);
	}

	if(disp_state->receiver) {
		pthread_cancel(*disp_state->receiver);
		free(disp_state->receiver);
	}

	if(disp_state->ig_list_lock) {
		pthread_mutex_destroy(disp_state->ig_list_lock);
		free(disp_state->ig_list_lock);
	}

	free(disp_state);

	return SUCCESS;
}

proto_def* dispatcher_init(Channel* ch) {

	dispatcher_state* state = malloc(sizeof(dispatcher_state));
	state->channel = ch;
	state->ig_lst = NULL;
	state->receiver = NULL;
	state->ig_list_lock = NULL;


	proto_def* dispatcher = create_protocol_definition(PROTO_DISPATCH, "Dispatcher", (void*) state, dispatcher_destroy);
	proto_def_add_protocol_main_loop(dispatcher, dispatcher_main_loop);

	return dispatcher;
}
