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

#include "discovery_events.h"

void send_event_neighbour_up(short proto_id, uuid_t neighbour_id, WLANAddr* neighbour_addr) {
	YggEvent ev;
	YggEvent_init(&ev, proto_id, NEIGHBOUR_UP);
	YggEvent_addPayload(&ev, neighbour_id, sizeof(uuid_t));
	YggEvent_addPayload(&ev, neighbour_addr->data, WLAN_ADDR_LEN);

	deliverEvent(&ev);
	YggEvent_freePayload(&ev);
}

void send_event_neighbour_down(short proto_id, uuid_t neighbour_id) {
	YggEvent ev;
	YggEvent_init(&ev, proto_id, NEIGHBOUR_DOWN);
	YggEvent_addPayload(&ev, neighbour_id, sizeof(uuid_t));

	deliverEvent(&ev);
	YggEvent_freePayload(&ev);
}
