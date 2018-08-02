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

#ifndef INTERFACES_ROUTING_ROUTING_INTERFACE_H_
#define INTERFACES_ROUTING_ROUTING_INTERFACE_H_

#include <stdlib.h>
#include <string.h>

#include "core/ygg_runtime.h"

#define PROTO_ROUTING 60

typedef enum routing_request_{
	SEND_MESSAGE,
	NO_ROUTE_TO_HOST
}routing_requests;

int request_route_message(YggMessage* msg, int node_number);

int request_specific_route_message(int routing_proto_id, YggMessage* msg, int node_number);

void unload_request_route_message(YggRequest* req, YggMessage* msg, uuid_t destination);

#endif /* INTERFACES_ROUTING_ROUTING_INTERFACE_H_ */
