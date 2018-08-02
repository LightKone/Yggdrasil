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

#include "MiRAge.h"

typedef enum neigh_state_{
	ACTIVE,
	PASSIVE
}neigh_state;

typedef struct _tree_info{
	uuid_t tree;
	time_t last_seen;
}tree_info;

typedef struct _neighbour_info{
	uuid_t id;
	uuid_t tree_id;
	//avg_val v;
	void* value;
	short value_size;
	unsigned short level;
	neigh_state status;

}neighbour_info;

typedef struct _mirage_state {
	short proto_id;
	short fault_detector_id;

	list* trees_seen;
	tree_info* my_tree;

	list* neighbours;
	neighbour_info* self;
	neighbour_info* parent;

	void* result;

	aggregation_function aggregate;
	aggregation_function disaggregate;

	agg_op operation;
	agg_value_src src;

	YggTimer* beacon;
}mirage_state;

static tree_info* create_tree(uuid_t tree_id, time_t timestamp) {
	tree_info* tree = malloc(sizeof(tree_info));
	tree->last_seen = timestamp;
	memcpy(tree->tree, tree_id, sizeof(uuid_t));
	return tree;
}

static void destroy_tree(tree_info* tree) {
	free(tree);
}

static void update_val(mirage_state* state) {

	void* newval = malloc(state->self->value_size);
	memcpy(newval, state->self->value, state->self->value_size);

	list_item* it = state->neighbours->head;
	while(it != NULL){
		neighbour_info* info = (neighbour_info*) it->data;
		if(info->status == ACTIVE){
			state->aggregate(newval, info->value, newval);
		}

		it = it->next;
	}

	free(state->result);
	state->result = newval;

}

static bool equal_id(uuid_t id_1, uuid_t id_2) {
	return uuid_compare(id_1, id_2) == 0;
}

static void update_value_to_send(neighbour_info* nei, void* tosend, mirage_state* state) {
	list_item* it = state->neighbours->head;
	memcpy(tosend, state->self->value, state->self->value_size);

	while(it != NULL){
		neighbour_info* info = (neighbour_info*) it->data;
		if(info->status == ACTIVE && !equal_id(nei->id, info->id)){
			state->aggregate(tosend, info->value, tosend);

		}

		it = it->next;
	}

}

static void process_timer(YggTimer* beacon, mirage_state* state) {
	update_val(state);

	YggMessage msg;
	YggMessage_initBcast(&msg, state->proto_id);

	if(equal_id(state->self->id, state->my_tree->tree)) {
		state->my_tree->last_seen = time(NULL);
	}

	//Tree ids
	YggMessage_addPayload(&msg, (void*) state->my_tree->tree, sizeof(uuid_t));
	YggMessage_addPayload(&msg, (void*) &state->my_tree->last_seen, sizeof(time_t));
	YggMessage_addPayload(&msg, (void*) &state->self->level, sizeof(unsigned short));
	YggMessage_addPayload(&msg, (void*) state->parent->id, sizeof(uuid_t));

	//my values
	YggMessage_addPayload(&msg, (void*) state->self->id, sizeof(uuid_t));
	YggMessage_addPayload(&msg, (void*) state->self->value, state->self->value_size);

	//neighs offset to me
	list_item* it = state->neighbours->head;
	void* tosend = malloc(state->self->value_size);
	while(it != NULL){
		neighbour_info* info = (neighbour_info*) it->data;
		if(info->status == ACTIVE) {
			YggMessage_addPayload(&msg, (void*) info->id, sizeof(uuid_t));
			//TODO max min != sum avg
			if(state->operation == OP_MAX || state->operation == OP_MIN)
				update_value_to_send(info, tosend, state);
			else
				state->disaggregate(state->result, info->value, tosend);

			YggMessage_addPayload(&msg, (void*) tosend, state->self->value_size);

		}
		it = it->next;
	}

	dispatch(&msg);

	free(tosend);

}

static neighbour_info* create_neighbour(uuid_t nei_id, uuid_t nei_tree, unsigned short lvl, void* nei_val){
	neighbour_info* newnei = malloc(sizeof(neighbour_info));
	newnei->status = PASSIVE;
	newnei->value = nei_val;
	newnei->level = lvl;
	memcpy(newnei->id, nei_id, sizeof(uuid_t));
	memcpy(newnei->tree_id, nei_tree, sizeof(uuid_t));

	return newnei;
}

