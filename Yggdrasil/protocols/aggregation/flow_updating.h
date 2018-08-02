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

#ifndef PROTOCOLS_AGGREGATION_FLOW_UPDATING_H_
#define PROTOCOLS_AGGREGATION_FLOW_UPDATING_H_

#include "core/ygg_runtime.h"
#include "interfaces/aggregation/aggregation_operations.h"
#include "interfaces/discovery/discovery_events.h"

#include "data_structures/generic/list.h"

#define PROTO_AGG_FLOW_UPDATING_ID 201

typedef struct _flow_updating_args {
	short discovery_id;
	bool run_on_executor;
	agg_value_src src;
	time_t beacon_period_s;
	unsigned long beacon_period_ns;
}flow_updating_args;

proto_def* flow_updating_init(void* args);

flow_updating_args* flow_updating_args_init(short discovery_id, bool run_on_executor, agg_value_src src, time_t beacon_period_s, unsigned long beacon_period_ns);
void flow_updating_args_destroy(flow_updating_args* args);

#endif /* PROTOCOLS_AGGREGATION_FLOW_UPDATING_H_ */
