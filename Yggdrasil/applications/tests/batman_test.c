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
#include "protocols/communication/routing/batman.h"
#include "protocols/utility/topologyManager.h"

#include "interfaces/routing/routing_interface.h"


static int get_random_destination(int number) {
	int dest = number;
	while(dest == number) {
		dest = rand()%3 + 1;
	}
	return dest;
}

int main(int argc, char* argv[]) {

	NetworkConfig* ntconf = defineNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

	ygg_runtime_init(ntconf);

	topology_manager_args* args = topology_manager_args_init(24, DB_FILE_PATH, NEIGHS_FILE_PATH, true);
	registerYggProtocol(PROTO_TOPOLOGY_MANAGER, topologyManager_init, args);
	topology_manager_args_destroy(args);

	batman_args* bargs = batman_args_init(false, false, 2, 0, 5, DEFAULT_BATMAN_WINDOW_SIZE, 3);
	registerProtocol(PROTO_ROUTING_BATMAN, batman_init, bargs);
	batman_args_destroy(bargs);

	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");
	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	YggTimer timer;
	YggTimer_init(&timer, myId, myId);
	YggTimer_set(&timer, 2, 0, 2, 0);
	setupTimer(&timer);

	int my_number = getTestValue();
	uuid_t myid;
	getmyId(myid);
	char str[37];
	uuid_unparse(myid, str);

	YggMessage msg;
	YggMessage_initBcast(&msg, myId);
	char hello[200];
	bzero(hello, 200);
	sprintf(hello, "hello from %s", str);
	YggMessage_addPayload(&msg, hello, strlen(hello)+1);

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_MESSAGE) {
			printf("%s\n", elem.data.msg.data);
		}

		if(elem.type == YGG_TIMER) {
			request_specific_route_message(PROTO_ROUTING_BATMAN, &msg, get_random_destination(my_number));
		}

		free_elem_payload(&elem);
	}

}