static bool equal_neighbour_id(neighbour_info* info, uuid_t id) {
	return uuid_compare(info->id, id) == 0;
}

static bool equal_tree_id(tree_info* info, uuid_t id) {
	return uuid_compare(info->tree, id) == 0;
}

static void update_entry(uuid_t nei_id, uuid_t nei_tree, unsigned short nei_lvl, void* nei_val, mirage_state* state){

	neighbour_info* nei = list_find_item(state->neighbours, (comparator_function) equal_neighbour_id, nei_id);
	if(nei == NULL){
		nei = create_neighbour(nei_id, nei_tree, nei_lvl, nei_val);
		list_add_item_to_head(state->neighbours, nei);
	}else{
		nei->level = nei_lvl;
		free(nei->value);
		nei->value = nei_val;
		memcpy(nei->tree_id, nei_tree, sizeof(uuid_t));
	}

}

static void update_tree(uuid_t nei_tree, time_t nei_time, unsigned short nei_lvl, uuid_t nei_parent, uuid_t nei_id, void* nei_val, mirage_state* state){

	int treediff = uuid_compare(nei_tree, state->my_tree->tree);

	neighbour_info* nei = list_find_item(state->neighbours, (comparator_function) equal_neighbour_id, nei_id);
	if(nei == NULL){
		nei = create_neighbour(nei_id, nei_tree, nei_lvl, nei_val);
		list_add_item_to_head(state->neighbours, nei);
	} else
		free(nei_val);

	if(treediff == 0){

		if(state->my_tree->last_seen < nei_time)
			state->my_tree->last_seen = nei_time;

	}

	if(equal_id(nei_parent, state->self->id)){

		if(nei != state->parent){
			if(treediff == 0){
				//all good, we are in the same tree
				nei->status = ACTIVE;

			}else{
				//we are not in the same tree, what is the correct tree?
				//if he thinks I am his parent then we should be in the same tree:
			}
		}else{ //deal with cases when nei things i am his parent and I think he is my parent
			//force reconvergence
			state->my_tree = list_find_item(state->trees_seen, (comparator_function) equal_tree_id, state->self->id);
			if(state->my_tree == NULL){
				state->my_tree = create_tree(state->self->id, time(NULL));
				list_add_item_to_head(state->trees_seen, state->my_tree);
			}

			state->self->level = 0;
			state->parent = state->self;
		}
	}else if(nei == state->parent){
		//he is my parent
		if(treediff == 0){
			//all good, we are in the same tree
			nei->status = ACTIVE;
			state->self->level = state->parent->level + 1;

		}else{
			//we are not in the same tree, what is the correct tree?
			//my parent should be correct
			if(uuid_compare(nei_tree, state->self->id) < 0){
				state->my_tree = list_find_item(state->trees_seen, (comparator_function) equal_tree_id, nei_tree);
				if(state->my_tree == NULL){
					state->my_tree = create_tree(nei_tree, nei_time);
					list_add_item_to_head(state->trees_seen, state->my_tree);
				}

				nei->status = ACTIVE;
				state->self->level = state->parent->level + 1;

			}else{
				state->my_tree = list_find_item(state->trees_seen, (comparator_function) equal_tree_id, state->self->id);
				if(state->my_tree == NULL){
					state->my_tree = create_tree(state->self->id, time(NULL));
					list_add_item_to_head(state->trees_seen, state->my_tree);
				}
				state->self->level = 0;
				state->parent = state->self;
			}

		}
	}else{
		//he is my peer
		if(treediff >= 0)
			nei->status = PASSIVE;
		else if(treediff < 0){
			tree_info* tree = list_find_item(state->trees_seen, (comparator_function) equal_tree_id, nei_tree);
			if(tree == NULL){
				state->my_tree = create_tree(nei_tree, nei_time);
				list_add_item_to_head(state->trees_seen, state->my_tree);
				nei->status = ACTIVE;
				state->parent = nei;
				state->self->level = state->parent->level + 1;

			}else{

				if(tree->last_seen < nei_time){
					tree->last_seen = nei_time;
					state->my_tree = tree;
					nei->status = ACTIVE;
					state->parent = nei;
					state->self->level = state->parent->level + 1;

				}else{
					nei->status = PASSIVE;
				}
			}

		}

	}

	list_item* it = state->neighbours->head;
	while(it != NULL){
		neighbour_info* info = (neighbour_info*) it->data;
		if(info->status == PASSIVE && equal_id(info->tree_id, state->my_tree->tree) && info->level < state->parent->level){
			state->parent->status = PASSIVE;
			state->parent = info;

			state->self->level = state->parent->level + 1;
			state->parent->status = ACTIVE;
		}

		it = it->next;
	}

}

