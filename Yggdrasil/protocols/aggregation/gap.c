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

#include "gap.h"

#define UND -50
#define DEFAULT_LEVEL 50

typedef enum status_{
	SELF,
	PEER,
	CHILD,
	PARENT
}status;

typedef struct _neighbour_entry{
	uuid_t neigh_id;
	WLANAddr neigh_addr;

	status st; //(self, peer, child, parent)
	int level; //distance to root
	void* weight; //value

}neigh_entry;

typedef struct _update_vector{
	uuid_t neigh;
	void* weight;
	int level;
	uuid_t parent;
}update_vector;

typedef struct _gap_state{
	short proto_id;
	short discovery_id;

	list* neighbours;
	neigh_entry* self;
	neigh_entry* parent;

	aggregation_function aggregate;
	value_size_t val_size;

	void* neutral_value;
	bool timeout;

	neigh_entry* new_neighbour;
	update_vector* vector;

	YggTimer* timer;
}gap_state;

static void destroy_neighbour(neigh_entry* entry) {
	free(entry->weight);
}

static bool equal_entry_id(neigh_entry* entry, uuid_t id) {
	return uuid_compare(entry->neigh_id, id) == 0;
}

static bool comp_vector(update_vector* v1, update_vector* v2){
	return (aggregation_valueDiff(v1->weight,v2->weight) == 0) && (v1->level == v2->level) && (uuid_compare(v1->neigh, v2->neigh) == 0) && (uuid_compare(v1->parent, v2->parent) == 0);
}

static void restoreTableInvariant(gap_state* state){

	if(state->neighbours->size > 0){

		list_item* it = state->neighbours->head;
		state->parent = (neigh_entry*) it->data;
		while(it != NULL){
			neigh_entry* entry = (neigh_entry*) it->data;
			if(entry->st == PARENT)
				entry->st = PEER;

			//parent has minimal level among other entries
			if(entry != state->self){
				if(state->parent != NULL) {
					if(entry->level < state->parent->level){
						state->parent = entry;
					}
				}else {
					state->parent = entry;
				}
			}

			it = it->next;
		}

		state->parent->st = PARENT;
		//level of parent is one less than the level of parent
		state->self->level = state->parent->level + 1;
	}
}

static void updatevector(update_vector* v, gap_state* state){
	v->level = state->self->level;
	memcpy(v->neigh, state->self->neigh_id, sizeof(uuid_t));

	if(state->parent != NULL)
		memcpy(v->parent, state->parent->neigh_id, sizeof(uuid_t));

	memcpy(v->weight, state->self->weight, state->val_size);

	list_item* it = state->neighbours->head;
	while(it != NULL){
		neigh_entry* entry = (neigh_entry*) it->data;
		if(entry->st == CHILD){
			state->aggregate(v->weight, entry->weight, v->weight);
		}

		it = it->next;
	}

	if(state->parent != NULL && state->parent->level < 0) {
		if(!state->parent->weight)
			state->parent->weight = malloc(state->val_size);
		memcpy(state->parent->weight, v->weight, state->val_size);
	}
}

static void addVectorToMsg(YggMessage* msg, update_vector* v, gap_state* state){
	YggMessage_addPayload(msg, (void*) v->neigh, sizeof(uuid_t));
	YggMessage_addPayload(msg, (void*) v->weight, state->val_size);
	YggMessage_addPayload(msg, (void*) &v->level, sizeof(int));
	if(v->parent != NULL)
		YggMessage_addPayload(msg, (void*) v->parent, sizeof(uuid_t));
}

static void send_update_vector(neigh_entry* n, update_vector* v, gap_state* state){
	//send message (update vector) to node n
	YggMessage msg;
	YggMessage_init(&msg, n->neigh_addr.data, state->proto_id);

	addVectorToMsg(&msg, v, state);

	dispatch(&msg);

}

static void broadcast_update_vector(update_vector* v, gap_state* state){
	//broadcast update vector
	YggMessage msg;
	YggMessage_initBcast(&msg, state->proto_id);

	addVectorToMsg(&msg, v, state);

	dispatch(&msg);
}

