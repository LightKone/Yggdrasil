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

#include <stdlib.h>

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"

#include "remote_control/protocols/control_command_tcp_tree.h"
#include "remote_control/protocols/control_discovery.h"
#include "remote_control/protocols/control_protocol_server.h"
#include "protocols/utility/topologyManager.h"


int main(int argc, char* argv[]) {

	char* type = "AdHoc"; //should be an argument
	NetworkConfig* ntconf = defineNetworkConfig(type, 0, 5, 0, "ledge", YGG_filter);


	//Init ygg_runtime and protocols
	ygg_runtime_init(ntconf);

	signal(SIGPIPE, SIG_IGN);

	//addYggProtocol(PROTO_TOPOLOGY_MANAGER, &topologyManager_init, NULL, 0);

	short controlDiscovery = PROTO_CONTROL_DISCOVERY;
	short controlTree = PROTO_CONTROL_TCP_TREE;
	struct timespec anunceTime;
	anunceTime.tv_sec = 5;
	anunceTime.tv_nsec = 0;

	control_args cargs;
	cargs.discov_id = controlDiscovery;
	cargs.mode = 0;

	registerYggProtocol(controlDiscovery, &control_discovery_init, &anunceTime);
	registerYggProtocol(PROTO_CONTROL_TCP_TREE, &control_command_tcp_tree_init, &cargs);
	registerYggProtocol(PROTO_CONTROL_SERVER, &control_protocol_server_init, &controlTree);

	//Start ygg_runtime
	ygg_runtime_start();

	while(1) {
		printf("Control Structure Test online...\n");
		sleep(30);
	}

	return 0;
}

