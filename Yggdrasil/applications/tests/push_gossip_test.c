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

#include "Yggdrasil_lowlvl.h"
#include "core/ygg_runtime.h"
#include "protocols/communication/broadcast/push_gossip.h"
#include "protocols/utility/topologyManager.h"


int main(int argc, char* argv[]) {

	NetworkConfig* ntconf = defineNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

	ygg_runtime_init(ntconf);

	topology_manager_args* args = topology_manager_args_init(24, DB_FILE_PATH, NEIGHS_FILE_PATH, true);
	registerYggProtocol(PROTO_TOPOLOGY_MANAGER, topologyManager_init, args);
	topology_manager_args_destroy(args);

	push_gossip_args* pargs = push_gossip_args_init(true, 500 * 1000/*us*/, 1000 * 1000 /*us*/, false);
	registerProtocol(PROTO_PGOSSIP_BCAST, push_gossip_init, pargs);
	push_gossip_args_destroy(pargs);

	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");
	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	YggTimer timer;
	YggTimer_init(&timer, myId, myId);
	YggTimer_set(&timer, 2, 0, 2, 0);
	setupTimer(&timer);

	uuid_t myid;
	getmyId(myid);
	char str[37];
	uuid_unparse(myid, str);

	char hello[200];
	bzero(hello, 200);
	sprintf(hello, "hello from %s", str);


	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_MESSAGE) {
			printf("%s\n", elem.data.msg.data);
		}

		if(elem.type == YGG_TIMER) {
			YggRequest req;
			YggRequest_init(&req, myId, PROTO_PGOSSIP_BCAST, REQUEST, PG_REQ);
			YggRequest_addPayload(&req, hello, strlen(hello) +1);
			deliverRequest(&req);
		}

		free_elem_payload(&elem);
	}

}
