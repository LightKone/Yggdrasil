/*
 * plumtree.h
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_DISSIMINATION_PLUMTREE_H_
#define PROTOCOLS_IP_DISSIMINATION_PLUMTREE_H_

#include "core/ygg_runtime.h"
#include "protocols/ip/membership/hyparview.h"

#include "utils/hashfunctions.h"


#define PROTO_PLUMTREE 232

#define PLUMTREE_BROADCAST_REQUEST 77

typedef struct __plumtree_args {
	int fanout;

	unsigned short timeout_s;
	long timeout_ns;

	unsigned short membership_id;
}plumtree_args;


proto_def* plumtree_init(void* args);

plumtree_args* plumtree_args_init(int fanout, unsigned short timeout_s, long timeout_ns, unsigned short membership_id);
void plumtree_args_destroy(plumtree_args* args);

#endif /* PROTOCOLS_IP_DISSIMINATION_PLUMTREE_H_ */
