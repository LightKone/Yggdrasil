/*
 * xbot_test.c
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */


#include "core/ygg_runtime.h"

#include "protocols/ip/membership/xbot.h"
#include "protocols/ip/utility/oracles/udp_oracle.h"

void main(int argc, char* argv[]) {

	char* myIp = argv[1];
	unsigned short myport = atoi(argv[2]);
	char* contact = argv[3];
	unsigned short contactport = atoi(argv[4]);

	NetworkConfig* ntconf = defineIpNetworkConfig(myIp, myport, TCP, 10, 0);

	ygg_runtime_init(ntconf);

	overrideDispatcherProtocol(simple_tcp_dispatcher_init, NULL);

	xbot_args* a = xbot_args_init(contact, contactport, 4, 7, 4, 2, 2, 3, 2, 0, 1, 0, 3, 2, 0, 10, PROTO_UDP_ORACLE, 0.5);
	registerProtocol(PROTO_XBOT, xbot_init, a);
	xbot_args_destroy(a);

	udp_oracle_args* oracle = udp_oracle_args_init(10);
	registerProtocol(PROTO_UDP_ORACLE, udp_oracle_init, oracle);
	udp_oracle_args_destroy(oracle);

	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");

	app_def_add_consumed_events(myapp, PROTO_XBOT, OVERLAY_NEIGHBOUR_UP);
	app_def_add_consumed_events(myapp, PROTO_XBOT, OVERLAY_NEIGHBOUR_DOWN);

	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_EVENT) {
			IPAddr ip;
			void* ptr = YggEvent_readPayload(&elem.data.event, NULL, ip.addr, 16);
			YggEvent_readPayload(&elem.data.event, ptr, &ip.port, sizeof(unsigned short));
			char s[50];
			bzero(s, 50);
			sprintf(s, "%s %d", ip.addr, ip.port);

			if(elem.data.event.notification_id == OVERLAY_NEIGHBOUR_UP) {
				ygg_log("MYAPP", "NEIGHBOUR UP", s);
			} else if (elem.data.event.notification_id == OVERLAY_NEIGHBOUR_DOWN) {
				ygg_log("MYAPP", "NEIGHBOUR DOWN", s);
			}
		}

		//do nothing for now
	}

}
