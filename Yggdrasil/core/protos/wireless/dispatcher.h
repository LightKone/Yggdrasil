/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include <pthread.h>

#include "core/utils/queue.h"
#include "core/ygg_runtime.h"
#include "Yggdrasil_lowlvl.h"

typedef enum dispatcher_requests_ {
	DISPATCH_IGNORE_REQ = 0,
	DISPATCH_SHUTDOWN = 1
}dispatcher_requests;

typedef enum dispatcher_ignore_ {
	NOT_IGNORE = 0,
	IGNORE = 1
}dispatcher_ignore;

typedef struct dispatcher_ignore_request_ {
	dispatcher_ignore ignore;
	WLANAddr src;
}dispatch_ignore_req;

void dispatcher_serializeIgReq(dispatcher_ignore ignore, WLANAddr src, YggRequest* req);

proto_def* dispatcher_init(Channel* ch);

#endif /* DISPATCHER_H_ */