static void process_all(gap_state* state) {


	restoreTableInvariant(state);
	update_vector* newvector = malloc(sizeof(update_vector));
	newvector->weight = malloc(state->val_size);
	updatevector(newvector, state);

	if(state->new_neighbour != NULL){
		send_update_vector(state->new_neighbour, newvector, state);
		state->new_neighbour = NULL;

	}

	if((state->vector == NULL || !comp_vector(state->vector, newvector)) && state->timeout){

		broadcast_update_vector(newvector, state);
		if(state->vector != NULL){
			free(state->vector->weight);
			state->vector->weight = NULL;
			free(state->vector);
		}
		state->vector = newvector;
		newvector = NULL;
		state->timeout = false;

	}else{
		free(newvector->weight);
		newvector->weight = NULL;
		free(newvector);
		newvector = NULL;
	}
}

static void deserialize_vector(update_vector* other, YggMessage* msg, gap_state* state) {

	void* payload = msg->data;
	short len = msg->dataLen;

	memcpy(other->neigh, payload, sizeof(uuid_t));
	payload += sizeof(uuid_t);
	memcpy(other->weight, payload, state->val_size);
	payload += state->val_size;
	memcpy(&other->level, payload, sizeof(int));
	len -= (sizeof(uuid_t) + state->val_size + sizeof(int));

	if(len > 0){
		payload += sizeof(int);
		memcpy(other->parent, payload, sizeof(uuid_t));
	}
}

static void updateentry(uuid_t n, WLANAddr addr, void* w, int l, uuid_t p, gap_state* state){

	neigh_entry* entry = list_find_item(state->neighbours, (comparator_function) equal_entry_id, n);
	if(entry == NULL){
		entry = malloc(sizeof(neigh_entry));
		memcpy(entry->neigh_addr.data, addr.data, WLAN_ADDR_LEN);
		memcpy(entry->neigh_id, n, sizeof(uuid_t));
		entry->weight = malloc(state->val_size);
		entry->st = PEER;
		list_add_item_to_head(state->neighbours, entry);
	}

	entry->level = l;
	if(!entry->weight)
		entry->weight = malloc(state->val_size);
	memcpy(entry->weight, w, state->val_size);

	if(uuid_compare(p, state->self->neigh_id) == 0){
		entry->st = CHILD;
	}else if(entry->st == CHILD){
		entry->st = PEER;
	}
}

static void process_msg(YggMessage* msg, gap_state* state) {

	update_vector other;
	other.weight = malloc(state->val_size);
	deserialize_vector(&other, msg, state);
	updateentry(other.neigh, msg->srcAddr, other.weight, other.level, other.parent, state);

	free(other.weight);
	other.weight = NULL;

	process_all(state);
}

static void process_timer(YggTimer* timer, gap_state* state) {
	state->timeout = true;
	process_all(state);
}

static void process_event(YggEvent* event, gap_state* state) {

	if(event->proto_origin == state->discovery_id){

		if(event->notification_id == NEIGHBOUR_DOWN){
			//process (fail, n)
			uuid_t torm;
			YggEvent_readPayload(event, NULL, torm, sizeof(uuid_t));
			neigh_entry* entry = list_remove_item(state->neighbours, (comparator_function) equal_entry_id, torm);
			destroy_neighbour(entry);
			free(entry);

		}else if(event->notification_id == NEIGHBOUR_UP){
			//process (new, n)
			neigh_entry* entry = malloc(sizeof(neigh_entry));

			void* ptr = YggEvent_readPayload(event, NULL, entry->neigh_id, sizeof(uuid_t));
			if(!list_find_item(state->neighbours, (comparator_function) equal_entry_id, entry->neigh_id)) {
				YggEvent_readPayload(event, ptr, entry->neigh_addr.data, WLAN_ADDR_LEN);
				list_add_item_to_head(state->neighbours, entry);
				entry->level = DEFAULT_LEVEL;
				entry->st = PEER;
				entry->weight = NULL;

				state->new_neighbour = entry;

			} else {
				free(entry);
			}

		}else {
			//undefined
		}
	}  else if(event->proto_origin == ANY_PROTO) {
		if(event->notification_id == VALUE_CHANGE) {
			YggEvent_readPayload(event, NULL, state->self->weight, sizeof(double));

		}
	}

	process_all(state);
}

