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

#ifndef CORE_PROTOS_EXECUTOR_H_
#define CORE_PROTOS_EXECUTOR_H_

#include <pthread.h>
#include <stdlib.h>
#include <uuid/uuid.h>

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"
#include "core/proto_data_struct.h"

typedef enum executor_requests_{
	EXECUTOR_STOP_PROTOCOL,
}executor_requests;

void unregister_proto_handlers(short proto_id, void* state);

void register_proto_handlers(proto_def* protocol, void* state);

proto_def* executor_init(void* args);


#endif /* CORE_PROTOS_EXECUTOR_H_ */
