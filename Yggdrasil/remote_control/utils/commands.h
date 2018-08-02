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

#ifndef CONTROL_COMMANDS_H_
#define CONTROL_COMMANDS_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <wordexp.h>
#include <pthread.h>

#include "protocols/utility/topologyManager.h"
#include "interfaces/aggregation/aggregation_operations.h"
#include "control_protocol_utils.h"

int do_command(char* executable, char* exec_args[], int nargs);

void* start_experience(char* command);

void* old_start_experience(char* command);

void stop_experience(char* command);

void old_stop_experience(char* command);

void process_change_link(char* command);

void process_change_val(char* command);

void sudo_reboot();

void sudo_shutdown();

#endif /* CONTROL_COMMANDS_H_ */
