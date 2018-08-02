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

#include "LiMoSense.h"

static const double q = 1/24;

typedef struct _estimation{
	double val;
	double weight;
}est;

typedef struct _neighbour_info{
	est sent;
	est recved;
}nei_info;

typedef struct _limosense_state{
	short proto_id;
	short discovery_id;
	uuid_t myid;

	est my_est;
	est unrecved;
	double prev_val;

	list* neighbours;
	YggTimer* timer;
}limosense_state;

static void sum_est(est* est1, est* est2, est* res){
#ifdef DEBUG
	char s[500];
	bzero(s, 500);
	sprintf(s, "res->val = ((%f * %f) + (%f * %f)) / (%f + %f)   ", est1->val, est1->weight, est2->val, est2->weight, est1->weight, est2->weight);
	ygg_log("LIMOSENSE","SUM EST",s);
#endif
	if(est1->weight + est2->weight != 0){
		res->val = (((est1->val*est1->weight) + (est2->val*est2->weight)) / (est1->weight + est2->weight));
		res->weight = est1->weight + est2->weight;
	} else{
		ygg_log("LIMOSENSE", "ERROR", "Attempt to divide by zero");
	}

}

static void subtract_est(est* est1, est* est2, est* res){
	est est3;
	est3.val = est2->val;
	est3.weight = est2->weight*(-1);
	sum_est(est1, &est3, res);

}


static void process_msg(YggMessage* msg, limosense_state* state) {
	uuid_t n;
	est in_est;

	void* ptr = YggMessage_readPayload(msg, NULL, n, sizeof(uuid_t));
	ptr = YggMessage_readPayload(msg, ptr, &in_est.val, sizeof(double));
	ptr = YggMessage_readPayload(msg, ptr, &in_est.weight, sizeof(double));

	neighbour_item* neighbour = neighbour_find(state->neighbours, n);
	if(neighbour == NULL){
		nei_info empty_est;
		empty_est.sent.val = 0;
		empty_est.sent.weight = 0;
		empty_est.recved.val = 0;
		empty_est.recved.weight = 0;
		neighbour = new_neighbour(n, msg->srcAddr, &empty_est, sizeof(nei_info), NULL);
		neighbour_add_to_list(state->neighbours, neighbour);
	}

	est diff;
	subtract_est(&in_est, &((nei_info*)neighbour->attribute)->recved, &diff);
	sum_est(&state->my_est, &diff, &state->my_est);
	((nei_info*)neighbour->attribute)->recved.val = in_est.val;
	((nei_info*)neighbour->attribute)->recved.weight = in_est.weight;

}

static neighbour_item* choose_random(limosense_state* state){
	if(state->neighbours->size <= 0)
		return NULL;

	int n = rand() % state->neighbours->size;
	list_item* it = state->neighbours->head;
	while(it != NULL && n > 0){
		it = it->next;
		n--;
	}

	return it->data;
}

static void process_timer(YggTimer* timer, limosense_state* state) {
	neighbour_item* tosend = choose_random(state);
	if(tosend == NULL){
		return;
	}

	if(state->my_est.weight >= 2*q){
		double weight_tmp;
		if(state->unrecved.weight < state->my_est.weight - q){
			weight_tmp = state->unrecved.weight;
		}else{
			weight_tmp = state->my_est.weight - q;
		}
		est tmp_est;
		tmp_est.val = state->unrecved.val;
		tmp_est.weight = weight_tmp;
		subtract_est(&state->my_est, &tmp_est, &state->my_est);

		state->unrecved.weight = state->unrecved.weight - weight_tmp;

	}
	if(state->my_est.weight >= 2*q){
		est tmp_est;
		tmp_est.val = state->my_est.val;
		tmp_est.weight = (state->my_est.weight / 2);
		sum_est(&((nei_info*)tosend->attribute)->sent, &tmp_est, &((nei_info*)tosend->attribute)->sent);

		state->my_est.weight = tmp_est.weight;
	}

	YggMessage msg;
	YggMessage_init(&msg, tosend->addr.data, state->proto_id);

	YggMessage_addPayload(&msg, (void*)state->myid, sizeof(uuid_t));

	YggMessage_addPayload(&msg, (void*)&((nei_info*)tosend->attribute)->sent.val, sizeof(double)); //sent_i(j)
	YggMessage_addPayload(&msg, (void*)&((nei_info*)tosend->attribute)->sent.weight, sizeof(double));

	dispatch(&msg);

}

static void change_val(double newval, limosense_state* state){
	state->my_est.val = state->my_est.val + (1/state->my_est.weight)*(newval - state->prev_val);
	state->prev_val = newval;
}

