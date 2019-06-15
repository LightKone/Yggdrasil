/*
 * tls_test.c
 *
 *  Created on: May 26, 2019
 *      Author: akos
 */

#include "core/ygg_runtime.h"

#include "protocols/ip/dispatcher/simple_tls_dispatcher.h"

void main(int argc, char* argv[]) {

	char* myIp = argv[1];
	unsigned short myport = atoi(argv[2]);
	char* contact = argv[3];
	unsigned short contactport = atoi(argv[4]);

	NetworkConfig* ntconf = defineIpNetworkConfig(myIp, myport, TCP, 10, 0);

	ygg_runtime_init(ntconf);

	overrideDispatcherProtocol(simple_tls_dispatcher_init, NULL);


	short myId = 400;

	app_def* myapp = create_application_definition(myId, "MyApp");

	queue_t* inBox = registerApp(myapp);

	ygg_runtime_start();

	YggTimer t;
	YggTimer_init(&t, myId, myId);

	YggTimer_set(&t, 2, 0, 0, 500000000);


	setupTimer(&t);

	char hello_msg[200];
	unsigned int seq_num = 0;

	while(true) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_TIMER) {

			bzero(hello_msg, 200);
			sprintf(hello_msg, "Hello from %s  %d  seq num: %d", myIp, myport, seq_num);
			int hello_msg_len = strlen(hello_msg)+1;
			seq_num ++;

			YggMessage msg;
			YggMessage_initIp(&msg, myId, contact, contactport);
			YggMessage_addPayload(&msg, hello_msg, hello_msg_len);
			dispatch(&msg);
			YggMessage_freePayload(&msg);

			ygg_log("APP", "SENT", hello_msg);

		}

		if(elem.type == YGG_MESSAGE) {

			ygg_log("APP", "RECEIVE", elem.data.msg.data);
			YggMessage_freePayload(&elem.data.msg);
		}

		//do nothing for now
	}

}