static void process_msg(YggMessage* msg, mirage_state* state) {

	unsigned short payload_len = msg->dataLen;

	//tree id of sender
	uuid_t other_tree;
	time_t other_time;
	unsigned short other_lvl;
	uuid_t other_parent;

	void* ptr = YggMessage_readPayload(msg, NULL, (void*) other_tree, sizeof(uuid_t));
	ptr = YggMessage_readPayload(msg, ptr, (void*) &other_time, sizeof(time_t));
	ptr = YggMessage_readPayload(msg, ptr, (void*) &other_lvl, sizeof(unsigned short));
	ptr = YggMessage_readPayload(msg, ptr, (void*) other_parent, sizeof(uuid_t));

	//values of sender
	uuid_t other_node_id;
	void* other_val = malloc(state->self->value_size);

	ptr = YggMessage_readPayload(msg, ptr, (void*) other_node_id, sizeof(uuid_t));
	ptr = YggMessage_readPayload(msg, ptr, other_val, state->self->value_size);


	payload_len -= ((sizeof(uuid_t)*3) + sizeof(time_t) + sizeof(unsigned short) + state->self->value_size);
	void* payload = ptr;

	while(payload_len >= (sizeof(uuid_t) + state->self->value_size)){

		uuid_t nei;
		memcpy(nei, payload, sizeof(uuid_t));
		payload += sizeof(uuid_t);
		if(equal_id(nei, state->self->id)){
			void* nei_val = malloc(state->self->value_size);
			memcpy(nei_val, payload, state->self->value_size);

			update_entry(other_node_id, other_tree, other_lvl, nei_val, state);
			break;
		}
		payload += state->self->value_size;
		payload_len -= (sizeof(uuid_t) + state->self->value_size);
	}

	update_tree(other_tree, other_time, other_lvl, other_parent, other_node_id, other_val, state);
}

static void destroy_neighbour(neighbour_info* neighbour) {
	free(neighbour->value);
	free(neighbour);
}

static void process_neighbour_down(uuid_t torm, mirage_state* state) {

	neighbour_info* neighbour = list_remove_item(state->neighbours, (comparator_function) equal_neighbour_id, torm);
	if(state->parent == neighbour){
		list_item* it = state->neighbours->head;
		neighbour_info* candidate = state->self;
		while(it != NULL){
			neighbour_info* info = (neighbour_info*)it->data;
			if(info->status == PASSIVE && equal_id(state->my_tree->tree, info->tree_id)){
				if(info->level < state->self->level){ //TODO it->level < candidate->lvl
					candidate = info;
				}
			}
			it = it->next;
		}

		state->parent = candidate;
		state->parent->status = ACTIVE;
		if(state->parent == state->self){
			state->my_tree = list_find_item(state->trees_seen, (comparator_function) equal_tree_id, state->self->id);
			if(state->my_tree == NULL){
				state->my_tree = create_tree(state->self->id, time(NULL));
				list_add_item_to_head(state->trees_seen, state->my_tree);
			}
			state->self->level = 0;
		}else{
			state->self->level = state->parent->level + 1;
		}

		destroy_neighbour(neighbour);
	}
}

static void process_event(YggEvent* event, mirage_state* state) {
	if(event->proto_origin == state->fault_detector_id){
		if(event->notification_id == NEIGHBOUR_DOWN){ //TODO Update discoveries to use these names
			uuid_t torm;
			YggEvent_readPayload(event, NULL, torm, sizeof(uuid_t));
			//restore tree
			process_neighbour_down(torm, state);
		}
	} else if(event->proto_origin == ANY_PROTO) {
		if(event->notification_id == VALUE_CHANGE) {
			YggEvent_readPayload(event, NULL, state->self->value, sizeof(double));

		}
	}
}

