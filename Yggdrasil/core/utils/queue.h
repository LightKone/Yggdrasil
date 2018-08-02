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

#ifndef CORE_QUEUE_H_
#define CORE_QUEUE_H_

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <uuid/uuid.h>

#include "Yggdrasil_lowlvl.h"

#include "core/utils/queue_elem.h"
#include "core/proto_data_struct.h"

typedef struct _queue_t queue_t;

queue_t* queue_init(unsigned short pid, unsigned short capacity);
void queue_destroy(queue_t* q);
void queue_pop(queue_t* q, queue_t_elem* elem);
void queue_push(queue_t* q, queue_t_elem* elem);
short queue_size(queue_t* q, queue_t_elem_type type);
short queue_totalSize(queue_t* q);
void destroy_inner_queue(queue_t* q, queue_t_elem_type type);
int queue_try_timed_pop(queue_t* q, struct timespec* uptotime, queue_t_elem* elem);

#endif /* CORE_QUEUE_H_ */
