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

#include "queue.h"

typedef struct _inner_queue_t {
	unsigned short element_size;
	unsigned short max_count;
	unsigned short occupation;
	void* data;
	int writer;
	int reader;
}inner_queue_t;

struct _queue_t {
	unsigned short pid;
	pthread_mutex_t lock;
	sem_t full;
	inner_queue_t iqs[TYPE_MAX];
};

void inner_queue_init(inner_queue_t* iq, unsigned short capacity, unsigned short element_size) {
	iq->max_count  = capacity;
	iq->element_size = element_size;
	iq->occupation = 0;
	iq->data = malloc(element_size * capacity);
	iq->writer = 0;
	iq->reader = 0;
}

void inner_queue_destroy(inner_queue_t* iq) {
	free(iq->data);
}

queue_t* queue_init(unsigned short pid, unsigned short capacity) {

	queue_t* q = malloc(sizeof(queue_t));

	pthread_mutex_init(&q->lock, NULL);

	q->pid = pid;
	int i;
	for(i = 0; i < TYPE_MAX; i++) {
		inner_queue_init(&(q->iqs[i]), capacity, size_of_element_types[i]);

	}
	sem_init(&q->full, 0, 0);

	return q;
}

void queue_destroy(queue_t* q) {
	sem_destroy(&q->full);
	pthread_mutex_destroy(&q->lock);
	int i;
	for(i = 0; i < TYPE_MAX; i++) {
		inner_queue_destroy(&(q->iqs[i]));
	}
	free(q);
}

int inner_queue_pop(inner_queue_t* iq, queue_t_cont* cont) {

	if(iq->occupation > 0){

		memcpy(cont, iq->data + (iq->reader*iq->element_size), iq->element_size);
		iq->occupation--;
		iq->reader++;
		if(iq->reader >= iq->max_count) {
			iq->reader = 0;

		}
		return 1;
	}
	return 0;
}

void queue_pop(queue_t* q, queue_t_elem* elem) {

	while(sem_wait(&q->full) != 0);
	pthread_mutex_lock(&q->lock);
	unsigned short i = 0;

	while(i < TYPE_MAX && inner_queue_pop(&(q->iqs[i]), &elem->data) == 0) {
		i++;

	}
	elem->type = i;

	pthread_mutex_unlock(&q->lock);
}

int queue_try_timed_pop(queue_t* q, struct timespec* uptotime, queue_t_elem* elem) {

	int r = 0;
	while(1) {
		r = sem_timedwait(&q->full, uptotime) != 0;
		if(r == 0)
			break;
		else if(errno == ETIMEDOUT)
			return 0;
	}
	pthread_mutex_lock(&q->lock);
	unsigned short i = 0;
	while(i < TYPE_MAX && inner_queue_pop(&(q->iqs[i]), &elem->data) == 0) {
		i++;

	}
	elem->type = i;
	pthread_mutex_unlock(&q->lock);
	return 1;
}

void queue_push(queue_t* q, queue_t_elem* elem) {
	inner_queue_t* target = &q->iqs[elem->type];
	pthread_mutex_lock(&q->lock);

	if(target->occupation >= target->max_count) {
		void* newdata = malloc(target->element_size * target->max_count * 2);

		if( target-> reader == target->writer && target->writer != 0) {
			memcpy(newdata, target->data + (target->reader * target->element_size),
					target->element_size * (target->max_count - target->reader));

			memcpy(newdata + ((target->max_count - target->reader) * target->element_size), target->data,
					target->element_size * target->reader);
			target->reader = 0;
		} else {
			memcpy(newdata, target->data, target->element_size * target->max_count);
		}

		free(target->data);
		target->data = newdata;
		target->writer = target->max_count;
		target->max_count = target->max_count * 2;
	}

	memcpy(target->data + (target->writer * target->element_size), &elem->data, target->element_size);

	if(elem->type == YGG_REQUEST) {
		YggRequest* tmp = (YggRequest*)(target->data + (target->writer * target->element_size));
		if(tmp->length > 0) {
			tmp->payload = malloc(elem->data.request.length);
			memcpy(tmp->payload, elem->data.request.payload, elem->data.request.length);
		} else {
			tmp->payload = NULL;
		}
	} else if(elem->type == YGG_TIMER) {
		YggTimer* tmp = (YggTimer*)(target->data + (target->writer * target->element_size));
		if(tmp->length > 0) {
			tmp->payload = malloc(elem->data.timer.length);
			memcpy(tmp->payload, elem->data.timer.payload, elem->data.timer.length);
		} else {
			tmp->payload = NULL;
		}
	} else if(elem->type == YGG_EVENT) {
		YggEvent* tmp = (YggEvent*)(target->data + (target->writer * target->element_size));
		if(tmp->length > 0) {
			tmp->payload = malloc(elem->data.event.length);
			memcpy(tmp->payload, elem->data.event.payload, elem->data.event.length);
		} else {
			tmp->payload = NULL;
		}
	}

	target->writer++;
	if(target->writer >= target->max_count) {
		target->writer = 0;
	}
	target->occupation++;
	sem_post(&q->full);
	pthread_mutex_unlock(&q->lock);

}

short queue_size(queue_t* q, queue_t_elem_type type) {
	pthread_mutex_lock(&q->lock);
	unsigned short s = q->iqs[type].occupation;
	pthread_mutex_unlock(&q->lock);
	return s;
}

short queue_totalSize(queue_t* q) {
	pthread_mutex_lock(&q->lock);
	unsigned short s = q->iqs[0].occupation;
	int i;
	for(i = 1; i < TYPE_MAX; i++)
		s += q->iqs[i].occupation;
	pthread_mutex_unlock(&q->lock);
	return s;
}

//After the execution of this command attempting to manipulate
//this inner queue will result on unspecified behavior (most likely a crash)
//This should only only only be executed if this queue will never
//hold an element of the type provided as argument.
void destroy_inner_queue(queue_t* q, queue_t_elem_type type) {
	inner_queue_destroy(&q->iqs[type]);
	memset(&q->iqs[type], 0, sizeof(inner_queue_t));
}
