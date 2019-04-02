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

#include "proto_data_struct.h"

static void* readPayload(void* payload, unsigned short payloadLen, void* ptr, void* buffer, unsigned short toRead) {
	if(payload != NULL) {
		if(ptr == NULL) { //read first element
			ptr = payload;
			memcpy(buffer, ptr, toRead);
			ptr += toRead;
			return ptr;
		} else if((ptr - payload) + toRead <= payloadLen){ //read in the middle
			memcpy(buffer, ptr, toRead);
			ptr += toRead;
			return ptr;
		} else
			return NULL; //no more to read
	}else
		return NULL;
}

static void ygg_msg_payload(YggMessage* msg, char* payload, unsigned short length) {
	if(msg->data == NULL) {
		msg->data = malloc(length);
	} else {
		msg->data = realloc(msg->data, msg->dataLen + length);
	}

	memcpy(msg->data+msg->dataLen, payload, length);

	msg->dataLen += length;

}

void YggMessage_initIp(YggMessage* msg, short protoID, const char* dest_addr, unsigned short dest_port) {
	msg->header.type = IP;
	bzero(msg->header.src_addr.ip.addr, 16);
	bzero(msg->header.dst_addr.ip.addr, 16);
	memcpy(msg->header.dst_addr.ip.addr, dest_addr, strlen(dest_addr));
	msg->header.dst_addr.ip.port = dest_port;
	msg->Proto_id = protoID;
	msg->dataLen = 0;
	msg->data = NULL;
}

void YggMessage_initBcast(YggMessage* msg, short protoID) {
	msg->header.type = MAC;
	setBcastAddr(&msg->header.dst_addr.mac_addr);
	msg->Proto_id = protoID;
	msg->dataLen = 0;
	msg->data = NULL;
}

void YggMessage_init(YggMessage* msg, unsigned char addr[6], short protoID) {
	msg->header.type = MAC;
	memcpy(msg->header.dst_addr.mac_addr.data, addr, WLAN_ADDR_LEN);
	msg->Proto_id = protoID;
	msg->dataLen = 0;
	msg->data = NULL;
}

int YggMessage_addPayload(YggMessage* msg, char* payload, unsigned short payloadLen) {
	if(msg->header.type == MAC && msg->dataLen + payloadLen > YGG_MESSAGE_PAYLOAD)
		return FAILED;

	ygg_msg_payload(msg, payload, payloadLen);
	return SUCCESS;
}

void* YggMessage_readPayload(YggMessage* msg, void* ptr, void* buffer, unsigned short toRead) {
	return readPayload(msg->data, msg->dataLen, ptr, buffer, toRead);
}

void YggMessage_freePayload(YggMessage* msg) {
	if(msg->dataLen > 0 && msg->data != NULL) {
		free(msg->data);
		msg->data = NULL;
		msg->dataLen = 0;
	}
}

void YggTimer_init(YggTimer* timer, short protoOrigin, short protoDest) {
	genUUID(timer->id);
	timer->proto_origin = protoOrigin;
	timer->proto_dest = protoDest;
	timer->length = 0;
	timer->payload = NULL;
	timer->timer_type = -1;

	timer->config.first_notification.tv_sec = 0;
	timer->config.first_notification.tv_nsec = 0;

	timer->config.repeat_interval.tv_sec = 0;
	timer->config.repeat_interval.tv_nsec = 0;
}

void YggTimer_init_with_uuid(YggTimer* timer, uuid_t uuid, short protoOrigin, short protoDest) {
	memcpy(timer->id, uuid, sizeof(uuid_t));
	timer->proto_origin = protoOrigin;
	timer->proto_dest = protoDest;
	timer->length = 0;
	timer->payload = NULL;
	timer->timer_type = -1;
}

void YggTimer_set(YggTimer* timer, time_t firstNotification, unsigned long firstNotification_ns, time_t repeat, unsigned long repeat_ns) {

	clock_gettime(CLOCK_REALTIME, &timer->config.first_notification);

	timer->config.first_notification.tv_sec += firstNotification;
	setNanoTime(&timer->config.first_notification, firstNotification_ns);

	if(repeat > 0 || repeat_ns > 0) {
		timer->config.repeat_interval.tv_sec = repeat;
		setNanoTime(&timer->config.repeat_interval, repeat_ns);
	}
	else {
		timer->config.repeat_interval.tv_sec = 0;
		timer->config.repeat_interval.tv_nsec = 0;
	}
}

