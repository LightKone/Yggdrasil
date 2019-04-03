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

#include "topologyManager.h"

typedef struct WLANAddr_db_ {
	short block;
	WLANAddr addr;
}WLANAddr_db;

typedef struct _topology_manager_state {
	int db_size;
	WLANAddr_db* db; //The number of nodes you have in the topology control mac address database file
	short proto_id;
}topology_manager_state;

static short process_change_link_request(YggRequest* req, topology_manager_state* state) {
	if(req->request == REQUEST && req->request_type == CHANGE_LINK){
		int tochange;
		memcpy(&tochange, req->payload, sizeof(int));
#ifdef DEBUG
		char s[500];
		bzero(s, 500);
		sprintf(s, "request to change %d", tochange);
		ygg_log("TOPOLOGY MANAGER", "REQUEST", s);
#endif
		tochange -= 1;
		if(tochange < 24 && tochange >= 0){
			if(state->db[tochange].block == 1){
				state->db[tochange].block = 0;

				dispatcher_serializeIgReq(NOT_IGNORE, state->db[tochange].addr, req);
				deliverRequest(req);
			}else{
				state->db[tochange].block = 1;

				dispatcher_serializeIgReq(IGNORE, state->db[tochange].addr, req);
				deliverRequest(req);
			}

		}
	}

	YggRequest_freePayload(req);
	return SUCCESS;
}

static void * topologyManager_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	topology_manager_state* state = args->state;
	while(1){
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_REQUEST){
			process_change_link_request(&elem.data.request, state);
		}
	}

	return NULL;
}

static void topology_manager_destroy(topology_manager_state* state) {
	free(state->db);
	free(state);
}


proto_def* topologyManager_init(void * args) {

	topology_manager_args* targs = (topology_manager_args*) args;
	topology_manager_state* state = malloc(sizeof(topology_manager_state));
	state->proto_id = PROTO_TOPOLOGY_MANAGER;
	state->db_size = targs->db_size;
	state->db = malloc(sizeof(WLANAddr_db)*state->db_size);

	FILE* f_db = NULL;
	f_db = fopen(targs->db_file_path,"r");

	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	if(f_db > 0 && f_db != NULL){

		//01 - b8:27:eb:3a:96:0e (line format)
		while ((read = getline(&line, &len, f_db)) != -1) {
			//printf("Retrieved line of length %zu :\n", read);
			//printf("%s", line);

			char index[3];
			bzero(index, 3);
			memcpy(index, line, 2);
			//printf("%d\n", atoi(index));
			char* mac = line + 5;
			//printf("%s", mac);
			if(str2wlan((char*) state->db[atoi(index) - 1].addr.data, mac) != 0){
				ygg_log("TOPOLOGY MANAGER", "ERROR", "Error retrieving wlan address");
			}
			state->db[atoi(index) - 1].block = 1;
		}

		free(line);
		line = NULL;
		fclose(f_db);
		f_db = NULL;
	}else{
		perror("fopen failed");
		ygg_log("TOPOLOGY MANAGER", "ERROR", "Unable to open mac address database file at /etc/lightkone/topologyControl/macAddrDB.txt");
		ygg_logflush();
		exit(2);
	}
#ifdef DEBUG
	ygg_log("TOPOLOGY MANAGER", "ALIVE", "Populated mac address database");
#endif
	f_db = fopen(targs->neighbours_file_path,"r");

	if(f_db > 0 && f_db != NULL){

		//01 (line format)
		while ((read = getline(&line, &len, f_db)) != -1) {

			state->db[atoi(line) - 1].block = 0;

		}

		free(line);
		line = NULL;
		fclose(f_db);
		f_db = NULL;
	}else{

		for(int i = 0; i < 24; i++){
			state->db[i].block = 0;
		}
	}

	YggRequest req;
	YggRequest_init(&req, state->proto_id, PROTO_DISPATCH, REQUEST, DISPATCH_IGNORE_REQ);

	int i;
	for(i = 0; i < 24; i++) {
		if(state->db[i].block == 1){
			//TODO optimize to send only one request
			dispatcher_serializeIgReq(IGNORE, state->db[i].addr, &req);
			deliverRequest(&req);

			YggRequest_freePayload(&req);
		}

	}

	proto_def* topology = create_protocol_definition(state->proto_id, "Topology Manager", state, (Proto_destroy) topology_manager_destroy);
	if(targs->run_on_executor)
		proto_def_add_request_handler(topology, (YggRequest_handler) process_change_link_request);
	else
		proto_def_add_protocol_main_loop(topology, (Proto_main_loop) topologyManager_main_loop);

	return topology;
}


topology_manager_args* topology_manager_args_init(int db_size, char* db_file_path, char* neighbours_file_path, bool run_on_executor) {
	topology_manager_args* args = malloc(sizeof(topology_manager_args));
	args->db_size = db_size;
	args->db_file_path = db_file_path;
	args->neighbours_file_path = neighbours_file_path;
	args->run_on_executor = run_on_executor;
	return args;
}
void topology_manager_args_destroy(topology_manager_args* args) {
	free(args);
}