static void process_request(YggRequest* request, mirage_state* state) {
	if(request->request == REQUEST){
		//got a request to do something
		if(request->request_type == AGG_GET){
			//what is the estimated value?
			YggRequest_freePayload(request);
			short proto_dest = request->proto_origin;
			YggRequest_init(request, state->proto_id, proto_dest, REPLY, AGG_GET);
			YggRequest_addPayload(request, state->self->value, state->self->value_size);
			YggRequest_addPayload(request, state->result, state->self->value_size);

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

static void multi_root_main_loop(main_loop_args* args) {
	queue_t* inBox = args->inBox;
	mirage_state* state = args->state;
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
		case YGG_REQUEST:
			process_request(&elem.data.request, state);
			break;
		case YGG_EVENT:
			process_event(&elem.data.event, state);
			break;
		default:
			break;
		}

		free_elem_payload(&elem);
	}
}

static void destroy_trees(list* trees) {
	while(trees->size > 0) {
		destroy_tree(list_remove_head(trees));
	}
}

static void destroy_neighbours(list* neighbours) {
	while(neighbours->size > 0) {
		neighbour_info* info = list_remove_head(neighbours);
		destroy_neighbour(info);
	}
}

static short multi_root_destroy(void* state) {
	mirage_state* my_state = (mirage_state*) state;
	if(my_state->beacon) {
		cancelTimer(my_state->beacon);
		free(my_state->beacon);
	}
	if(my_state->result) {
		free(my_state->result);
	}


	destroy_trees(my_state->trees_seen);
	free(my_state->trees_seen);
	destroy_neighbours(my_state->neighbours);
	free(my_state->neighbours);
	destroy_neighbour(my_state->self);

	free(state);
	return SUCCESS;
}

proto_def* multi_root_init(void* args) {
	mirage_state* state = malloc(sizeof(mirage_state));
	state->proto_id = PROTO_AGG_MULTI_ROOT;

	multi_root_agg_args* margs = (multi_root_agg_args*) args;
	state->fault_detector_id = margs->fault_detector_id;

	state->self = malloc(sizeof(neighbour_info));
	getmyId(state->self->id);
	state->operation = margs->aggregation_operation;
	state->aggregate = aggregation_function_get(state->operation);
	state->disaggregate = aggregation_function_get_simetric(state->operation);

	state->self->value_size = aggregation_function_getValsize(state->operation);
	state->self->value = malloc(sizeof(state->self->value_size));
	state->src = TEST_INPUT; //To become an argument in latter versions
	get_initial_input_value_for_op(state->src, state->operation, state->self->value);
	state->self->level = 0;
	state->parent = state->self;

	state->neighbours = list_init();
	state->trees_seen = list_init();
	state->my_tree = create_tree(state->self->id, time(NULL));
	list_add_item_to_head(state->trees_seen, (void*) state->my_tree);

	state->result = malloc(state->self->value_size);
	memcpy(state->result, state->self->value, state->self->value_size);

	state->beacon = malloc(sizeof(YggTimer));
	YggTimer_init(state->beacon, state->proto_id, state->proto_id);
	YggTimer_set(state->beacon, margs->beacon_period_s, margs->beacon_period_ns, margs->beacon_period_s, margs->beacon_period_ns);

	setupTimer(state->beacon);

	proto_def* mirage = create_protocol_definition(state->proto_id, "MiRAge", state, (Proto_destroy) multi_root_destroy);
	proto_def_add_consumed_event(mirage, state->fault_detector_id, NEIGHBOUR_DOWN);
	proto_def_add_consumed_event(mirage, ANY_PROTO, VALUE_CHANGE);

	if(margs->run_on_executor) {
		proto_def_add_msg_handler(mirage, (YggMessage_handler) process_msg);
		proto_def_add_timer_handler(mirage, (YggTimer_handler) process_timer);
		proto_def_add_request_handler(mirage, (YggRequest_handler) process_request);
		proto_def_add_event_handler(mirage, (YggEvent_handler) process_event);
	} else
		proto_def_add_protocol_main_loop(mirage, (Proto_main_loop) multi_root_main_loop);

	return mirage;

}

multi_root_agg_args* multi_root_agg_args_init(short fault_detector_id, bool run_on_executor, agg_value_src src, agg_op aggregation_operation, time_t beacon_period_s, unsigned long beacon_period_ns) {
	multi_root_agg_args* args = malloc(sizeof(multi_root_agg_args));
	args->fault_detector_id = fault_detector_id;
	args->run_on_executor = run_on_executor;
	args->src = src;
	args->aggregation_operation = aggregation_operation;
	args->beacon_period_s = beacon_period_s;
	args->beacon_period_ns = beacon_period_ns;
	return args;
}
void multi_root_agg_args_destroy(multi_root_agg_args* args) {
	free(args);
}
