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
#include "protocols/discovery/simple_discovery.h"


int main(int argc, char* argv[]) {

	NetworkConfig* ntconf = defineNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

	ygg_runtime_init(ntconf);

	simple_discovery_args* sargs = simple_discovery_args_init(true, 2, 0);
	registerProtocol(PROTO_SIMPLE_DISCOVERY_ID, simple_discovery_init, sargs);
	simple_discovery_args_destroy(sargs);

	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");
	app_def_add_consumed_events(myapp, PROTO_SIMPLE_DISCOVERY_ID, NEIGHBOUR_UP);
	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_EVENT) {
			uuid_t id;
			void* ptr = YggEvent_readPayload(&elem.data.event, NULL, id, sizeof(uuid_t));
			char s[37];
			uuid_unparse(id, s);
			printf("Found neighbour %s\n", s);
		}

		free_elem_payload(&elem);
	}

}
