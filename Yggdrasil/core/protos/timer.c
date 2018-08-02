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

#include "timer.h"

typedef struct timer_elem_ {
	YggTimer* timer;
	struct timer_elem_* next;
} timer_elem;

typedef struct _timer_state {
	unsigned short pendingTimersSize;
	timer_elem* pendingTimers;
}timer_state;

static void processExpiredTimers(timer_state* state) {

	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);

	struct timespec zero;
	zero.tv_sec = 0;
	zero.tv_nsec = 0;


	while(state->pendingTimers != NULL && (compare_timespec(&state->pendingTimers->timer->config.first_notification, &now) <= 0 )) {

		//Remove it from the queue
		timer_elem* elem = state->pendingTimers;
		state->pendingTimers = elem->next;
		elem->next = NULL;

		//Check if this is a repeatable timer, if so reinsert
		if(compare_timespec(&elem->timer->config.repeat_interval, &zero) > 0) {
			timer_elem* tmp = malloc(sizeof(timer_elem));
			tmp->timer = malloc(sizeof(YggTimer));
			tmp->next = NULL;
			memcpy(tmp->timer, elem->timer, sizeof(YggTimer));


			clock_gettime(CLOCK_REALTIME, &tmp->timer->config.first_notification);
			tmp->timer->config.first_notification.tv_sec += tmp->timer->config.repeat_interval.tv_sec;
			setNanoTime(&tmp->timer->config.first_notification, tmp->timer->config.repeat_interval.tv_nsec);

			if(state->pendingTimers == NULL || (compare_timespec(&state->pendingTimers->timer->config.first_notification, &tmp->timer->config.first_notification) > 0)) {
				tmp->next = state->pendingTimers;
				state->pendingTimers = tmp;
			} else {
				timer_elem* current = state->pendingTimers;
				while(current->next != NULL) {
					if(compare_timespec(&current->next->timer->config.first_notification, &tmp->timer->config.first_notification) > 0) {
						break;
					}
					current = current->next;
				}
				tmp->next = current->next;
				current->next = tmp;
			}
		} else {
			state->pendingTimersSize--;
		}

		if(deliverTimer(elem->timer) == FAILED){
			ygg_log("TIMER", "WARNING", "No one to be delivered to..");
		}
		free(elem->timer);
		free(elem);

		//Update the time, just for compensation of the time spent in the delivery... (others might have expired...)
		clock_gettime(CLOCK_REALTIME, &now);
	}
}

static void* timer_main_loop(main_loop_args* args) {

	timer_state* state = (timer_state*) args->state;
	queue_t* inBox = args->inBox;
	state->pendingTimersSize = 0;
	state->pendingTimers = NULL;

	queue_t_elem el;

	while(1){

		int gotRequest = 1;
		if(state->pendingTimers == NULL) {

			queue_pop(inBox, &el);
		} else {
			struct timespec* timeout = &state->pendingTimers->timer->config.first_notification;

			gotRequest = queue_try_timed_pop(inBox, timeout, &el);
		}

		if(gotRequest == 1) {

			//If this is not something we understand we stop here
			//should never have
			if(el.type != YGG_TIMER) {
				ygg_log("TIMER", "ERROR", "received something on queue that is not a Timer.");
				continue;
			}

			if(el.data.timer.proto_dest != PROTO_TIMER || el.data.timer.proto_origin != PROTO_TIMER) {

				timer_elem* newTimerEvent = malloc(sizeof(timer_elem));
				newTimerEvent->timer = malloc(sizeof(YggTimer));
				newTimerEvent->next = NULL;
				memcpy(newTimerEvent->timer, &el.data.timer, sizeof(YggTimer));

				if(newTimerEvent->timer->config.first_notification.tv_sec == 0 && newTimerEvent->timer->config.first_notification.tv_nsec == 0
						&& newTimerEvent->timer->config.repeat_interval.tv_sec == 0 && newTimerEvent->timer->config.repeat_interval.tv_nsec == 0) {

					//The goal now is to cancel a timer
					if(state->pendingTimers != NULL && uuid_compare(state->pendingTimers->timer->id, newTimerEvent->timer->id) == 0) {
						//We are canceling the first timer
						timer_elem* tmp = state->pendingTimers;
						state->pendingTimers = tmp->next;
						state->pendingTimersSize--;
						free(tmp->timer);
						free(tmp);
					} else if(state->pendingTimers != NULL) {
						timer_elem* current = state->pendingTimers;
						while(current->next != NULL && uuid_compare(current->next->timer->id, newTimerEvent->timer->id) != 0) {
							current = current->next;
						}
						if(current->next != NULL) {
							timer_elem* tmp = current->next;
							current->next = tmp->next;
							free(tmp->timer);
							free(tmp);
							state->pendingTimersSize--;

						}
					} else {
						char s[100];
						sprintf(s,"Received a cancellation request when I have no pending timers (origin %d).", newTimerEvent->timer->proto_origin);
						ygg_log("TIMER", "WARNING", s);
					}
					free(newTimerEvent->timer);
					free(newTimerEvent);
				} else {
					if(state->pendingTimers == NULL || compare_timespec(&state->pendingTimers->timer->config.first_notification, &newTimerEvent->timer->config.first_notification) > 0) {
						newTimerEvent->next = state->pendingTimers;
						state->pendingTimers = newTimerEvent;
						state->pendingTimersSize++;
					} else {
						timer_elem* current = state->pendingTimers;
						while(current->next != NULL) {
							if(compare_timespec(&current->next->timer->config.first_notification, &newTimerEvent->timer->config.first_notification) > 0)
								break;
							current = current->next;
						}
						newTimerEvent->next = current->next;
						current->next = newTimerEvent;
						state->pendingTimersSize++;
					}
				}
			}
		}

		processExpiredTimers(state);

	}

	return NULL;
}

static short timer_destroy(void* state) {
	timer_elem* el = ((timer_state*)state)->pendingTimers;
	while(el != NULL) {
		timer_elem* torm = el;
		el = el->next;
		free(torm->timer);
		free(torm);
		((timer_state*)state)->pendingTimersSize --;
	}

	free(state);

	return SUCCESS;
}

proto_def* timer_init(void* args) {

	timer_state* state = malloc(sizeof(timer_state));

	proto_def* timer = create_protocol_definition(PROTO_TIMER, "Timer", state, timer_destroy);
	proto_def_add_protocol_main_loop(timer, &timer_main_loop);

	return timer;
}
