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
#include "protocols/aggregation/MiRAge.h"
#include "protocols/aggregation/flow_updating.h"
#include "interfaces/aggregation/aggregation_operations.h"

int main(int argc, char* argv[]) {

	NetworkConfig* ntconf = defineNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

	ygg_runtime_init(ntconf);

	fault_detector_discovery_args* fdargs = fault_detector_discovery_args_init(2,0,2,2);
	registerProtocol(PROTO_FAULT_DETECTOR_DISCOVERY_ID, fault_detector_discovery_init, fdargs);
	fault_detector_discovery_args_destroy(fdargs);


	pre_registerProtocol(PROTO_AGG_MULTI_ROOT, multi_root_init, NULL);

	pre_registerProtocol(PROTO_AGG_FLOW_UPDATING_ID, flow_updating_init, NULL);


	short agg_proto = PROTO_AGG_MULTI_ROOT;
	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");
	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	multi_root_agg_args* margs = multi_root_agg_args_init(PROTO_FAULT_DETECTOR_DISCOVERY_ID, true, TEST_INPUT, OP_SUM, 2, 0);
	startProtocol(agg_proto, margs);
	multi_root_agg_args_destroy(margs);

	YggTimer timer;
	YggTimer_init(&timer, myId, myId);
	YggTimer_set(&timer, 2, 0, 2, 0);
	setupTimer(&timer);

	YggTimer stop;
	YggTimer_init(&stop, myId, myId);
	YggTimer_set(&stop, 10, 0, 10, 0);
	setupTimer(&stop);

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_TIMER) {
			if(uuid_compare(elem.data.timer.id, timer.id) == 0) {
				YggRequest req;
				YggRequest_init(&req, myId, agg_proto, REQUEST, AGG_GET);
				deliverRequest(&req);
			}
			else {
				if(agg_proto == PROTO_AGG_MULTI_ROOT) {
					stopProtocol(agg_proto);
					sleep(2); //make sure previous protocol is unregistered in the runtime (concurrency issues may arrise)
					agg_proto = PROTO_AGG_FLOW_UPDATING_ID;
					flow_updating_args* args = flow_updating_args_init(PROTO_FAULT_DETECTOR_DISCOVERY_ID, true, TEST_INPUT, 2, 0);
					startProtocol(agg_proto, args);
					flow_updating_args_destroy(args);
				} else {
					stopProtocol(agg_proto);
					sleep(2); //make sure previous protocol is unregistered in the runtime (concurrency issues may arrise)
					agg_proto = PROTO_AGG_MULTI_ROOT;
					multi_root_agg_args* margs = multi_root_agg_args_init(PROTO_FAULT_DETECTOR_DISCOVERY_ID, true, TEST_INPUT, OP_SUM, 2, 0);
					startProtocol(agg_proto, margs);
					multi_root_agg_args_destroy(margs);
				}
			}
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
