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

#ifndef PROTOCOLS_AGGREGATION_MIRAGE_H_
#define PROTOCOLS_AGGREGATION_MIRAGE_H_

#include <string.h>
#include <uuid/uuid.h>

#include "core/ygg_runtime.h"
#include "interfaces/aggregation/aggregation_operations.h"
#include "interfaces/aggregation/aggregation_functions.h"
#include "interfaces/discovery/discovery_events.h"

#include "data_structures/generic/list.h"

#define PROTO_AGG_MULTI_ROOT 200


typedef struct multi_root_agg_args_ {
	short fault_detector_id;
	bool run_on_executor;
	agg_value_src src;
	agg_op aggregation_operation; //aggregation function to compute
	time_t beacon_period_s;
	unsigned long beacon_period_ns;
}multi_root_agg_args;

proto_def* multi_root_init(void* args);

multi_root_agg_args* multi_root_agg_args_init(short fault_detector_id, bool run_on_executor, agg_value_src src, agg_op aggregation_operation, time_t beacon_period_s, unsigned long beacon_period_ns);
void multi_root_agg_args_destroy(multi_root_agg_args* args);


#endif /* PROTOCOLS_AGGREGATION_MIRAGE_H_ */