void YggTimer_setType(YggTimer* timer, short type) {
	timer->timer_type = type;
}

void YggTimer_setPayload(YggTimer* timer, void* payload, unsigned short payloadLen) {
	if(timer->payload != NULL)
		free(timer->payload);

	timer->payload = malloc(payloadLen);

	memcpy(timer->payload, payload, payloadLen);

	timer->length = payloadLen;
}

void YggTimer_addPayload(YggTimer* timer, void* payload, unsigned short payloadLen) {
	if(timer->payload == NULL){
		timer->payload = malloc(payloadLen);
	}else{
		timer->payload = realloc(timer->payload, timer->length + payloadLen);
	}

	memcpy(timer->payload+timer->length, payload, payloadLen);

	timer->length += payloadLen;
}

void* YggTimer_readPayload(YggTimer* timer, void* ptr, void* buffer, unsigned short toRead) {
	return readPayload(timer->payload, timer->length, ptr, buffer, toRead);
}

void YggTimer_freePayload(YggTimer* timer) {
	if(timer->length > 0 && timer->payload != NULL) {
		free(timer->payload);
		timer->payload = NULL;
		timer->length = 0;
	}
}

void YggEvent_init(YggEvent* ev, short protoOrigin, short notification_id) {
	ev->proto_origin = protoOrigin;
	ev->proto_dest = -1;
	ev->notification_id = notification_id;
	ev->length  = 0;
	ev->payload = NULL;
}

void YggEvent_addPayload(YggEvent* ev, void* payload, unsigned short payloadLen) {
	if(ev->payload == NULL){
		ev->payload = malloc(payloadLen);
	}else{
		ev->payload = realloc(ev->payload, ev->length + payloadLen);
	}

	memcpy(ev->payload+ev->length, payload, payloadLen);

	ev->length += payloadLen;
}

void* YggEvent_readPayload(YggEvent* ev, void* ptr, void* buffer, unsigned short toRead) {
	return readPayload(ev->payload, ev->length, ptr, buffer, toRead);
}

void YggEvent_freePayload(YggEvent* ev) {
	if(ev->length > 0 && ev->payload != NULL) {
		free(ev->payload);
		ev->payload = NULL;
		ev->length = 0;
	}
}

void YggRequest_init(YggRequest* req, short protoOrigin, short protoDest, request_type request, short request_id) {
	req->proto_origin = protoOrigin;
	req->proto_dest = protoDest;
	req->request = request;
	req->request_type = request_id;
	req->payload = NULL;
	req->length = 0;
}

void YggRequest_addPayload(YggRequest* req, void* payload, unsigned short payloadLen) {
	if(req->payload == NULL){
		req->payload = malloc(payloadLen);
	}else{
		req->payload = realloc(req->payload, req->length + payloadLen);
	}

	memcpy(req->payload+req->length, payload, payloadLen);

	req->length += payloadLen;
}

void* YggRequest_readPayload(YggRequest* req, void* ptr, void* buffer, unsigned short toRead) {

	return readPayload(req->payload, req->length, ptr, buffer, toRead);
}

void YggRequest_freePayload(YggRequest* req) {
	if(req->length > 0 && req->payload != NULL) {
		free(req->payload);
		req->payload = NULL;
		req->length = 0;
	}
}

int pushPayload(YggMessage* msg, char* buffer, unsigned short len, short protoID, WLANAddr* newDest) {
	if(msg->header.type != MAC) {
		ygg_log("RUNTIME", "UNSUPORTED", "push payload is not yet prepared to deal with ip msgs"); //TODO
		return FAILED;
	}

	unsigned short newPayloadSize = len + (sizeof(unsigned short) * 3) + WLAN_ADDR_LEN + msg->dataLen;

	if(newPayloadSize > YGG_MESSAGE_PAYLOAD)
		return FAILED;


	void* newdata = malloc(newPayloadSize);
	void* tmp = newdata;
	memcpy(tmp, &len, sizeof(unsigned short));
	tmp += sizeof(unsigned short);
	memcpy(tmp, buffer, len);
	tmp += len;
	memcpy(tmp, &msg->Proto_id, sizeof(unsigned short));
	tmp += sizeof(unsigned short);
	memcpy(tmp, msg->header.dst_addr.mac_addr.data, WLAN_ADDR_LEN);
	tmp += WLAN_ADDR_LEN;
	memcpy(tmp, &msg->dataLen, sizeof(unsigned short));
	tmp += sizeof(unsigned short);

	memcpy(tmp, msg->data, msg->dataLen);
	free(msg->data);

	msg->data = newdata;
	msg->dataLen = newPayloadSize;
	msg->Proto_id = protoID;
	memcpy(msg->header.dst_addr.mac_addr.data, newDest->data, WLAN_ADDR_LEN);

	return SUCCESS;
}

