/*
 * multi_socket_dispatcher.h
 *
 *  Created on: May 29, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_DISPATCHER_MULTI_TCP_SOCKET_DISPATCHER_H_
#define PROTOCOLS_IP_DISPATCHER_MULTI_TCP_SOCKET_DISPATCHER_H_

#include <errno.h>

#include "core/protos/ip/simple_tcp_dispatcher.h"

#define ASYNC_MSG_SENT 44

typedef struct __multi_tcp_dispatch_args {
    int msg_size_threashold;
}multi_tcp_dispatch_args;

proto_def* multi_tcp_socket_dispatcher_init(Channel* ch, void* args);

#endif /* PROTOCOLS_IP_DISPATCHER_MULTI_TCP_SOCKET_DISPATCHER_H_ */
