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

#include "src_wireless/Yggdrasil_wireless_lowlvl.h"
#include "core/ygg_runtime.h"
#include "protocols/wireless/discovery/fault_detector_discovery.h"
#include "protocols/wireless/aggregation/MiRAge.h"

#include "interfaces/aggregation/aggregation_operations.h"

int main(int argc, char* argv[]) {

	NetworkConfig* ntconf = defineWirelessNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

	ygg_runtime_init(ntconf);

	fault_detector_discovery_args* fdargs = fault_detector_discovery_args_init(2,0,2,2);
	registerProtocol(PROTO_FAULT_DETECTOR_DISCOVERY_ID, fault_detector_discovery_init, fdargs);
	fault_detector_discovery_args_destroy(fdargs);

	multi_root_agg_args* margs = multi_root_agg_args_init(PROTO_FAULT_DETECTOR_DISCOVERY_ID, false, TEST_INPUT, OP_SUM, 2, 0);
	registerProtocol(PROTO_AGG_MULTI_ROOT, multi_root_init, margs);
	multi_root_agg_args_destroy(margs);

	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");
	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	YggTimer timer;
	YggTimer_init(&timer, myId, myId);
	YggTimer_set(&timer, 2, 0, 2, 0);
	setupTimer(&timer);
	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_TIMER) {
			YggRequest req;
			YggRequest_init(&req, myId, PROTO_AGG_MULTI_ROOT, REQUEST, AGG_GET);
			deliverRequest(&req);
		} else if (elem.type == YGG_REQUEST) {
			YggRequest* reply = &elem.data.request;
			double input;
			double result;
			void* ptr = YggRequest_readPayload(reply, NULL, &input, sizeof( double));
			YggRequest_readPayload(reply, ptr, &result, sizeof(double));

			printf("input: %f result: %f\n", input, result);
		}

		free_elem_payload(&elem);
	}

}
