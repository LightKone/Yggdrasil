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

#include "flow_updating.h"

typedef struct _flow_info {
	uuid_t neighbour_id;
	double flow_value;
	double estimation;
}flow_info;

typedef struct _flow_updating_state {
	uuid_t myid;
	short proto_id;
	short discovery_id;

	agg_value_src src;
	double input_value;
	double estimation;

	list* flows;

	YggTimer* timer;
}flow_updating_state;

static bool equal_flow_id(flow_info* neighbour, uuid_t id) {
	return uuid_compare(neighbour->neighbour_id, id) == 0;
}

static void process_request(YggRequest* request, flow_updating_state* state) {
	if(request->request == REQUEST){
		//got a request to do something
		if(request->request_type == AGG_GET){
			//what is the estimated value?
			YggRequest_freePayload(request);
			short proto_dest = request->proto_origin;
			YggRequest_init(request, state->proto_id, proto_dest, REPLY, AGG_GET);
			YggRequest_addPayload(request, (void*) &state->input_value, sizeof(double));
			YggRequest_addPayload(request, (void*) &state->estimation, sizeof(double));

			deliverReply(request);

			YggRequest_freePayload(request);
		}else{
			ygg_log("FLOW UPDATING", "WARNING", "Unknown request type");
		}

	}else if(request->request == REPLY){
		ygg_log("FLOW UPDATING", "WARNING", "YGG Request, got an unexpected reply");
	}else{
		ygg_log("FLOW UPDATING", "WARNING", "YGG Request, not a request nor a reply");
	}

}

static void process_event(YggEvent* event, flow_updating_state* state) {
	if(event->proto_origin == state->discovery_id){
		if(event->notification_id == NEIGHBOUR_UP){

			flow_info* new_flow = malloc(sizeof(flow_info));

			new_flow->flow_value = 0;
			new_flow->estimation = 0;
			YggEvent_readPayload(event, NULL, new_flow->neighbour_id, sizeof(uuid_t));

			if(!list_find_item(state->flows, (comparator_function) equal_flow_id, new_flow->neighbour_id))
				list_add_item_to_head(state->flows, new_flow);
			else
				free(new_flow);

		}else if(event->notification_id == NEIGHBOUR_DOWN){

			uuid_t torm;
			YggEvent_readPayload(event, NULL, torm, sizeof(uuid_t));

			flow_info* info = list_remove_item(state->flows, (comparator_function) equal_flow_id, torm);
			free(info);

		}else{
			ygg_log("FLOW UPDATING", "WARNING", "Discovery sent unknown event");
		}
	} else if(event->proto_origin == ANY_PROTO) {
		if(event->notification_id == VALUE_CHANGE) {
			double new_val;
			YggEvent_readPayload(event, NULL, &new_val, sizeof(double));
			state->input_value = new_val;

		}
	}
}

static void calculate_estimate(flow_updating_state* state) {
	list_item* it = state->flows->head;
	double flows = 0;
	double estimations = 0;

	while(it != NULL){
		flow_info* info = (flow_info*) it->data;
		flows += info->flow_value;
		estimations += info->estimation;

		it = it->next;
	}

	state->estimation = (((state->input_value - flows) + estimations) / (state->flows->size + 1));

	it = state->flows->head;
	while(it != NULL){
		flow_info* info = (flow_info*) it->data;
		info->flow_value = info->flow_value + (state->estimation - info->estimation);
		info->estimation = state->estimation;

		it = it->next;
	}
}

static void process_timer(YggTimer* timer, flow_updating_state* state) {
	calculate_estimate(state);

	//build message and send it
	YggMessage msg;
	YggMessage_initBcast(&msg, state->proto_id);

	//message payload
	YggMessage_addPayload(&msg, (void*) state->myid, sizeof(uuid_t));
	YggMessage_addPayload(&msg, (void*) &state->estimation, sizeof(double));

	list_item* it = state->flows->head;
	while(it != NULL){
		flow_info* info = (flow_info*) it->data;
		YggMessage_addPayload(&msg, (void*) info->neighbour_id, sizeof(uuid_t));
		YggMessage_addPayload(&msg, (void*) &info->flow_value, sizeof(double));
		it = it->next;
	}

	dispatch(&msg);
}

