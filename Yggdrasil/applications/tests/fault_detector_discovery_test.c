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
#include "protocols/discovery/fault_detector_discovery.h"

int main(int argc, char* argv[]) {

	NetworkConfig* ntconf = defineNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

	ygg_runtime_init(ntconf);

	fault_detector_discovery_args* fdargs = fault_detector_discovery_args_init(2,0,2,2);
	registerProtocol(PROTO_FAULT_DETECTOR_DISCOVERY_ID, fault_detector_discovery_init, fdargs);
	fault_detector_discovery_args_destroy(fdargs);

	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");
	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		free_elem_payload(&elem);
	}

}
