/*
 * hyparview.h
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_MEMBERSHIP_HYPARVIEW_H_
#define PROTOCOLS_IP_MEMBERSHIP_HYPARVIEW_H_

#include "core/ygg_runtime.h"
#include "data_structures/generic/list.h"

#define  PROTO_HYPARVIEW 166

typedef enum _hyparview_events {
	OVERLAY_NEIGHBOUR_UP,
	OVERLAY_NEIGHBOUR_DOWN
}hyparview_events;


typedef struct __hyparview_args {
	IPAddr contact; //this could be optimized to be a list of contacts

	int max_active; //param: maximum active nodes (degree of random overlay)
	int max_passive; //param: maximum passive nodes
	short ARWL; //param: active random walk length
	short PRWL; //param: passive random walk length

	short k_active; //param: number of active nodes to exchange on shuffle
	short k_passive; //param: number of passive nodes to exchange on shuffle

	short shuffle_period_s;
	long shuffle_period_ns;

	short backoff_s;
	long backoff_ns;
}hyparview_args;

hyparview_args* hyparview_args_init(const char* contact, unsigned short contact_port, int max_active, int max_passive, short ARWL, short PRWL, short k_active, short k_passive, short shuffle_period_s, long shuffle_period_ns, short backoff_s, long backoff_ns);
void hyparview_args_destroy(hyparview_args* args);

proto_def* hyparview_init(void* args);


#endif /* PROTOCOLS_IP_MEMBERSHIP_HYPARVIEW_H_ */
