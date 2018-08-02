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

#include "control_protocol_server.h"

static queue_t* inBox;
static short protoID;
static short commandTreeProtoID;

#define TIMEOUT_SEC 60
#define TIMEOUT_NSEC 0

static int executeSimpleOperation(int clientSocket, int OPERATION_CODE) {
	YggRequest req;
	req.length = 0;
	req.payload = NULL;
	req.proto_dest = commandTreeProtoID;
	req.proto_origin = protoID;
	req.request = REQUEST;
	req.request_type = OPERATION_CODE;

	deliverRequest(&req);

	char* reply = malloc(1000);
	bzero(reply, 1000);

	queue_t_elem elem;
	struct timespec timeout;
	while(1) {
		timeout.tv_sec = time(NULL) + 10;
		timeout.tv_nsec = TIMEOUT_NSEC;
		if( queue_try_timed_pop(inBox, &timeout, &elem) != 1 ) {
			//A timeout has happened;
			//Two lines have been commented to disable timeout
			sprintf(reply,"Operation timed out %d sec %d nsec", TIMEOUT_SEC, TIMEOUT_NSEC);
			break;
		} else {
			//Process the response;
			if(elem.type == YGG_REQUEST && elem.data.request.request_type == OPERATION_CODE
					&& elem.data.request.request == REPLY && elem.data.request.length >= 1) {
				char* payload = (char*) elem.data.request.payload;
				if(payload[0] == '0') {
					sprintf(reply, "Command Executed.");
				} else if (payload[0] == '1') {
					sprintf(reply, "Error executing command");
				} else {
					sprintf(reply, "Unexpected value in the reply %c", payload[0]);
				}
				break;
			} else {
				continue;
			}
		}
		if(elem.type == YGG_REQUEST && elem.data.request.length > 0 && elem.data.request.payload != NULL)
			free(elem.data.request.payload);
	}

	int replyLenght = strlen(reply) + 1;

	int ret = (writefully(clientSocket, &replyLenght, sizeof(int)) > 0 && writefully(clientSocket, reply, replyLenght) > 0 ? 0 : -1);

	free(reply);

	return ret;

}

static int executeGenericOperation(int clientSocket, int OPERATION_CODE, char* payload, int payloadsize) {
	YggRequest req;
	req.length = payloadsize;
	req.payload = payload;
	req.proto_dest = commandTreeProtoID;
	req.proto_origin = protoID;
	req.request = REQUEST;
	req.request_type = OPERATION_CODE;

	deliverRequest(&req);

	if(payload != NULL)
		free(payload);

	char* reply;

	queue_t_elem elem;
	struct timespec timeout;
	while(1) {
		timeout.tv_sec = time(NULL) + TIMEOUT_SEC;
		timeout.tv_nsec = TIMEOUT_NSEC;
		if( queue_try_timed_pop(inBox, &timeout, &elem) != 1 ) {
			//A timeout has happened;

			//Commented to disable the timeout operation
			reply = malloc(100);
			bzero(reply, 100);
			sprintf(reply,"Operation timed out %d sec %d nsec", TIMEOUT_SEC, TIMEOUT_NSEC);
			break;
		} else {
			//Process the response;
			if(elem.type == YGG_REQUEST && elem.data.request.request_type == OPERATION_CODE
					&& elem.data.request.request == REPLY && elem.data.request.length >= 1) {
				reply = malloc(elem.data.request.length);
				memcpy(reply, elem.data.request.payload, elem.data.request.length);
				break;
			} else {
				continue;
			}
		}
		if(elem.type == YGG_REQUEST && elem.data.request.length > 0 && elem.data.request.payload != NULL)
			free(elem.data.request.payload);
	}

	int replyLenght = strlen(reply) + 1;

	int ret = (writefully(clientSocket, &replyLenght, sizeof(int)) > 0 && writefully(clientSocket, reply, replyLenght) > 0 ? 0 : -1);

	free(reply);

	return ret;
}

static int executeOperationWithPayloadAndConfirmation (int clientSocket, int OPERATION_CODE) {
	char* requestPayload = NULL;
	int requestPayloadSize = 0;

	if(readfully(clientSocket, &requestPayloadSize, sizeof(int)) > 0) {
		requestPayload = malloc(requestPayloadSize);
		if(readfully(clientSocket, requestPayload, requestPayloadSize) > 0) {
			return executeGenericOperation(clientSocket, OPERATION_CODE, requestPayload, requestPayloadSize);
		}
	}

	return -1;
}

