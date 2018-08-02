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

#ifndef INTERFACES_AGGREGATION_AGGREGATION_OPERATIONS_H_
#define INTERFACES_AGGREGATION_AGGREGATION_OPERATIONS_H_

#include <stdlib.h>

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"

typedef enum {
	USER_INPUT, //controlled by user
	TEST_INPUT //use test value
}agg_value_src;

typedef enum {
	OP_SUM = 0,
	OP_MAX = 1,
	OP_MIN = 2,
	OP_AVG = 3,
	OP_COUNT = 4,
	OP_NONE = -1
}agg_op;

typedef enum {
	AGG_CHANGE_VAL, //change the value being aggregated
	AGG_GET //request the current aggregation value given the aggregation operation
}agg_requests;

typedef enum {
	SRC_VALUE_CHANGE //notify that the source value has changed
}agg_events;

short get_initial_input_value(agg_value_src src, double* value);
short get_initial_input_value_for_op(agg_value_src src, agg_op op, void* value);

void send_change_value_request(short proto_dest, short proto_origin, double* new_input_value, agg_op operation);

#endif /* INTERFACES_AGGREGATION_AGGREGATION_OPERATIONS_H_ */
