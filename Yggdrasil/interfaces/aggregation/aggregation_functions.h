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

#ifndef INTERFACES_AGGREGATION_AGGREGATION_FUNCTIONS_H_
#define INTERFACES_AGGREGATION_AGGREGATION_FUNCTIONS_H_

#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "aggregation_operations.h"

typedef enum value_size_t_{
	NOTDEFINEDVALSIZE = -1,
	SUMVALSIZE = sizeof(double),
	MINVALSIZE = sizeof(double),
	MAXVALSIZE = sizeof(double),
	AVGVALSIZE = sizeof(double) + sizeof(int),
	COUNTSIZE = sizeof(double)
}value_size_t;

typedef void* (*aggregation_function) (void* value1, void* value2, void* newvalue);

aggregation_function aggregation_function_get(agg_op op);
aggregation_function aggregation_function_get_simetric(agg_op op);

void* sum_function(void* value1, void* value2, void* newvalue);

void* min_function(void* value1, void* value2, void* newvalue);

void* max_function(void* value1, void* value2, void* newvalue);

void* avg_function(void* value1, void* value2, void* newvalue);

/**********************************************************
 * Other useful functions
 **********************************************************/

void* subtract_function(void* value1, void* value2, void* newvalue);

void* rmAvg_contribution(void* value, void* valuetorm, void* newvalue);

short aggregation_function_getValsize(agg_op op);

short aggregation_valueDiff(void* value1, void* value2);

#endif /* INTERFACES_AGGREGATION_AGGREGATION_FUNCTIONS_H_ */