static void process_event(YggEvent* event, limosense_state* state) {
	if(event->proto_origin == state->discovery_id){
		//topology changes
		if(event->notification_id == NEIGHBOUR_UP){
			//on addneighbor
			uuid_t n;
			WLANAddr addr;
			void* ptr = YggEvent_readPayload(event, NULL, n, sizeof(uuid_t));
			YggEvent_readPayload(event, ptr, addr.data, WLAN_ADDR_LEN);

			if(!neighbour_find(state->neighbours, n)) {
				nei_info empty_est;
				empty_est.sent.val = 0;
				empty_est.sent.weight = 0;
				empty_est.recved.val = 0;
				empty_est.recved.weight = 0;
				neighbour_item* nei = new_neighbour(n, addr, &empty_est, sizeof(nei_info), NULL);
				neighbour_add_to_list(state->neighbours, nei);
			}

		}else if(event->notification_id == NEIGHBOUR_DOWN){
			//on remneighbor
			uuid_t n;
			//extract n from event payload
			YggEvent_readPayload(event, NULL, n, sizeof(uuid_t));
			neighbour_item* torm = list_remove_item(state->neighbours, equal_neigh_uuid, n);
			if(torm != NULL){
				sum_est(&state->my_est, &((nei_info*)torm->attribute)->sent, &state->my_est);
				sum_est(&state->unrecved, &((nei_info*)torm->attribute)->recved, &state->unrecved);
				neighbour_item_destroy(torm);
			}

		}else {
			//WARNING
		}

	} else if(event->proto_origin == ANY_PROTO) {
		if(event->notification_id == VALUE_CHANGE) {
			double new_val;
			YggEvent_readPayload(event, NULL, &new_val, sizeof(double));
			change_val(new_val, state);

		}
	}
}

static void process_request(YggRequest* request, limosense_state* state) {
	if(request->request == REQUEST){
		//got a request to do something
		if(request->request_type == AGG_GET){
			//what is the estimated value?
			YggRequest_freePayload(request);
			short proto_dest = request->proto_origin;
			YggRequest_init(request, state->proto_id, proto_dest, REPLY, AGG_GET);
			YggRequest_addPayload(request, &state->prev_val, sizeof(double));
			YggRequest_addPayload(request, &state->my_est.val, sizeof(double));

			deliverReply(request);

			YggRequest_freePayload(request);
		}else{
			ygg_log("MULTI ROOT", "WARNING", "Unknown request type");
		}

	}else if(request->request == REPLY){
		ygg_log("MULTI ROOT", "WARNING", "YGG Request, got an unexpected reply");
	}else{
		ygg_log("MULTI ROOT", "WARNING", "YGG Request, not a request nor a reply");
	}
}

static void limosense_main_loop(main_loop_args* args) {
	queue_t* inBox = args->inBox;
	limosense_state* state = args->state;
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
		default:
			break;
		}

		free_elem_payload(&elem);
	}
}


static void limosense_destroy(limosense_state* state) {
	if(state->timer) {
		cancelTimer(state->timer);
		free(state->timer);
	}
	if(state->neighbours) {
		neighbour_list_destroy(state->neighbours);
		free(state->neighbours);
	}
	free(state);
}

proto_def* limosense_init(void* args) {
	limosense_args* largs = (limosense_args*) args;
	limosense_state* state = malloc(sizeof(limosense_state));
	state->discovery_id = largs->discovery_id;
	state->proto_id = PROTO_AGG_LIMOSENSE;
	state->my_est.weight = 1;
	get_initial_input_value(largs->src, &state->my_est.val);
	state->prev_val = state->my_est.val;
	state->unrecved.val = 0;
	state->unrecved.weight = 0;

	state->neighbours = list_init();
	getmyId(state->myid);

	state->timer = malloc(sizeof(YggTimer));
	YggTimer_init(state->timer, state->proto_id, state->proto_id);
	YggTimer_set(state->timer, largs->beacon_period_s, largs->beacon_period_ns, largs->beacon_period_s, largs->beacon_period_ns);

	proto_def* limosense = create_protocol_definition(state->proto_id, "LiMoSense", state, (Proto_destroy) limosense_destroy);
	proto_def_add_consumed_event(limosense, state->discovery_id, NEIGHBOUR_UP);
	proto_def_add_consumed_event(limosense, state->discovery_id, NEIGHBOUR_DOWN);
	proto_def_add_consumed_event(limosense, ANY_PROTO, VALUE_CHANGE);

	if(largs->run_on_executor) {
		proto_def_add_msg_handler(limosense, (YggMessage_handler) process_msg);
		proto_def_add_timer_handler(limosense, (YggTimer_handler) process_timer);
		proto_def_add_event_handler(limosense, (YggEvent_handler) process_event);
		proto_def_add_request_handler(limosense, (YggRequest_handler) process_request);
	}else {
		proto_def_add_protocol_main_loop(limosense, (Proto_main_loop) limosense_main_loop);
	}

	setupTimer(state->timer);
	return limosense;
}

limosense_args* limosense_args_init(short discovery_id, bool run_on_executor, agg_value_src src, time_t beacon_period_s, unsigned long beacon_period_ns) {
	limosense_args* args = malloc(sizeof(limosense_args));
	args->discovery_id = discovery_id;
	args->run_on_executor = run_on_executor;
	args->src = src;
	args->beacon_period_s = beacon_period_s;
	args->beacon_period_ns = beacon_period_ns;
	return args;
}
void limosense_args_destroy(limosense_args* args) {
	free(args);
}

