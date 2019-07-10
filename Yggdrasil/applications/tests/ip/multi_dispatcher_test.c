/*
 * multi_dispatcher_test.c
 *
 *  Created on: May 30, 2019
 *      Author: akos
 */


#include "core/ygg_runtime.h"

#include "protocols/ip/membership/xbot.h"
#include "protocols/ip/utility/oracles/udp_oracle.h"

#include "protocols/ip/dissemination/plumtree.h"

#include "protocols/ip/dispatcher/multi_tcp_socket_dispatcher.h"

void main(int argc, char* argv[]) {

	char* myIp = argv[1];
	unsigned short myport = atoi(argv[2]);
	char* contact = argv[3];
	unsigned short contactport = atoi(argv[4]);

	int transmit = atoi(argv[5]);
	int build_tree = atoi(argv[6]);

	NetworkConfig* ntconf = defineIpNetworkConfig(myIp, myport, TCP, 10, 0);

	ygg_runtime_init(ntconf);

	overrideDispatcherProtocol(multi_tcp_socket_dispatcher_init, NULL);


    hyparview_args* a = hyparview_args_init(contact, contactport, 4, 7, 4, 2, 2, 3, 2, 0, 1, 0);
    registerProtocol(PROTO_HYPARVIEW, hyparview_init, a);
    hyparview_args_destroy(a);

    plumtree_args* p = plumtree_args_init(0, 5, 0, PROTO_HYPARVIEW);
    registerProtocol(PROTO_PLUMTREE, plumtree_init, p);
    plumtree_args_destroy(p);


	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");

	app_def_add_consumed_events(myapp, PROTO_XBOT, OVERLAY_NEIGHBOUR_UP);
	app_def_add_consumed_events(myapp, PROTO_XBOT, OVERLAY_NEIGHBOUR_DOWN);

	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();
	YggTimer t;
	YggTimer_init(&t, myId, myId);
	if(build_tree)
		YggTimer_set(&t, 2, 0, 2, 0);
	else
		YggTimer_set(&t, 60, 0, 2, 0);

	if(transmit)
		setupTimer(&t);

	char hello_msg[65000];
	unsigned int seq_num = 0;


	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_TIMER) {

			bzero(hello_msg, 65000);
			while(strlen(hello_msg) < 64000)
			    sprintf(hello_msg + strlen(hello_msg), " hello ");
			sprintf(hello_msg + strlen(hello_msg), "Hello from %s  %d  seq num: %d", myIp, myport, seq_num);
			int hello_msg_len = strlen(hello_msg)+1;
			seq_num ++;

			YggRequest req;
			YggRequest_init(&req, myId, PROTO_PLUMTREE, REQUEST, PLUMTREE_BROADCAST_REQUEST);
			YggRequest_addPayload(&req, hello_msg, hello_msg_len);
			deliverRequest(&req);
			YggRequest_freePayload(&req);
			char len[4];
			bzero(len, 4);
			sprintf(len, "%d", hello_msg_len);
            ygg_log("APP", "REQUEST MSG", len);
		}
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

        if(elem.type == YGG_MESSAGE) {

            char len[4];
            bzero(len, 4);
            sprintf(len, "%d", elem.data.msg.dataLen);

            ygg_log("APP", "RECEIVE", len);
            YggMessage_freePayload(&elem.data.msg);
        }

		//do nothing for now
	}

}
