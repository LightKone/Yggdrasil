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

#include "simple_discovery.h"

#define DEFAULT_ANNOUNCE_PERIOD_S 2
#define DEFAULT_ANNOUNCE_PERIOD_NS 0

typedef struct _simple_discovery_state {
	short proto_id;
	uuid_t myid;
	list* neighbours;
	YggTimer announce;
}simple_discovery_state;

static short process_msg(YggMessage* msg, void* state) {
	simple_discovery_state* discov_state = (simple_discovery_state*) state;


	uuid_t id;
	YggMessage_readPayload(msg, NULL, id, sizeof(uuid_t));

	if(neighbour_find(discov_state->neighbours, id) == NULL) {

#ifdef DEGUB
		printf("New neighbour\n");
#endif
		neighbour_item* newnei = new_neighbour(id, msg->srcAddr, NULL, 0, NULL);
		neighbour_add_to_list(discov_state->neighbours, newnei);

		send_event_neighbour_up(discov_state->proto_id, newnei->id, &newnei->addr);

	}

	return SUCCESS;
}

static short process_timer(YggTimer* timer, void* state) {
	simple_discovery_state* discov_state = (simple_discovery_state*) state;

	YggMessage msg;
	YggMessage_initBcast(&msg, discov_state->proto_id);
	YggMessage_addPayload(&msg, (void*) discov_state->myid, sizeof(uuid_t));

	dispatch(&msg);

	return SUCCESS;
}

static void* simple_discovery_main_loop(main_loop_args* args) {
	simple_discovery_state* state = (simple_discovery_state*) args->state;
	queue_t* inBox = args->inBox;

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		switch(elem.type) {
		case YGG_MESSAGE:
			process_msg(&elem.data.msg, (void*) state);
			break;
		case YGG_TIMER:
			process_timer(&elem.data.timer, (void*) state);
			break;
		default:
			//noop
			break;
		}

		free_elem_payload(&elem);
	}

	return NULL;
}

static short simple_discovery_destroy(void* state) {
	cancelTimer(&((simple_discovery_state*)state)->announce);
	neighbour_list_destroy(((simple_discovery_state*)state)->neighbours);
	free(((simple_discovery_state*)state)->neighbours);
	free(state);

	return SUCCESS;
}

proto_def* simple_discovery_init(void* args) {

	simple_discovery_args* sargs = (simple_discovery_args*)args;

	simple_discovery_state* state = malloc(sizeof(simple_discovery_state));
	getmyId(state->myid);
	state->neighbours = NULL;
	state->proto_id = PROTO_SIMPLE_DISCOVERY_ID;
	state->neighbours = list_init();
	proto_def* discovery = create_protocol_definition(state->proto_id, "Simple Discovery", state, simple_discovery_destroy);

	proto_def_add_produced_events(discovery, 1); //NEIGHBOUR_UP

	if(sargs->run_on_executor) {
		proto_def_add_msg_handler(discovery, process_msg);
		proto_def_add_timer_handler(discovery, process_timer);
	} else
		proto_def_add_protocol_main_loop(discovery, simple_discovery_main_loop);


	YggTimer_init(&state->announce, state->proto_id, state->proto_id);
	YggTimer_set(&state->announce, sargs->announce_period_s, sargs->announce_period_ns, sargs->announce_period_s, sargs->announce_period_ns);


	setupTimer(&state->announce);
	return discovery;
}

simple_discovery_args* simple_discovery_args_init(bool run_on_executor, time_t announce_period_s, unsigned long announce_period_ns) {
	simple_discovery_args* sargs = malloc(sizeof(simple_discovery_args));
	sargs->run_on_executor = run_on_executor;
	sargs->announce_period_s = announce_period_s;
	sargs->announce_period_ns = announce_period_ns;
	return sargs;
}
void simple_discovery_args_destroy(simple_discovery_args* sargs) {
	free(sargs);
}
