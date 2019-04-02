/*
 * simple_tcp_dispatcher.h
 *
 *  Created on: Apr 2, 2019
 *      Author: akos
 */

#ifndef CORE_PROTOS_IP_SIMPLE_TCP_DISPATCHER_H_
#define CORE_PROTOS_IP_SIMPLE_TCP_DISPATCHER_H_


#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"

#include "data_structures/generic/list.h"

#define CLOSE_CONNECTION 123

typedef enum __tcp_dispatcher_notifications {
	TCP_DISPATCHER_CONNECTION_UP,
	TCP_DISPATCHER_CONNECTION_DOWN,
	TCP_DISPATCHER_UNABLE_TO_CONNECT
}tcp_dispatcher_notifications;

proto_def* simple_tcp_dispatcher_init(Channel* ch, void* args);


#endif /* CORE_PROTOS_IP_SIMPLE_TCP_DISPATCHER_H_ */
