/*
 * udp_oracle.h
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_UTILITY_ORACLES_UDP_ORACLE_H_
#define PROTOCOLS_IP_UTILITY_ORACLES_UDP_ORACLE_H_


#include "core/ygg_runtime.h"
#include "data_structures/generic/list.h"

#define PROTO_UDP_ORACLE 367

#define MEASURE_REQUEST 22
#define CANCEL_MEASURE_REQUEST 33

typedef struct __udp_oracle_args {
	int period_s;
}udp_oracle_args;


proto_def* udp_oracle_init(void* args);

udp_oracle_args* udp_oracle_args_init(int period_s);
void udp_oracle_args_destroy(udp_oracle_args* args);

#endif /* PROTOCOLS_IP_UTILITY_ORACLES_UDP_ORACLE_H_ */
