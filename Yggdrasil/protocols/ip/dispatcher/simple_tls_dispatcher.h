/*
 * simple_tls_dispatcher.h
 *
 *  Created on: May 26, 2019
 *      Author: akos
 */

#ifndef PROTOCOLS_IP_DISPATCHER_SIMPLE_TLS_DISPATCHER_H_
#define PROTOCOLS_IP_DISPATCHER_SIMPLE_TLS_DISPATCHER_H_

#include "core/protos/ip/simple_tcp_dispatcher.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

proto_def* simple_tls_dispatcher_init(Channel* ch, void* args);

#endif /* PROTOCOLS_IP_DISPATCHER_SIMPLE_TLS_DISPATCHER_H_ */
