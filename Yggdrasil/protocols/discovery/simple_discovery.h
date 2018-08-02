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

#ifndef PROTOCOLS_DISCOVERY_SIMPLE_DISCOVERY_H_
#define PROTOCOLS_DISCOVERY_SIMPLE_DISCOVERY_H_

#include "core/ygg_runtime.h"
#include "interfaces/discovery/discovery_events.h"
#include "data_structures/specialized/neighbour_list.h"

#define PROTO_SIMPLE_DISCOVERY_ID 100

typedef struct _simple_discovery_args {
	bool run_on_executor;
	time_t announce_period_s;
	unsigned long announce_period_ns;
}simple_discovery_args;

simple_discovery_args* simple_discovery_args_init(bool run_on_executor, time_t announce_period_s, unsigned long announce_period_ns);
void simple_discovery_args_destroy(simple_discovery_args* sargs);

proto_def* simple_discovery_init(void* args);

#endif /* PROTOCOLS_DISCOVERY_SIMPLE_DISCOVERY_H_ */