static int executeOperationWithConfirmation(int clientSocket, int OPERATION_CODE) {
	return executeGenericOperation(clientSocket, OPERATION_CODE, NULL, 0);
}


static void executeClientSession(int clientSocket, struct sockaddr_in* addr) {

	char clientIP[16];
	inet_ntop(AF_INET, &addr->sin_addr, clientIP, 16);
	printf("CONTROL PROTOCOL SERVER - Starting session for a client located with address %s:%d\n", clientIP, addr->sin_port);

	int command_code;
	int lenght;
	int stop = 0;
	char* reply = malloc(50);

	while(stop == 0) {
		bzero(reply, 50);
		if(readfully(clientSocket, &command_code, sizeof(int)) <= 0) { stop = 1; continue; }

		switch(command_code) {
		case SETUP:
			stop = executeSimpleOperation(clientSocket, SETUP);
			break;
		case IS_UP:
			stop = executeOperationWithConfirmation(clientSocket, IS_UP);
			break;
		case KILL:
			stop = executeOperationWithPayloadAndConfirmation(clientSocket, KILL);
			break;
		case RUN:
			stop = executeOperationWithPayloadAndConfirmation(clientSocket, RUN);
			break;
		case REMOTE_CHANGE_LINK:
			stop = executeOperationWithPayloadAndConfirmation(clientSocket, REMOTE_CHANGE_LINK);
			break;
		case REMOTE_CHANGE_VAL:
			stop = executeOperationWithPayloadAndConfirmation(clientSocket, REMOTE_CHANGE_VAL);
			break;
		case REBOOT:
			stop = executeOperationWithConfirmation(clientSocket, REBOOT);
			break;
		case LOCAL_ENABLE_DISC:
			stop = executeOperationWithConfirmation(clientSocket, LOCAL_ENABLE_DISC);
			break;
		case LOCAL_DISABLE_DISC:
			stop = executeOperationWithConfirmation(clientSocket, LOCAL_DISABLE_DISC);
			break;
		case SHUTDOWN:
			stop = executeOperationWithConfirmation(clientSocket, SHUTDOWN);
			break;
		case GET_NEIGHBORS:
			stop = executeOperationWithConfirmation(clientSocket, GET_NEIGHBORS);
			break;
		case DEBUG_NEIGHBOR_TABLE:
			stop = executeSimpleOperation(clientSocket, DEBUG_NEIGHBOR_TABLE);
			break;
		default:
			sprintf(reply,"Unknown Command");
			lenght = strlen(reply) + 1;
			if( writefully(clientSocket, &lenght, sizeof(int)) <= 0 ||
					writefully(clientSocket, reply, lenght) <= 0) {
				stop = 1;
				continue;
			}
		}

	}

	printf("CONTROL PROTOCOL SERVER - Terminating current session with client\n");
	close(clientSocket);
}

static void * control_protocol_server_main_loop(main_loop_args* args) {

	inBox = args->inBox;
	int listen_socket = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	bzero(&address, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons((unsigned short) 5000);

	if(bind(listen_socket, (const struct sockaddr*) &address, sizeof(address)) == 0 ) {

		if(listen(listen_socket, 20) < 0) {
			perror("CONTROL PROTOCOL SERVER - Unable to setup listen on socket: ");
			return NULL;
		}

		while(1) {
			//Main control cycle... we handle a single client each time...
			bzero(&address, sizeof(struct sockaddr_in));
			unsigned int length = sizeof(struct sockaddr_in);
			int client_socket = accept(listen_socket, (struct sockaddr*) &address, &length);

			executeClientSession(client_socket, &address);
		}

	} else {
		perror("CONTROL PROTOCOL SERVER - Unable to bind on listen socket: ");
		return NULL;
	}

	return NULL;
}

proto_def* control_protocol_server_init(void * args) {

	protoID = PROTO_CONTROL_SERVER;
	commandTreeProtoID = *((unsigned short*) args);

	proto_def* control_server = create_protocol_definition(PROTO_CONTROL_SERVER, "Control Server", NULL, NULL);
	proto_def_add_protocol_main_loop(control_server, &control_protocol_server_main_loop);

	return control_server;
}