int popPayload(YggMessage* msg, char* buffer, unsigned short readlen) {
	if(msg->header.type != MAC) {
		ygg_log("RUNTIME", "UNSUPORTED", "pop payload is not yet prepared to deal with ip msgs"); //TODO
		return FAILED;
	}

	unsigned short readBytes = 0;
	void* tmp = msg->data;

	memcpy(&readBytes, tmp, sizeof(unsigned short));
	tmp += sizeof(unsigned short);

	if(readBytes > readlen)
		return -1;

	memcpy(buffer, tmp, readBytes);
	tmp += readBytes;

	msg->dataLen = msg->dataLen - sizeof(unsigned short) - readBytes;
	if(msg->dataLen > 0) {
		memcpy(&msg->Proto_id, tmp, sizeof(unsigned short));
		tmp += sizeof(unsigned short);
		memcpy(msg->header.dst_addr.mac_addr.data, tmp, WLAN_ADDR_LEN);
		tmp += WLAN_ADDR_LEN;
		unsigned short payloadLen = 0;
		memcpy(&payloadLen, tmp, sizeof(unsigned short));
		tmp += sizeof(unsigned short);
		msg->dataLen = msg->dataLen - ((sizeof(unsigned short) * 2) + WLAN_ADDR_LEN);
#ifdef DEBUG
		if(payloadLen != msg->dataLen) {
			char s[2000];
			memset(s, 0, 2000);
			sprintf(s, "Warning, enclosing message has %u bytes but was expected to have %u bytes instead\n",msg->dataLen, payloadLen);
			ygg_log("YGGMESSAGE", "WARNING", s);
		}
#endif
		char remainingPayload[payloadLen];
		memcpy(remainingPayload, tmp, payloadLen);

		msg->data = realloc(msg->data, payloadLen);
		memcpy(msg->data, remainingPayload, payloadLen);
		msg->dataLen = payloadLen;
	}

	return readBytes;
}

int pushEmptyPayload(YggMessage* msg, short protoID) {

	unsigned short newPayloadSize = msg->dataLen + 2*sizeof(short);
	if(newPayloadSize > YGG_MESSAGE_PAYLOAD)
		return FAILED;

	void* tmp = malloc(newPayloadSize);
	memcpy(tmp, &msg->Proto_id, sizeof(unsigned short));
	tmp += sizeof(unsigned short);

	memcpy(tmp, &msg->dataLen, sizeof(unsigned short));
	tmp += sizeof(unsigned short);

	memcpy(tmp, msg->data, msg->dataLen);
	free(msg->data);

	msg->data = tmp;
	msg->dataLen = newPayloadSize;
	msg->Proto_id = protoID;

	return SUCCESS;
}

int popEmptyPayload(YggMessage* msg) {

	unsigned short readBytes = 0;
	void* tmp = msg->data;

	if(msg->dataLen > 0) {
		memcpy(&msg->Proto_id, tmp, sizeof(unsigned short));
		tmp += sizeof(unsigned short);

		unsigned short payloadLen = 0;
		memcpy(&payloadLen, tmp, sizeof(unsigned short));
		tmp += sizeof(unsigned short);
		msg->dataLen = msg->dataLen - (sizeof(unsigned short)*2);

#ifdef DEBUG
		if(payloadLen != msg->dataLen) {
			char s[2000];
			memset(s, 0, 2000);
			sprintf(s, "Warning, enclosing message has %u bytes but was expected to have %u bytes instead\n",msg->dataLen, payloadLen);
			ygg_log("YGGMESSAGE", "WARNING", s);
		}
#endif
		char remainingPayload[payloadLen];
		memcpy(remainingPayload, tmp, payloadLen);

		msg->data = realloc(msg->data, payloadLen);
		memcpy(msg->data, remainingPayload, payloadLen);
		msg->dataLen = payloadLen;
	}

	return readBytes;
}

