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

#include "executor.h"


typedef struct executor_protos_ {
	proto_def* protocol;
	struct executor_protos_* next;
}exec_protos;

typedef struct _executor_state {
	pthread_mutex_t* head_lock;
	exec_protos* executor_protos;
}executor_state;



static short registered(short proto_id, exec_protos* executor_protos) {

	exec_protos* it = executor_protos;

	while(it != NULL) {
		if(proto_def_getId(it->protocol) == proto_id)
			return SUCCESS;
		it = it->next;

	}

	return FAILED;
}

void unregister_proto_handlers(short proto_id, void* state) {


	pthread_mutex_t* head_lock = ((executor_state*) state)->head_lock;
	pthread_mutex_lock(head_lock);
	exec_protos* it = ((executor_state*) state)->executor_protos;
	proto_def* proto = NULL;
	if(it != NULL) {
		proto = it->protocol;
		if(proto_def_getId(proto) == proto_id) {
			exec_protos* torm = it;
			((executor_state*) state)->executor_protos = torm->next;
			destroy_protocol_definition(proto);
			free(torm);
			pthread_mutex_unlock(head_lock);
		} else {
			pthread_mutex_unlock(head_lock);
			while(it->next != NULL) {
				proto = it->next->protocol;
				if(proto_def_getId(proto) == proto_id) {
					exec_protos* torm = it->next;
					it->next = torm->next;
					destroy_protocol_definition(proto);
					free(torm);
					break;
				}
				it = it->next;
			}

		}
	}
}

void register_proto_handlers(proto_def* protocol, void* state) {

	pthread_mutex_t* head_lock = ((executor_state*) state)->head_lock;

	if(!registered(proto_def_getId(protocol), ((executor_state*) state)->executor_protos)) {
		pthread_mutex_lock(head_lock);
		exec_protos* proto = malloc(sizeof(exec_protos));
		proto->protocol = protocol;
		proto->next = ((executor_state*) state)->executor_protos;
		((executor_state*) state)->executor_protos = proto;
		pthread_mutex_unlock(head_lock);
	}
}

static exec_protos* find_proto(short proto_id, exec_protos* executor_protos) {

	exec_protos* it = executor_protos;

	while(it != NULL) {
		if(proto_def_getId(it->protocol) == proto_id)
			return it;

		it = it->next;
	}
	return NULL;
}


static short execute_msg_handler(YggMessage* msg, exec_protos* executor_protos) {
	exec_protos* proto = find_proto(msg->Proto_id, executor_protos);
	if(proto != NULL) {
		YggMessage_handler handler = proto_def_get_YggMessageHandler(proto->protocol);
		if(handler != NULL) {
			return handler(msg, proto_def_getState(proto->protocol));
		}
	}
	return FAILED;
}

static short execute_timer_handler(YggTimer* timer, exec_protos* executor_protos) {
	exec_protos* proto = find_proto(timer->proto_dest, executor_protos);
	if(proto != NULL) {
		YggTimer_handler handler = proto_def_get_YggTimerHandler(proto->protocol);
		if(handler != NULL) {
			return handler(timer, proto_def_getState(proto->protocol));
		}
	}
	return FAILED;
}

static short execute_event_handler(YggEvent* event, exec_protos* executor_protos) {
	exec_protos* proto = find_proto(event->proto_dest, executor_protos);
	if(proto != NULL) {
		YggEvent_handler handler = proto_def_get_YggEventHandler(proto->protocol);
		if(handler != NULL) {
			return handler(event, proto_def_getState(proto->protocol));
		}
	}
	return FAILED;
}

static short execute_request_handler(YggRequest* request, exec_protos* executor_protos) {
	exec_protos* proto = find_proto(request->proto_dest, executor_protos);
	if(proto != NULL) {
		YggRequest_handler handler = proto_def_get_YggRequestHandler(proto->protocol);
		if(handler != NULL) {
			return handler(request, proto_def_getState(proto->protocol));
		}
	}
	return FAILED;
}


static void* executor_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	executor_state* state = (executor_state*)args->state;
	while(1) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		switch(elem.type) {
		case YGG_MESSAGE:
			execute_msg_handler(&elem.data.msg, state->executor_protos);
			break;
		case YGG_TIMER:
			execute_timer_handler(&elem.data.timer, state->executor_protos);
			break;
		case YGG_EVENT:
			execute_event_handler(&elem.data.event, state->executor_protos);
			break;
		case YGG_REQUEST:
			if(elem.data.request.proto_dest == PROTO_EXECUTOR
					&& elem.data.request.request == REQUEST
					&& elem.data.request.request_type == EXECUTOR_STOP_PROTOCOL) {
				short proto_id;
				YggRequest_readPayload(&elem.data.request, NULL, &proto_id, sizeof(short));
				unregister_proto_handlers(proto_id, args->state);
			} else
				execute_request_handler(&elem.data.request, state->executor_protos);
			break;
		default:
			ygg_log("EXECUTOR", "WARNING", "Invalid element retrieved");
			break;
		}

		free_elem_payload(&elem);
	}

	return NULL;
}

static short executor_destroy(void* state) {

	exec_protos* executor_protos = ((executor_state*)state)->executor_protos;
	pthread_mutex_lock(((executor_state*)state)->head_lock);
	exec_protos* it = executor_protos;
	while(it != NULL) {
		exec_protos* torm = it;
		it = it->next;
		destroy_protocol_definition(torm->protocol);
		free(torm);
	}

	pthread_mutex_unlock(((executor_state*)state)->head_lock);
	pthread_mutex_destroy(((executor_state*)state)->head_lock);

	return SUCCESS;

}

proto_def* executor_init(void* args) {

	executor_state* state = malloc(sizeof(executor_state));

	state->head_lock = malloc(sizeof(pthread_mutex_t));
	state->executor_protos = NULL;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(state->head_lock, &attr);

	proto_def* executor = create_protocol_definition(PROTO_EXECUTOR, "Executor", state, executor_destroy);
	proto_def_add_protocol_main_loop(executor, &executor_main_loop);

	return executor;
}
