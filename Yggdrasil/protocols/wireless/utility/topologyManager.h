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

#ifndef TOPOLOGYMANAGER_H_
#define TOPOLOGYMANAGER_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "core/ygg_runtime.h"
#include "core/proto_data_struct.h"

#include "Yggdrasil_lowlvl.h"

#define DB_FILE_PATH "/etc/lightkone/topologyControl/macAddrDB.txt"
#define NEIGHS_FILE_PATH "/etc/lightkone/topologyControl/neighs.txt"

typedef enum topologyMan_REQ {
	CHANGE_LINK
}topologyMan_req;

typedef struct _topology_manager_args {
	int db_size;
	char* db_file_path;
	char* neighbours_file_path;
	bool run_on_executor;
}topology_manager_args;


proto_def* topologyManager_init(void * args);

topology_manager_args* topology_manager_args_init(int db_size, char* db_file_path, char* neighbours_file_path, bool run_on_executor);
void topology_manager_args_destroy(topology_manager_args* args);

#endif /* TOPOLOGYMANAGER_H_ */