static void process_request(YggRequest* request, gap_state* state) {

	if(request->request == REQUEST){
		//got a request to do something
		if(request->request_type == AGG_GET){
			//what is the estimated value?
			YggRequest_freePayload(request);
			short proto_dest = request->proto_origin;
			YggRequest_init(request, state->proto_id, proto_dest, REPLY, AGG_GET);
			YggRequest_addPayload(request, state->self->weight, state->val_size);
			if(state->parent)
				YggRequest_addPayload(request, state->parent->weight, state->val_size);
			else
				YggRequest_addPayload(request, state->self->weight, state->val_size);

			deliverReply(request);

			YggRequest_freePayload(request);
		}else{
			ygg_log("GAP", "WARNING", "Unknown request type");
		}

	}else if(request->request == REPLY){
		ygg_log("GAP", "WARNING", "YGG Request, got an unexpected reply");
	}else{
		ygg_log("GAP", "WARNING", "YGG Request, not a request nor a reply");
	}
}

static void gap_main_loop(main_loop_args* args) {
	queue_t* inBox = args->inBox;
	gap_state* state = (gap_state*) args->state;

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


static void destroy_neighbours(list* neighbours) {
	while(neighbours->size > 0) {
		neigh_entry* entry = list_remove_head(neighbours);
		destroy_neighbour(entry);
		free(entry);
	}
}

static void gap_destroy(gap_state* state) {
	if(state->timer) {
		cancelTimer(state->timer);
		free(state->timer);
	}

	if(state->neighbours) {
		destroy_neighbours(state->neighbours);
		free(state->neighbours);
	}

	if(state->vector) {
		free(state->vector->weight);
		free(state->vector);
	}

	if(state->neutral_value)
		free(state->neutral_value);

	free(state);

}

proto_def* gap_init(void* args) {

	gap_state* state = malloc(sizeof(gap_state));

	gap_args* gargs = (gap_args*) args;
	state->discovery_id = gargs->discovery_id;
	state->proto_id = PROTO_AGG_GAP;

	state->aggregate = aggregation_function_get(gargs->aggregation_operation);
	state->val_size = aggregation_function_getValsize(gargs->aggregation_operation);

	state->neighbours = list_init();
	state->self = malloc(sizeof(neigh_entry));
	getmyId(state->self->neigh_id);

	state->self->weight = malloc(state->val_size);
	get_initial_input_value_for_op(gargs->src, gargs->aggregation_operation, state->self->weight);
	state->self->level = DEFAULT_LEVEL;
	list_add_item_to_head(state->neighbours, state->self);

	state->new_neighbour = NULL;
	state->vector = NULL;
	state->parent = NULL;
	if(gargs->root) {
		state->parent = malloc(sizeof(neigh_entry));
		state->parent->st = PARENT;
		state->parent->level = -1;
		state->parent->weight = NULL;
		genUUID(state->parent->neigh_id);

		list_add_item_to_head(state->neighbours, state->parent);
		state->self->level = 0;
	}

	proto_def* gap = create_protocol_definition(state->proto_id, "GAP", state, (Proto_destroy) gap_destroy);

	proto_def_add_consumed_event(gap, state->discovery_id, NEIGHBOUR_UP);
	proto_def_add_consumed_event(gap, state->discovery_id, NEIGHBOUR_DOWN);
	proto_def_add_consumed_event(gap, ANY_PROTO, VALUE_CHANGE);

	if(gargs->run_on_executor) {
		proto_def_add_msg_handler(gap, (YggMessage_handler) process_msg);
		proto_def_add_timer_handler(gap, (YggTimer_handler) process_timer);
		proto_def_add_event_handler(gap, (YggEvent_handler) process_event);
		proto_def_add_request_handler(gap, (YggRequest_handler) process_request);
	} else {
		proto_def_add_protocol_main_loop(gap, (Proto_main_loop) gap_main_loop);
	}

	state->timer = malloc(sizeof(YggTimer));
	YggTimer_init(state->timer, state->proto_id, state->proto_id);
	YggTimer_set(state->timer, gargs->beacon_period_s, gargs->beacon_period_ns, gargs->beacon_period_s, gargs->beacon_period_ns);
	setupTimer(state->timer);
	return gap;

}

gap_args* gap_args_init(short discovery_id, bool root, bool run_on_executor, agg_value_src src, agg_op aggregation_operation, time_t beacon_period_s, unsigned long beacon_period_ns) {
	gap_args* args = malloc(sizeof(gap_args));
	args->discovery_id = discovery_id;
	args->root = root;
	args->run_on_executor = run_on_executor;
	args->src = src;
	args->aggregation_operation = aggregation_operation;
	args->beacon_period_s = beacon_period_s;
	args->beacon_period_ns = beacon_period_ns;
	return args;
}
void gap_args_destroy(gap_args* args) {
	free(args);
}
