/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * André Rosa (af.rosa@campus.fct.unl.pt)
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#ifndef PROTOCOLS_COMMUNICATION_BROADCAST_PUSH_GOSSIP_H_
#define PROTOCOLS_COMMUNICATION_BROADCAST_PUSH_GOSSIP_H_

#define PROTO_PGOSSIP_BCAST 140

#include <uuid/uuid.h>
#include <stdlib.h>
#include <time.h>

#include "core/ygg_runtime.h"
#include "Yggdrasil_lowlvl.h"
#include "core/utils/utils.h"

#include "data_structures/generic/list.h"

typedef struct _push_gossip_args {
	bool avoid_BCastStorm;
	unsigned long default_timeout;
	int gManSchedule;
	bool run_on_executor;
} push_gossip_args;

typedef enum pg_requests_ {
	PG_REQ
} pg_requests;

proto_def * push_gossip_init(void * args);

push_gossip_args* push_gossip_args_init(bool avoid_BCastStorm, unsigned long default_timeout, int gManSchedule, bool run_on_executor);
void push_gossip_args_destroy(push_gossip_args* args);

#endif /* PROTOCOLS_COMMUNICATION_BROADCAST_PUSH_GOSSIP_H_ */
