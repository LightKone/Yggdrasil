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

#include "routing_interface.h"

static void trasnlate_number_to_id(int node_number, uuid_t node_id) {
	char uuid_txt[37];
	sprintf(uuid_txt, "66600666-1001-1001-1001-0000000000%02d", node_number);

	uuid_parse(uuid_txt, node_id);
}

int request_route_message(YggMessage* msg, int node_number) {
	return request_specific_route_message(PROTO_ROUTING, msg, node_number);
}

int request_specific_route_message(int routing_proto_id, YggMessage* msg, int node_number) {
	YggRequest req;

	YggRequest_init(&req, msg->Proto_id, routing_proto_id, REQUEST, SEND_MESSAGE);

	uuid_t node_id;
	trasnlate_number_to_id(node_number, node_id);

	YggRequest_addPayload(&req, node_id, sizeof(uuid_t));
	YggRequest_addPayload(&req, &msg->dataLen, sizeof(unsigned short));
	YggRequest_addPayload(&req, msg->data, msg->dataLen);

	int r = deliverRequest(&req);

	YggRequest_freePayload(&req);

	return r;
}

void unload_request_route_message(YggRequest* req, YggMessage* msg, uuid_t destination) {

	void* ptr = YggRequest_readPayload(req, NULL, destination, sizeof(uuid_t));
	ptr = YggRequest_readPayload(req, ptr, &msg->dataLen, sizeof(unsigned short));
	YggRequest_readPayload(req, ptr, msg->data, msg->dataLen);
	msg->Proto_id = req->proto_origin;


}
