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

#ifndef CONTROL_PROTOCOL_SERVER_H_
#define CONTROL_PROTOCOL_SERVER_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"

#include "control_command_tcp_tree.h"
#include "remote_control/utils/control_protocol_utils.h"

#define PROTO_CONTROL_SERVER 33

proto_def* control_protocol_server_init(void* args);

#endif /* CONTROL_PROTOCOL_SERVER_H_ */
