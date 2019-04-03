/*
 * xbot.h
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_MEMBERSHIP_XBOT_H_
#define PROTOCOLS_IP_MEMBERSHIP_XBOT_H_

#include "core/ygg_runtime.h"
#include "data_structures/generic/list.h"
#include "data_structures/generic/ordered_list.h"

#include "protocols/ip/membership/hyparview.h"
#include "protocols/ip/utility/oracles/udp_oracle.h"

#define  PROTO_XBOT 166


typedef struct __xbot_args {

	hyparview_args* hyparview_conf;

	unsigned short passive_scan_length;
	unsigned short unbiased_neighs;

	unsigned short oracle_id;
	double oracle_min_val;
	double oracle_max_val;
	double optimization_threshold;
}xbot_args;

xbot_args* xbot_args_init(const char* contact, unsigned short contact_port, int max_active, int max_passive, short ARWL, short PRWL, short k_active, short k_passive, short shuffle_period_s, long shuffle_period_ns, short backoff_s, long backoff_ns, unsigned short passive_scan_length,unsigned short unbiased_neighs, double oracle_min_val, double oracle_max_val, unsigned short oracle_id, double optimization_threshold);
void xbot_args_destroy(xbot_args* args);

proto_def* xbot_init(void* args);


#endif /* PROTOCOLS_IP_MEMBERSHIP_XBOT_H_ */
