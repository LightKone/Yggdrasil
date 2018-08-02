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

#ifndef CONTROL_COMMAND_TCP_TREE_H_
#define CONTROL_COMMAND_TCP_TREE_H_

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

#include <arpa/inet.h>

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"

#include "remote_control/utils/commands.h"
#include "control_discovery.h"
#include "remote_control/utils/control_protocol_utils.h"


#define PROTO_CONTROL_TCP_TREE 31

typedef struct control_args_{
	short discov_id;
	short mode; //1 for new model, 0 for old
}control_args;

proto_def* control_command_tcp_tree_init(void* arg);
//void check_running_operations_completion();

#endif /* CONTROL_COMMAND_TCP_TREE_H_ */
