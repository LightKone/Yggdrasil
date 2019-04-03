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

#ifndef PROTOCOLS_COMMUNICATION_ROUTING_BATMAN_H_
#define PROTOCOLS_COMMUNICATION_ROUTING_BATMAN_H_

#include <stdlib.h>

#include "core/ygg_runtime.h"
#include "interfaces/routing/routing_interface.h"

#include "data_structures/specialized/neighbour_list.h"

#define PROTO_ROUTING_BATMAN 161

#define DEFAULT_BATMAN_WINDOW_SIZE 128

typedef struct _batman_args{
	bool executor;
	bool standart;
	time_t omg_beacon_s;
	unsigned long omg_beacon_ns;
	unsigned short ttl;
	int window_size;
	int log_s;
}batman_args;

proto_def* batman_init(void* args);

batman_args* batman_args_init(bool executor, bool standart, time_t omg_beacon_s, unsigned long omg_beacon_ns, unsigned short ttl, int window_size, int log_s);
void batman_args_destroy(batman_args* args);

#endif /* PROTOCOLS_COMMUNICATION_ROUTING_BATMAN_H_ */
