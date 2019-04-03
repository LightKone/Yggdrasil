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

#ifndef PROTOCOLS_AGGREGATION_LIMOSENSE_H_
#define PROTOCOLS_AGGREGATION_LIMOSENSE_H_

#include "core/ygg_runtime.h"

#include "interfaces/discovery/discovery_events.h"
#include "interfaces/aggregation/aggregation_operations.h"

#include "data_structures/specialized/neighbour_list.h"

#define PROTO_AGG_LIMOSENSE 203

typedef struct _limosense_args{
	short discovery_id;
	bool run_on_executor;
	agg_value_src src;
	time_t beacon_period_s;
	unsigned long beacon_period_ns;
}limosense_args;

proto_def* limosense_init(void* args);

limosense_args* limosense_args_init(short discovery_id, bool run_on_executor, agg_value_src src, time_t beacon_period_s, unsigned long beacon_period_ns);
void limosense_args_destroy(limosense_args* args);

#endif /* PROTOCOLS_AGGREGATION_LIMOSENSE_H_ */
