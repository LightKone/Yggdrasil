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

#ifndef PROTOCOLS_DISPATCHER_DISPATCHER_LOGGER_H_
#define PROTOCOLS_DISPATCHER_DISPATCHER_LOGGER_H_

#include <pthread.h>

#include "core/utils/queue.h"
#include "core/ygg_runtime.h"
#include "Yggdrasil_lowlvl.h"
#include "core/protos/dispatcher.h"

typedef struct _dispatcher_logger_args {
	int log_period_s;
}dispatcher_logger_args;

proto_def* dispatcher_logger_init(Channel* ch, void* args);

#endif /*PROTOCOLS_DISPATCHER_DISPATCHER_LOGGER_H_ */
