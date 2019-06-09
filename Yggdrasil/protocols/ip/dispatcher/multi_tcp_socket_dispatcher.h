/*
 * multi_socket_dispatcher.h
 *
 *  Created on: May 29, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_DISPATCHER_MULTI_TCP_SOCKET_DISPATCHER_H_
#define PROTOCOLS_IP_DISPATCHER_MULTI_TCP_SOCKET_DISPATCHER_H_


#include "core/protos/ip/simple_tcp_dispatcher.h"


proto_def* multi_tcp_socket_dispatcher_init(Channel* ch, void* args);

#endif /* PROTOCOLS_IP_DISPATCHER_MULTI_TCP_SOCKET_DISPATCHER_H_ */
