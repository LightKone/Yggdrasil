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

#ifndef APPS_CONTROL_APP_H_
#define APPS_CONTROL_APP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocols/utility/topologyManager.h"
#include "core/ygg_runtime.h"

typedef struct contro_app_attr_{
	queue_t* inBox;
	short protoId;
}contro_app_attr;

typedef enum control_events_{
	CHANGE_VAL
}control_events;

void* control_app_init(void* args);

#endif /* APPS_CONTROL_APP_H_ */
