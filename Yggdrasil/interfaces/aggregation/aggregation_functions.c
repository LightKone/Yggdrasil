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

#include "aggregation_functions.h"

aggregation_function aggregation_function_get(agg_op op) {
	switch(op){
	case OP_COUNT:
	case OP_SUM:
		return sum_function;
		break;
	case OP_MAX:
		return max_function;
		break;
	case OP_MIN:
		return min_function;
		break;
	case OP_AVG:
		return avg_function;
		break;
	default:
		break;
	}
	return NULL;
}

aggregation_function aggregation_function_get_simetric(agg_op op) {
	switch(op){
	case OP_COUNT:
	case OP_SUM:
		return subtract_function;
		break;
	case OP_MAX:
		return min_function;
		break;
	case OP_MIN:
		return max_function;
		break;
	case OP_AVG:
		return rmAvg_contribution;
		break;
	default:
		break;
	}
	return NULL;
}

void* sum_function(void* value1, void* value2, void* newvalue){
	double v1 = *(double*) value1;
	double v2 = *(double*) value2;
	double v = v1 + v2;
	memcpy(newvalue, &v, sizeof(double));
	return newvalue;
}

void* subtract_function(void* value1, void* value2, void* newvalue) {
	double v1 = *(double*) value1;
	double v2 = *(double*) value2;
	double v = v1 - v2;
	memcpy(newvalue, &v, sizeof(double));
	return newvalue;
}

void* min_function(void* value1, void* value2, void* newvalue) {
	double v1 = *(double*) value1;
	double v2 = *(double*) value2;
	double v = min(v1, v2);
	memcpy(newvalue, &v, sizeof(double));
	return newvalue;
}

void* max_function(void* value1, void* value2, void* newvalue) {
	double v1 = *(double*) value1;
	double v2 = *(double*) value2;
	double v = max(v1, v2);
	memcpy(newvalue, &v, sizeof(double));
	return newvalue;
}


void* avg_function(void* value1, void* value2, void* newvalue){
	double avg1, avg2, avg;
	int count1, count2, count;

	memcpy(&avg1, value1, sizeof(double));
	memcpy(&avg2, value2, sizeof(double));

	memcpy(&count1, value1 + sizeof(double), sizeof(int));
	memcpy(&count2, value2 + sizeof(double), sizeof(int));

	double sum1, sum2, sum;

	sum1 = avg1*count1;
	sum2 = avg2*count2;

	sum = sum1 + sum2;
	count = count1 + count2;
	avg = sum / count;


	memcpy(newvalue, &avg, sizeof(double));
	memcpy(newvalue + sizeof(double), &count, sizeof(int));

	return newvalue;
}

void* rmAvg_contribution(void* value, void* valuetorm, void* newvalue) {
	double avg1, avg2, avg;
	int count1, count2, count;

	memcpy(&avg1, value, sizeof(double));
	memcpy(&avg2, valuetorm, sizeof(double));

	memcpy(&count1, value + sizeof(double), sizeof(int));
	memcpy(&count2, valuetorm + sizeof(double), sizeof(int));

	double sum1, sum2, sum;

	sum1 = avg1*count1;
	sum2 = avg2*count2;

	sum = sum1 - sum2;
	count = count1 - count2;

	if(count != 0)
		avg = sum / count;
	else
		return NULL;


	memcpy(newvalue, &avg, sizeof(double));
	memcpy(newvalue + sizeof(double), &count, sizeof(int));

	return newvalue;
}

short aggregation_function_getValsize(agg_op op){
	switch(op){
	case OP_SUM:
		return SUMVALSIZE;
		break;
	case OP_MAX:
		return MAXVALSIZE;
		break;
	case OP_MIN:
		return MINVALSIZE;
		break;
	case OP_AVG:
		return AVGVALSIZE;
		break;
	case OP_COUNT:
		return COUNTSIZE;
		break;
	default:
		break;
	}
	return NOTDEFINEDVALSIZE;
}

short aggregation_valueDiff(void* value1, void* value2){
	double val1, val2;

	memcpy(&val1, value1, sizeof(double));
	memcpy(&val2, value2, sizeof(double));

	return val1 - val2;
}
