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

#include "queue_elem.h"

const int size_of_element_types[] = {sizeof(YggTimer), sizeof(YggEvent), sizeof(YggMessage), sizeof(YggRequest)};

void free_elem_payload(queue_t_elem* elem) {
	switch(elem->type) {
	case YGG_TIMER:
		if(elem->data.timer.length > 0  && elem->data.timer.payload != NULL) {
			free(elem->data.timer.payload);
			elem->data.timer.length = 0;
			elem->data.timer.payload = NULL;
		}
		break;
	case YGG_EVENT:
		if(elem->data.event.length > 0 && elem->data.event.payload != NULL) {
			free(elem->data.event.payload);
			elem->data.event.length = 0;
			elem->data.event.payload = NULL;
		}
		break;
	case YGG_REQUEST:
		if(elem->data.request.length > 0 && elem->data.request.payload != NULL) {
			free(elem->data.request.payload);
			elem->data.request.length = 0;
			elem->data.request.payload = NULL;
		}
		break;
	default:
		break;
	}
}
