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


#ifndef PROTOCOLS_AGGREGATION_GAP_H_
#define PROTOCOLS_AGGREGATION_GAP_H_

#include "core/ygg_runtime.h"
#include "interfaces/aggregation/aggregation_operations.h"
#include "interfaces/aggregation/aggregation_functions.h"
#include "interfaces/discovery/discovery_events.h"

#include "data_structures/generic/list.h"

#define PROTO_AGG_GAP 202

typedef struct _gap_args {
	short discovery_id;
	bool root;
	bool run_on_executor;
	agg_value_src src;
	agg_op aggregation_operation; //aggregation function to compute
	time_t beacon_period_s;
	unsigned long beacon_period_ns;
}gap_args;

proto_def* gap_init(void* args);

gap_args* gap_args_init(short discovery_id, bool root, bool run_on_executor, agg_value_src src, agg_op aggregation_operation, time_t beacon_period_s, unsigned long beacon_period_ns);
void gap_args_destroy(gap_args* args);

#endif /* PROTOCOLS_AGGREGATION_GAP_H_ */
