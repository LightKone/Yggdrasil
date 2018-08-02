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

#ifndef CORE_QUEUE_ELEM_H_
#define CORE_QUEUE_ELEM_H_

#include "core/proto_data_struct.h"

//The order of the elements here also define the priority of them being poped out of queues
typedef enum queue_t_elemeent_type_ {
	YGG_TIMER = 0,
	YGG_EVENT = 1,
	YGG_MESSAGE = 2,
	YGG_REQUEST = 3,
	TYPE_MAX = 4
} queue_t_elem_type;

//Must follow the same order as define above
const int size_of_element_types[TYPE_MAX];

typedef union queue_t_element_contents {
	YggTimer timer;
	YggMessage msg;
	YggEvent event;
	YggRequest request;
} queue_t_cont;

typedef struct queue_t_element_ {
	queue_t_elem_type type;
	queue_t_cont data;
} queue_t_elem;

void free_elem_payload(queue_t_elem* elem);


#endif /* CORE_QUEUE_ELEM_H_ */
