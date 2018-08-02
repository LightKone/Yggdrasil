/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#include <string.h>
#include <stdlib.h>

#include "core/ygg_runtime.h"

#include "Yggdrasil_lowlvl.h"

int main(int argc, char* agrv[]){

	//define a network configuration
	//use adhoc mode, default frequency (2412), scan the network 1 time to find and connect to a network called "ledge"
	//setup the YGG_filter in the kernel so that only receive messages tagged with YGG
	NetworkConfig* ntconf = defineNetworkConfig("AdHoc", DEFAULT_FREQ, 1, 0, "ledge", YGG_filter);

	//setup the runtime
	//the runtime will always setup the dispatcher protocol (to send and receive messages to and from the network)
	//the timer protocol (to setup timers within the framework)
	//and the protocol executor
	ygg_runtime_init(ntconf);

	//register the application in the runtime
	//create an application definition, where the application module (this program) will be configured
	short myid = 400;
	app_def* myapp = create_application_definition(myid, "My application");
	//register the application in the runtime, it will create a queue through where the communication among other protocols is possible
	queue_t* inBox = registerApp(myapp);

	//start the protocols
	//the runtime will lauch a thread per registered protocol (except applications, those are handle by the programmer)
	ygg_runtime_start();

	//In this example we will set a timer to fire every 2 seconds and send a message
	YggTimer timer;
	YggTimer_init(&timer, myid, myid); //this will generate an uuid (unique identifier) for the timer and set the timer destination to the app
	YggTimer_set(&timer, 2, 0, 2, 0); //set the timer to go off in 2 seconds and 0 nano seconds and repeat for every 2 seconds and 0 nanoseconds

	setupTimer(&timer); //setup the timer in the runtime, when the timer expires it will be delivered to the destination (in this case the app)

	int sequence_number = 0;

	while(1){
		queue_t_elem elem;

		//try to get something from the queue
		//the queue_t is a blocking priority queue
		queue_pop(inBox, &elem);


		//an element can be of 4 types
		if(elem.type == YGG_MESSAGE){
			//YGG_MESSAGE (a message from the network)
			//if the message was delivered to here, it was probably sent by the same protocol/app in another device
			YggMessage msg = elem.data.msg; //the message received from the inBox

			//write a log with the contents of the message and from who it was from
			char m[200];
			memset(m, 0, 200);
			char addr[33]; //a mac address as 33 characters
			memset(addr, 0, 33);
			wlan2asc(&msg.srcAddr, addr); //auxiliary function that translates the mac address from machine to human readable form
			sprintf(m, "Message from %s content: %s", addr, msg.data);
			ygg_log("ONE HOP BCAST", "RECEIVED MESSAGE", m);

		} else if(elem.type == YGG_TIMER) {
			//YGG_TIMER (a timer that was fired)
			YggTimer recv_timer = elem.data.timer;

			//a protocol can have many timers requested, to distinguish the timers we use the uuid
			if(uuid_compare(timer.id, recv_timer.id) == 0){ //compare the uuid of the timer received with the one stored
				//create a message and send it in one hop broadcast
				YggMessage msg;
				YggMessage_initBcast(&msg, myid); //init a YggMessage set with destination to BroadCast

				char m[200];
				memset(m, 0, 200);
				sprintf(m, "Message with sequence number %d", sequence_number); //our message payload
				YggMessage_addPayload(&msg, m, strlen(m)+1); //add the payload to the message (+1 to include string terminator)

				sequence_number ++;

				dispatch(&msg); //send the message to the network

				ygg_log("ONE HOP BCAST", "SENDING MESSAGE", m);

				//for the purpose of this demonstration we will cancel the timer and reset it
				cancelTimer(&timer); //cancel the timer in the runtime (the timer will not fire)

				YggTimer_set(&timer, 3, 0, 1, 0); //reset the time in the timer (cancel will zero the fields of the YggTimer related to the the time)
				setupTimer(&timer); //set it up again in the runtime

			}
		} else if(elem.type == YGG_EVENT) {
			//YGG_EVENT (an event fired by some other protocol)
		} else if(elem.type == YGG_REQUEST){
			//YGG_REQUEST (a request or a reply sent by another protocol)
		}

	}

	return 0;
}


