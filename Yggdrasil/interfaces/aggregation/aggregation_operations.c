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

#include "aggregation_operations.h"

short get_initial_input_value(agg_value_src val, double* value){

	switch(val){
	case TEST_INPUT:
		*value = getTestValue();
		break;
	default:
		return FAILED;
		break;
	}

	return SUCCESS;
}

short get_initial_input_value_for_op(agg_value_src valdef, agg_op op, void* value){
	double val;
	int one = 1;
	if(op == OP_COUNT){
		double oneDouble = 1;
		memcpy(value, &oneDouble, sizeof(double));
		return SUCCESS;
	}
	if(get_initial_input_value(valdef, &val) == SUCCESS){
		switch(op){
			case OP_SUM:
			case OP_MAX:
			case OP_MIN:
				memcpy(value, &val, sizeof(double));
				break;
			case OP_AVG:
				memcpy(value, &val, sizeof(double));
				memcpy(value+sizeof(double), &one, sizeof(int));
				break;
			default:
				return FAILED;
			}
			return SUCCESS;
	}

	return FAILED;
}

void send_change_value_request(short proto_dest, short proto_origin, double* new_input_value, agg_op operation) {
	YggRequest req;
	YggRequest_init(&req, proto_origin, proto_dest, REQUEST, AGG_CHANGE_VAL);
	YggRequest_addPayload(&req, &operation, sizeof(agg_op));
	YggRequest_addPayload(&req, new_input_value, sizeof(double));

	deliverRequest(&req);
	YggRequest_freePayload(&req);
}