static void process_msg(YggMessage* msg, flow_updating_state* state) {
	uuid_t sender_id;
	double sender_estimation;
	void* ptr = YggMessage_readPayload(msg, NULL, sender_id, sizeof(uuid_t));
	ptr = YggMessage_readPayload(msg, ptr, &sender_estimation, sizeof(double));

	flow_info* info = list_find_item(state->flows, (comparator_function) equal_flow_id, sender_id);

	if(info == NULL){
		info = malloc(sizeof(flow_info));
		memcpy(info->neighbour_id, sender_id, sizeof(uuid_t));
		list_add_item_to_head(state->flows, info);
	}
	info->estimation = sender_estimation;

	unsigned short payload_len = msg->dataLen - sizeof(uuid_t) - sizeof(double);

	while(payload_len >= sizeof(uuid_t) + sizeof(double)){
		uuid_t id;
		ptr = YggMessage_readPayload(msg, ptr, id, sizeof(uuid_t));

		if(uuid_compare(id, state->myid) == 0){
			ptr = YggMessage_readPayload(msg, ptr, &info->flow_value, sizeof(double));
			info->flow_value = info->flow_value * (-1);
			break;
		}
		ptr += sizeof(double);
		payload_len -= (sizeof(uuid_t) + sizeof(double));
	}
}

static void flow_updating_main_loop(main_loop_args* args) {
	queue_t* inBox = args->inBox;
	flow_updating_state* state = (flow_updating_state*) args->state;

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

static void destroy_flows(list* flows) {
	while(flows->size > 0) {
		flow_info* info = list_remove_head(flows);
		free(info);
	}
}

static short flow_updating_destroy(flow_updating_state* state) {
	if(state->timer) {
		cancelTimer(state->timer);
		free(state->timer);
	}
	if(state->flows) {
		destroy_flows(state->flows);
		free(state->flows);
	}

	free(state);
	return SUCCESS;
}

proto_def* flow_updating_init(void* args) {
	flow_updating_args* fargs = (flow_updating_args*) args;

	flow_updating_state* state = malloc(sizeof(flow_updating_state));
	state->discovery_id = fargs->discovery_id;
	state->src = fargs->src;
	getmyId(state->myid);
	state->proto_id = PROTO_AGG_FLOW_UPDATING_ID;
	state->flows = list_init();
	get_initial_input_value(state->src, &state->input_value);
	state->timer = malloc(sizeof(YggTimer));
	YggTimer_init(state->timer, state->proto_id, state->proto_id);
	YggTimer_set(state->timer, fargs->beacon_period_s, fargs->beacon_period_ns, fargs->beacon_period_s, fargs->beacon_period_ns);
	setupTimer(state->timer);

	proto_def* flow_updating = create_protocol_definition(state->proto_id, "Flow Updating", state, (Proto_destroy) flow_updating_destroy);
	proto_def_add_consumed_event(flow_updating, state->discovery_id, NEIGHBOUR_UP);
	proto_def_add_consumed_event(flow_updating, state->discovery_id, NEIGHBOUR_DOWN);
	proto_def_add_consumed_event(flow_updating, ANY_PROTO, VALUE_CHANGE);

	if(fargs->run_on_executor) {
		proto_def_add_msg_handler(flow_updating, (YggMessage_handler) process_msg);
		proto_def_add_timer_handler(flow_updating, (YggTimer_handler) process_timer);
		proto_def_add_event_handler(flow_updating, (YggEvent_handler) process_event);
		proto_def_add_request_handler(flow_updating, (YggRequest_handler) process_request);
	} else {
		proto_def_add_protocol_main_loop(flow_updating, (Proto_main_loop) flow_updating_main_loop);
	}

	return flow_updating;
}

flow_updating_args* flow_updating_args_init(short discovery_id, bool run_on_executor, agg_value_src src, time_t beacon_period_s, unsigned long beacon_period_ns) {
	flow_updating_args* args = malloc(sizeof(flow_updating_args));
	args->discovery_id = discovery_id;
	args->run_on_executor = run_on_executor;
	args->src = src;
	args->beacon_period_s = beacon_period_s;
	args->beacon_period_ns = beacon_period_ns;
	return args;
}
void flow_updating_args_destroy(flow_updating_args* args) {
	free(args);
}
