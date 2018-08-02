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

#include "control_discovery.h"

static unsigned short n_neighbor;
static control_neighbor* neighbors;

static struct timespec anuncePeriod;
static short protoID;

static void* control_discovery_main_loop(main_loop_args* args) { //argument is the time for the anuncement

	queue_t* inBox = args->inBox;
	YggMessage msg;
	YggMessage_initBcast(&msg, protoID);
	WLANAddr *myAddr = malloc(sizeof(WLANAddr));
	setMyAddr(myAddr);
	YggMessage_addPayload(&msg, (char*) myAddr->data, WLAN_ADDR_LEN);
	free(myAddr);
	char* myIP = NULL;
	while(myIP == NULL || myIP[0] == '\0')
		myIP = (char*) getChannelIpAddress();
	ygg_log_stdout("CONTROL_DISCOVERY", "INIT", myIP);
	YggMessage_addPayload(&msg, myIP, 16);

	directDispatch(&msg);

	YggTimer announce;
	YggTimer_init(&announce, protoID, protoID);
	//printf("Setting announce timer, for %ld\n", anuncePeriod.tv_sec);
	YggTimer_set(&announce, anuncePeriod.tv_sec, anuncePeriod.tv_nsec, 0, 0);

	struct timespec lastPeriod;
	lastPeriod.tv_sec = anuncePeriod.tv_sec;
	lastPeriod.tv_nsec = anuncePeriod.tv_nsec;

	setupTimer(&announce);

	queue_t_elem elem;

	int continue_operation = 1;

	while(1){
		queue_pop(inBox, &elem);

		if(elem.type == YGG_TIMER){
			//ygg_log_stdout("CONTROL_DISCOVERY", "ANOUUNCE", "");
			if(continue_operation) {

				directDispatch(&msg);
				if(lastPeriod.tv_sec * 2 < 3600)
					lastPeriod.tv_sec = lastPeriod.tv_sec * 2;
				//printf("Sent announce msg, next in %ld\n", lastPeriod.tv_sec);
				YggTimer_set(&announce, lastPeriod.tv_sec + (rand() % 2), lastPeriod.tv_nsec, 0, 0);
				setupTimer(&announce);
			}

		}else if(elem.type == YGG_MESSAGE) {
			YggMessage* recvMsg = &(elem.data.msg);
			control_neighbor* candidate = malloc(sizeof(control_neighbor));

			memcpy(candidate->mac_addr.data, recvMsg->data, WLAN_ADDR_LEN);
			memcpy(candidate->ip_addr, recvMsg->data+WLAN_ADDR_LEN, 16);

			int knownAddr = 0;
			control_neighbor* n = neighbors;
			while(knownAddr == 0 && n != NULL) {
				if(memcmp(n->mac_addr.data, candidate->mac_addr.data, WLAN_ADDR_LEN) == 0)
					knownAddr = 1;
				n = n->next;
			}

			//printf("Received announce msg\n");

			if(knownAddr == 0) {
				candidate->next = neighbors;
				neighbors = candidate;
				n_neighbor++;

				if(continue_operation) {
					//cancel the times
					cancelTimer(&announce);

					//setup the times for new times
					lastPeriod.tv_sec = anuncePeriod.tv_sec;
					lastPeriod.tv_nsec = anuncePeriod.tv_nsec;
					YggTimer_set(&announce, (rand() % lastPeriod.tv_sec) + 2, lastPeriod.tv_nsec, 0, 0);
					setupTimer(&announce);
				}

				YggEvent event;
				event.proto_origin = protoID;
				event.notification_id = NEW_NEIGHBOR_IP_NOTIFICATION;
				event.length = 16;
				event.payload = candidate->ip_addr;

				deliverEvent(&event);
			}

		}else if(elem.type == YGG_REQUEST){
			if(elem.data.request.request == REQUEST) {
				if(elem.data.request.request_type == DISABLE_DISCOVERY) {
					ygg_log_stdout("CONTROL_DISCOVERY", "YGG_REQUEST RECEIVED", "DISABLE DISCOVERY");
					if(continue_operation != 0) {
						continue_operation = 0;
						cancelTimer(&announce);

						//cant kill dispatch in new model (other protocol will still be running over dispatcher)
//						elem.data.request.request_type = DISPATCH_SHUTDOWN;
//						elem.data.request.proto_dest = 0; //Dispatch
//						deliverRequest(&elem.data.request);
					}

				} else if (elem.data.request.request_type == ENABLE_DISCOVERY) {
					ygg_log_stdout("CONTROL_DISCOVERY", "YGG_REQUEST RECEIVED", "ENABLE DISCOVERY");
					if(continue_operation != 1) {
						continue_operation = 1;
						//setup the times for new times
						lastPeriod.tv_sec = anuncePeriod.tv_sec;
						lastPeriod.tv_nsec = anuncePeriod.tv_nsec;
						YggTimer_set(&announce, (rand() % lastPeriod.tv_sec) + 2, lastPeriod.tv_nsec, 0, 0);
						setupTimer(&announce);
					}
				}
			}
		}
	}

	return NULL;
}

proto_def* control_discovery_init(void* arg) {

	n_neighbor = 0;
	neighbors = NULL;

	anuncePeriod = *((struct timespec*)arg); //will it work??
	protoID = PROTO_CONTROL_DISCOVERY;

	proto_def* control_discovery = create_protocol_definition(PROTO_CONTROL_DISCOVERY, "Control Discovery", NULL, NULL);
	proto_def_add_produced_events(control_discovery, 1); //NEW_NEIGHBOR_IP_NOTIFICATION
	proto_def_add_protocol_main_loop(control_discovery, &control_discovery_main_loop);

	return control_discovery;
}
