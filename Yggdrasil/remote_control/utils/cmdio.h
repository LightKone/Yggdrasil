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

#ifndef TOOLS_UTILS_CMDIO_H_
#define TOOLS_UTILS_CMDIO_H_

#include <stdlib.h>
#include <stdio.h>

#include "remote_control/utils/control_protocol_utils.h"

int executeCommand(CONTROL_COMMAND_TREE_REQUESTS commandCode, int sock);
char* getResponse(int sock);
int executeCommandWithStringArgument(CONTROL_COMMAND_TREE_REQUESTS commandCode, const char* command, int sock);

#endif /* TOOLS_UTILS_CMDIO_H_ */
