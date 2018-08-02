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

#include "control_command_tcp_tree.h"

#define RECONNECTION_ATTEMPT 15 //15 seconds between reconnection attempts
#define TEST_CONNECTION_INTERVAL 30//30 seconds between tests on ACTIVE peers

static char* my_ip_addr;
static char* my_hostname;
static short protoID;

static short mode; //1 is new; 0 is old;

typedef enum tree_state_ {
	ACTIVE,
	PASSIVE,
	HOLD
}tree_state;

typedef enum connection_state_ {
	CONNECTING,
	CONNECTED,
	ERROR,
	CLOSING,
	CLOSED
}connection_state;

typedef struct peer_connection_ {
	pthread_mutex_t peer_lock;
	int socket;
	connection_state state;
	int is_in_fdset;
	tree_state tree;
	char ip_addr[16];
	char* hostname;
	struct peer_connection_* next;
	time_t next_retry_connection;
} peer_connection;

typedef struct tree_msgs_history_ {
	uuid_t msg_id;
	CONTROL_COMMAND_TREE_REQUESTS cmd;
	void* payload;
	unsigned short payload_size;
	int announces_size;
	int retransmission_requests_sent;
	peer_connection** announces;
	time_t* announces_timestamp;
	time_t received_timestamp;
	struct tree_msgs_history_ *next;
} tree_msgs_history;

typedef struct is_up_operation_ {
	CONTROL_COMMAND_TREE_REQUESTS cmd;
	uuid_t instance_id;
	short protocol_source;
	peer_connection* source;
	int number_of_failed_peers;
	int number_active_peers;
	peer_connection** active_peers;
	unsigned int* value_peers;
	char** payload_peers;
	struct is_up_operation_* next;
} is_up_operation;

static void check_running_operations_completion();

static time_t next_keep_alive_time() {
	return time(NULL) + TEST_CONNECTION_INTERVAL + (rand() % TEST_CONNECTION_INTERVAL);
}

static void set_socket_options(int socket) {
	//	struct timeval t;
	//	t.tv_sec = 4;
	//	t.tv_usec = 0;
	int activate = 1;
	//	if(setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &activate, sizeof(int) ) != 0) {
	//		ygg_log_stdout("CONTROL COMMAND TREE", "SO_NOSIGPIPE", "FAILED");
	//		perror("set_sockopt so_nosigpipe");
	//	}
	//	if(setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) != 0) {
	//		ygg_log_stdout("CONTROL COMMAND TREE", "SO_SNDTIMEO", "FAILED");
	//		perror("set_sockopt so_sndtimeo");
	//	}
	//	if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) != 0) {
	//		ygg_log_stdout("CONTROL COMMAND TREE", "SO_RCTIMEO", "FAILED");
	//		perror("set_sockopt so_rctimeo");
	//	}
	if(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &activate, sizeof(int)) != 0) {
		ygg_log_stdout("CONTROL COMMAND TREE", "SO_KEEPALIVE", "FAILED");
		perror("set_sockopt so_keepalive");
	}
}

static tree_msgs_history* create_known_entry(uuid_t msg_id, peer_connection* sender) {
	tree_msgs_history* entry = malloc(sizeof(tree_msgs_history));
	memcpy(entry->msg_id, msg_id, sizeof(uuid_t));
	entry->cmd = UNDEFINED;
	entry->payload = NULL;
	entry->payload_size = 0;
	entry->announces_size = 1;
	entry->retransmission_requests_sent = 0;
	entry->announces = malloc(sizeof(peer_connection*));
	entry->announces_timestamp = malloc(sizeof(time_t));
	entry->announces[0] = sender;
	entry->announces_timestamp[0] = time(NULL);
	entry->received_timestamp = entry->announces_timestamp[0];
	entry->next = NULL;
	return entry;
}

static tree_msgs_history* create_log_entry(tree_command* msg_header) {
	tree_msgs_history* entry = malloc(sizeof(tree_msgs_history));
	memcpy(entry->msg_id, msg_header->id, sizeof(uuid_t));
	entry->cmd = msg_header->command_code;
	entry->payload_size = msg_header->command_size;
	if(entry->payload_size > 0) {
		entry->payload = malloc(entry->payload_size);
		memcpy(entry->payload, msg_header->command, entry->payload_size);
	}else
		entry->payload = NULL;

	entry->announces_size = 0;
	entry->retransmission_requests_sent = 0;
	entry->announces = NULL;
	entry->announces_timestamp = NULL;
	entry->received_timestamp = time(NULL);
	entry->next = NULL;
	return entry;
}

static void destroy_log_entry(tree_msgs_history* entry) {
	if(entry->payload != NULL)
		free(entry->payload);
	if(entry->announces_size > 0) {
		free(entry->announces);
		free(entry->announces_timestamp);
	}
	entry->next = NULL;
	free(entry);
}

static void register_msg_reception(tree_msgs_history* entry, tree_command* msg_header) {
	if(entry->cmd == UNDEFINED) {
		entry->cmd = msg_header->command_code;
		entry->received_timestamp = time(NULL);

		if(entry->announces_size > 0) {
			free(entry->announces);
			free(entry->announces_timestamp);
			entry->announces = NULL;
			entry->announces_timestamp = NULL;
			entry->announces_size = 0;
		}
	}
}

static void add_announce_for_msg(tree_msgs_history* entry, peer_connection* sender) {
	if(entry->cmd == UNDEFINED) {
		if(entry->announces_size == 0) {
			entry->announces = malloc(sizeof(peer_connection*));
			entry->announces_timestamp = malloc(sizeof(time_t));
		} else {
			entry->announces = realloc(entry->announces, sizeof(peer_connection*) * (entry->announces_size + 1));
			entry->announces_timestamp = realloc(entry->announces_timestamp, sizeof(time_t) * (entry->announces_size + 1));
		}
		entry->announces_timestamp[entry->announces_size] = time(NULL);
		entry->announces[entry->announces_size] = sender;
		entry->announces_size++;
	}
}


static pthread_mutex_t peers_lock;
static peer_connection* peers;

static pthread_mutex_t* lock_peer(peer_connection* peer) {
	pthread_mutex_lock(&(peer->peer_lock));
	return &peer->peer_lock;
}

static pthread_mutex_t* lock_peers() {
	pthread_mutex_lock(&peers_lock);
	return &peers_lock;
}

static void unlock(pthread_mutex_t* lock) {
	pthread_mutex_unlock(lock);
}

static tree_msgs_history* history;

static void handle_peer_exception(peer_connection* p) {
	//#ifdef DEBUG
	printf("CONTROL COMMAND TREE Protocol - Exception in connection with peer %s.\n", p->ip_addr);
	//#endif DEBUG

	pthread_mutex_t* lock = lock_peer(p);
	close(p->socket);
	p->state = CLOSED;
	p->tree = HOLD;
	p->next_retry_connection = time(NULL) + RECONNECTION_ATTEMPT;
	unlock(lock);

	check_running_operations_completion();

	/**
	pthread_mutex_t* previous = lock_peers();

	peer_connection** pp = &peers;
	while(*pp != p && *pp != NULL) {
		pthread_mutex_t* current = lock_peer(*pp);
		pp = &((*pp)->next);
		unlock(previous);
		previous = current;
	}
	if(*pp != NULL) {
		pthread_mutex_t* current = lock_peer(*pp);
	 *pp = p->next;
		p->next = NULL;
		unlock(previous);
		unlock(current);
		close(p->socket);
		pthread_mutex_destroy(&(p->peer_lock));
		if(p->hostname != NULL)
			free(p->hostname);
		free(p);
	} else {
		unlock(previous);
		printf("CONTROL COMMAND TREE Protocol - could not remove peer %s. (not found)\n", p->ip_addr);
	}
	 **/
}

static is_up_operation* create_is_up_operation_instance(tree_command* msg_header, peer_connection* source, short protocol_source) {
	is_up_operation* ret = malloc(sizeof(is_up_operation));
	ret->cmd = msg_header->command_code;
	memcpy(ret->instance_id, msg_header->id, sizeof(uuid_t));
	ret->protocol_source = protocol_source;
	ret->source = source;
	ret->number_of_failed_peers = 0;
	ret->number_active_peers = 0;
	ret->active_peers = NULL;
	ret->value_peers = NULL;
	ret->payload_peers = NULL;
	ret->next = NULL;

	pthread_mutex_t* previous = lock_peers();
	peer_connection* p = peers;

	while(p!=NULL) {
		pthread_mutex_t* lock = lock_peer(p);

		if(p != source && p->state == CONNECTED) {
			if(p->tree == ACTIVE) {
				ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP REQUEST: Requesting value to peer", p->ip_addr);

				int failed_transmission = 0;
				if( write_command_header(p->socket, msg_header) > 0 ) {
					if(msg_header->command_size > 0) {
						if( write_command_body(p->socket, msg_header)  <= 0 ) {
							failed_transmission = 1;
						}
					}
				} else {
					failed_transmission = 1;
				}

				if(failed_transmission == 0) {

					ret->active_peers = (ret->number_active_peers == 0 ? malloc(sizeof(peer_connection*)) : realloc(ret->active_peers, sizeof(peer_connection*) * (ret->number_active_peers + 1)));
					ret->value_peers = (ret->number_active_peers == 0 ? malloc(sizeof(unsigned int)) : realloc(ret->value_peers, sizeof(unsigned int) * (ret->number_active_peers + 1)));
					ret->payload_peers = (ret->number_active_peers == 0 ? malloc(sizeof(char*)) : realloc(ret->payload_peers, sizeof(char*) * (ret->number_active_peers + 1 )));
					ret->active_peers[ret->number_active_peers] = p;
					ret->value_peers[ret->number_active_peers] = 0;
					ret->payload_peers[ret->number_active_peers] = NULL;

					ret->number_active_peers++;
				} else {
					handle_peer_exception(p);
				}
			} else {
				if( write_command_annouce(p->socket, msg_header) <= 0) {
					handle_peer_exception(p);
				}
			}
		}

		p = p->next;
		unlock(previous);
		previous = lock;

	}

	unlock(previous);

	return ret;
}

static int check_is_up_terminated(is_up_operation* is_up_instance) {
	int i;
	for(i = 0; i < is_up_instance->number_active_peers; i++) {
		if(is_up_instance->active_peers[i]->state == CONNECTED && is_up_instance->active_peers[i]->tree == ACTIVE && is_up_instance->value_peers[i] == 0)
			return 0;
		else if((is_up_instance->active_peers[i]->state != CONNECTED || is_up_instance->active_peers[i]->tree == PASSIVE) && is_up_instance->value_peers[i] == 0) {
			//This connection has become unstable, therefore we will mark it as no longer being required
			is_up_instance->number_of_failed_peers++;
			is_up_instance->value_peers[i] = 1;
		}
	}
	return 1;
}

static unsigned int compute_is_up_return_value(is_up_operation* is_up_instance) {
	int v = 1, i = 0;
	for(; i < is_up_instance->number_active_peers; i++ )
		v+= is_up_instance->value_peers[i];
	return v - is_up_instance->number_of_failed_peers; //Disregard peers that failed during the operation
}

static char* compute_get_neighbors_payload_to_return(is_up_operation* is_up_instance) {
	int payloadsize = strlen(my_hostname) + 2;
	char* payload = malloc(payloadsize);
	char* p = payload;
	memcpy(p, my_hostname, strlen(my_hostname));
	p+= strlen(my_hostname);
	*p=' '; p++;
	*p='['; p++;

	pthread_mutex_t* previous = lock_peers();
	peer_connection* peer = peers;
	while(peer != NULL) {
		pthread_mutex_t* lock = lock_peer(peer);
		if(peer->state == CONNECTED) {
			int namelen = strlen(peer->hostname);
			payload = realloc(payload, payloadsize + namelen + 1 + 1);
			p = payload + payloadsize;
			payloadsize = payloadsize + namelen + 1;
			memcpy(p, peer->hostname, namelen);
			p+= namelen;
			*p = ' ';
		}
		peer = peer->next;
		unlock(previous);
		previous = lock;
	}
	unlock(previous);

	*p=']';
	p++;
	*p='\n';
	payloadsize++;

	int totalpayloadsize = payloadsize;

	int i;
	for(; i < is_up_instance->number_active_peers; i++ ) {
		if(is_up_instance->payload_peers[i] != NULL) {
			totalpayloadsize += strlen(is_up_instance->payload_peers[i]);
		}
	}

	totalpayloadsize++;
	payload = realloc(payload, totalpayloadsize);
	p = payload + payloadsize;

	for(i= 0; i < is_up_instance->number_active_peers; i++ ) {
		if(is_up_instance->payload_peers[i] != NULL) {
			memcpy(p, is_up_instance->payload_peers[i], strlen(is_up_instance->payload_peers[i]));
			p+= strlen(is_up_instance->payload_peers[i]);
		}
	}
	*p='\0';
	return payload;
}

static char* compute_is_up_payload_to_return(is_up_operation* is_up_instance) {
	int i = 0;
	int payloadsize = strlen(my_hostname) + 1;
	for(; i < is_up_instance->number_active_peers; i++ ) {
		if(is_up_instance->payload_peers[i] != NULL) {
			payloadsize += strlen(is_up_instance->payload_peers[i]) + 1;
		}
	}
	char* payload = malloc(payloadsize);
	char* p = payload;
	memcpy(p, my_hostname, strlen(my_hostname));
	p+= strlen(my_hostname);
	*p='\n';
	p++;
	for(i= 0; i < is_up_instance->number_active_peers; i++ ) {
		if(is_up_instance->payload_peers[i] != NULL) {
			memcpy(p, is_up_instance->payload_peers[i], strlen(is_up_instance->payload_peers[i]));
			p+= strlen(is_up_instance->payload_peers[i]);
			*p='\n';
			p++;
		}
	}
	p--;
	*p='\0';
	return payload;
}

static void destroy_is_up_operation(is_up_operation* is_up_instance) {
	if(is_up_instance->number_active_peers > 0) {
		int i = 0;
		for(; i < is_up_instance->number_active_peers; i++) {
			if(is_up_instance->payload_peers[i] != NULL)
				free(is_up_instance->payload_peers[i]);
		}
		free(is_up_instance->payload_peers);
		free(is_up_instance->active_peers);
		free(is_up_instance->value_peers);
	}

	free(is_up_instance);
}

static void is_up_instance_register_peer_reply(is_up_operation* instance, peer_connection* p, unsigned int value, char* payload) {
	int i;

#ifdef DEBUG
	char id[37];
	char val[100];
	bzero(id, 37);
	bzero(val, 100);

	int missing = 0;
	for(i = 0; i < instance->number_active_peers; i++) {
		if(instance->active_peers[i]->state == CONNECTED && instance->active_peers[i]->tree == ACTIVE && instance->value_peers[i] == 0) {
			ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP REPLY REGISTERED - Missing:", instance->active_peers[i]->ip_addr);
			missing += 1;
		}
	}

	uuid_unparse(instance->instance_id, id);
	sprintf(val, "%s ==> %d (missing %d)",id, value, missing);

	ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP REPLY REGISTERED", val);
#endif

	for(i = 0; i < instance->number_active_peers; i++) {
		if(instance->active_peers[i] == p) {
			if(instance->value_peers[i] != 0) {
				instance->number_of_failed_peers--;
			}
			instance->value_peers[i] = value;
			instance->payload_peers[i] = payload;
			break;
		}
	}
}

static CONTROL_COMMAND_TREE_REQUESTS get_operation_reply_code(CONTROL_COMMAND_TREE_REQUESTS op) {
	switch(op) {
	case IS_UP:
		return LOCAL_IS_UP_REPLY;
		break;
	case GET_NEIGHBORS:
		return LOCAL_GET_NEIGHBORS_REPLY;
		break;
	case RUN:
		return LOCAL_RUN_REPLY;
		break;
	case KILL:
		return LOCAL_KILL_REPLY;
		break;
	case REMOTE_CHANGE_VAL:
		return LOCAL_CHANGE_VAL_REPLY;
		break;
	case REMOTE_CHANGE_LINK:
		return LOCAL_CHANGE_LINK_REPLY;
		break;
	case REBOOT:
		return LOCAL_REBOOT_REPLY;
		break;
	case SHUTDOWN:
		return LOCAL_SHUTDOWN_REPLY;
		break;
	case LOCAL_DISABLE_DISC:
		return LOCAL_DISABLE_DISC_REPLY;
		break;
	case LOCAL_ENABLE_DISC:
		return LOCAL_ENABLE_DISC_REPLY;
		break;
	default:
		return LOCAL_IS_UP_REPLY;
		break;
	}
}

static void reply_to_is_up_operation(is_up_operation* instance) {
	unsigned int ret_value = compute_is_up_return_value(instance);
	char* ret_payload = compute_is_up_payload_to_return(instance);
	int payload_size = strlen(ret_payload) + 1;

	if(instance->source != NULL) { //This request was received from a peer
		pthread_mutex_t* lock = lock_peer(instance->source);
		if(instance->source->state == CONNECTED) {
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP OPERATION TERMINATED -- UP-STREAM", instance->source->ip_addr);
#endif
			tree_command* ret_msg = init_command_header(get_operation_reply_code(instance->cmd));
			ret_msg->command_size = sizeof(uuid_t) + sizeof(unsigned int) + sizeof(int) + payload_size;
			ret_msg->command = malloc(ret_msg->command_size);
			void* p = ret_msg->command;
			memcpy(p, &ret_value, sizeof(unsigned int));
			p+= sizeof(unsigned int);
			memcpy(p, instance->instance_id, sizeof(uuid_t));
			p+= sizeof(uuid_t);
			memcpy(p, &payload_size, sizeof(int));
			p+= sizeof(int);
			memcpy(p, ret_payload, payload_size);
			write_command_header(instance->source->socket, ret_msg);
			write_command_body(instance->source->socket, ret_msg);
			destroy_command_header(ret_msg);
			//#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP OPERATION TERMINATED -- REPLY SENT TO UP-STREAM", instance->source->ip_addr);
			//#endif
		}
		unlock(lock);
	} else { //This request was received from a client, so we will return the reply directly to the client.
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP OPERATION TERMINATED -- EVENT", "");
#endif
		YggRequest request;
		request.request = REPLY;
		request.request_type = instance->cmd;
		request.proto_origin = protoID;
		request.proto_dest = instance->protocol_source;
		request.payload = malloc(payload_size + 100);
		bzero(request.payload, payload_size + 100);
		sprintf(request.payload, "Total nodes: %d\n%s\n", ret_value, ret_payload);
		request.length = strlen(request.payload) + 1;
		deliverReply(&request);
		//#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP OPERATION TERMINATED -- SENT EVENT", request.payload);
		//#endif
		free(request.payload);
	}

	free(ret_payload);
}

static void reply_to_get_neighbors_operation(is_up_operation* instance) {
	unsigned int ret_value = compute_is_up_return_value(instance);
	char* ret_payload = compute_get_neighbors_payload_to_return(instance);
	int payload_size = strlen(ret_payload) + 1;

	if(instance->source != NULL) { //This request was received from a peer
		pthread_mutex_t* lock = lock_peer(instance->source);
		if(instance->source->state == CONNECTED) {
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "GET_NEIGHBORS OPERATION TERMINATED -- UP-STREAM", instance->source->ip_addr);
#endif
			tree_command* ret_msg = init_command_header(get_operation_reply_code(instance->cmd));
			ret_msg->command_size = sizeof(uuid_t) + sizeof(unsigned int) + sizeof(int) + payload_size;
			ret_msg->command = malloc(ret_msg->command_size);
			void* p = ret_msg->command;
			memcpy(p, &ret_value, sizeof(unsigned int));
			p+= sizeof(unsigned int);
			memcpy(p, instance->instance_id, sizeof(uuid_t));
			p+= sizeof(uuid_t);
			memcpy(p, &payload_size, sizeof(int));
			p+= sizeof(int);
			memcpy(p, ret_payload, payload_size);
			write_command_header(instance->source->socket, ret_msg);
			write_command_body(instance->source->socket, ret_msg);
			destroy_command_header(ret_msg);
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "GET_NEIGHBORS OPERATION TERMINATED -- REPLY SENT TO UP-STREAM", instance->source->ip_addr);
#endif
		}
		unlock(lock);
	} else { //This request was received from a client, so we will return the reply directly to the client.
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "GET_NEIGHBORS OPERATION TERMINATED -- EVENT", "");
#endif
		YggRequest request;
		request.request = REPLY;
		request.request_type = instance->cmd;
		request.proto_origin = protoID;
		request.proto_dest = instance->protocol_source;
		request.payload = ret_payload;
		request.length = strlen(ret_payload) + 1;
		deliverReply(&request);
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP OPERATION TERMINATED -- SENT EVENT", request.payload);
#endif
	}
	free(ret_payload);
}

static is_up_operation* running_operations;

static void append_to_is_up_running_operations(is_up_operation* is_up_instance) {
	is_up_operation** tmp = &running_operations;
	while(*tmp != NULL) {
		tmp = &((*tmp)->next);
	}
	*tmp = is_up_instance;
}

static is_up_operation* remove_is_up_operation(is_up_operation* is_up_instance) {
	is_up_operation** tmp = &running_operations;
	while(*tmp != NULL && *tmp != is_up_instance) {
		tmp = &((*tmp)->next);
	}
	if(*tmp != NULL) {
		*tmp = (*tmp)->next;
		return is_up_instance;
	}
	return NULL;
}

static is_up_operation* get_is_up_instance(uuid_t instance_id) {
	is_up_operation* tmp = running_operations;
	while(tmp != NULL && uuid_compare(instance_id, tmp->instance_id) != 0) {
		tmp = tmp->next;
	}
	return tmp;
}

static void check_running_operations_completion() {
	is_up_operation** tmp = &running_operations;
	while(*tmp != NULL) {
		is_up_operation* instance = *tmp;
		if(check_is_up_terminated(instance)) {
			if(instance->cmd == IS_UP || instance->cmd == RUN || instance->cmd == KILL || instance->cmd == REMOTE_CHANGE_VAL
					|| instance->cmd == REMOTE_CHANGE_LINK || instance->cmd == REBOOT || instance->cmd == SHUTDOWN ||
					instance->cmd ==  LOCAL_DISABLE_DISC || instance->cmd == LOCAL_ENABLE_DISC) {
				reply_to_is_up_operation(instance);
			} else if(instance->cmd == GET_NEIGHBORS) {
				reply_to_get_neighbors_operation(instance);
			} else {
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PENDING OPERATION PROCESSING (by PRUNE)", "DO NOT KNOW HOW TO REPLY");
			}
			*tmp = instance->next;
			destroy_is_up_operation(instance);
		} else {
			tmp = &((*tmp)->next);
		}
	}
}

static tree_msgs_history* findEntry(uuid_t msg) {
	tree_msgs_history* tmp = history;
	while(tmp != NULL && uuid_compare(msg, tmp->msg_id) != 0) {
		tmp = tmp->next;
	}
	return tmp;
}

static void append_to_history(tree_msgs_history* entry) {
	tree_msgs_history** tmp = &history;
	while(*tmp != NULL) {
		tmp = &((*tmp)->next);
	}
	*tmp = entry;
}

static void garbageCollectHistory() {
	time_t latest = time(NULL) - (1 * 60 * 60 ); //minus one hour = 3600 seconds
	tree_msgs_history** tmp = &history;
	while(*tmp != NULL) {
		if((*tmp)->received_timestamp != 0 && (*tmp)->received_timestamp < latest) {
			tree_msgs_history* toRemove = *tmp;
			*tmp = (*tmp)->next;
			destroy_log_entry(toRemove);
		} else {
			tmp = &((*tmp)->next);
		}
	}
}


static void retransmit(peer_connection* p, tree_msgs_history* entry) {
	if(writefully(p->socket, entry->msg_id, sizeof(uuid_t)) > 0 &&
			writefully(p->socket, &entry->cmd, sizeof(CONTROL_COMMAND_TREE_REQUESTS)) > 0) {
		if(entry->payload_size > 0) {
			if( writefully(p->socket, &entry->payload_size, sizeof(unsigned short)) > 0 &&
					writefully(p->socket, entry->payload, entry->payload_size) > 0 ) {
				return;
			}
		}
	}
	handle_peer_exception(p);
}

static void request_to_retransmit(peer_connection* p, tree_msgs_history* entry) {
	if(writefully(p->socket, entry->msg_id, sizeof(uuid_t)) <= 0)
		handle_peer_exception(p);
}

static void check_missed_messages() {
	tree_msgs_history** tmp = &history;
	while(*tmp != NULL) {

		if((*tmp)->cmd == UNDEFINED && time(NULL) >= ((*tmp)->received_timestamp + 2)) { //Received announce but not message at least for 2 seconds
			peer_connection** aps = (*tmp)->announces;
			for(; (*tmp)->retransmission_requests_sent < (*tmp)->announces_size; ((*tmp)->retransmission_requests_sent)++){
				pthread_mutex_t* lock = lock_peer(aps[(*tmp)->retransmission_requests_sent]);
				if((aps[(*tmp)->retransmission_requests_sent])->state == CONNECTED) {
					//send attach msg to p;
					tree_command* attach = init_command_header(LOCAL_ATTACH);
					write_command_header(aps[(*tmp)->retransmission_requests_sent]->socket, attach);
					request_to_retransmit(aps[(*tmp)->retransmission_requests_sent], (*tmp));
					destroy_command_header(attach);
					aps[(*tmp)->retransmission_requests_sent]->tree = ACTIVE;
					aps[(*tmp)->retransmission_requests_sent]->next_retry_connection = time(NULL) + TEST_CONNECTION_INTERVAL;
					((*tmp)->retransmission_requests_sent)++; //register that we have sent another request
					unlock(lock);
					break;
				}else {
					unlock(lock);
				}

			}
		}

		tmp = &((*tmp)->next);
	}
}

static int listen_socket;

static int local_pipe[2];

static pthread_t queueMonitor;
static pthread_t acceptThread;

static short controlDiscoveryProtoID;

static YggTimer history_garbage_collector;
static YggTimer missed_messages_check;

static void* queue_monitor_thread(queue_t* inBox) {
	ygg_log_stdout("CONTROL_COMMAND_TREE", "QUEUE_MONITOR_THREAD", "ACTIVE");

	queue_t_elem elem;

	while(1) {
		queue_pop(inBox, &elem);
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "QUEUE_MONITOR_THREAD", "RECEIVED SOMETHING ON QUEUE");
#endif
		write(local_pipe[1], &elem, sizeof(queue_t_elem));
	}
}

static void shutdown_protocol() {
	ygg_log_stdout("CONTROL_COMMAND_TREE", "SHUTTING DOWN", "something unexpected happened");
	ygg_logflush();
	pthread_mutex_t* mainLock = lock_peers();
	while(peers != NULL) {
		pthread_mutex_t* lock = lock_peer(peers);
		peer_connection* p = peers;
		peers = p->next;
		p->next = NULL;
		close(p->socket);
		unlock(lock);
		pthread_mutex_destroy(&(p->peer_lock));
		free(p);
	}
	unlock(mainLock);
	pthread_mutex_destroy(mainLock);

	close(local_pipe[0]);
	close(local_pipe[1]);
	pthread_cancel(queueMonitor);

	close(listen_socket);
	pthread_cancel(acceptThread);

	pthread_exit(NULL);
}

static int isIPSmaller(char ip1[16], char ip2[16]) {
	if( strcmp(ip1, ip2) < 0)
		return 1;
	return 0;
}

static peer_connection* findPeerConnection(char ip[16]) {
	pthread_mutex_t* previous = lock_peers();
	peer_connection* p = peers;

	while(p != NULL) {
		pthread_mutex_t* current = lock_peer(p);

		if(strcmp(p->ip_addr, ip) == 0) {
			unlock(previous);
			unlock(current);
			return p;
		}
		p = p->next;
		unlock(previous);
		previous = current;
	}

	unlock(previous);
	return NULL;
}

static int isNotKnwonPeer(char ip[16]) {
	if(findPeerConnection(ip) != 0)
		return 0;
	else
		return 1;
}

static void* extablishConnectionAndDoHandshake(void* arg) {
#ifdef DEBUG
	ygg_log_stdout("CONTROL_COMMAND_TREE", "extablishConnectionAndDoHandshake", ((peer_connection*) arg)->ip_addr);
#endif
	pthread_mutex_t* lock;
	peer_connection* newPeer = (peer_connection*) arg;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
#ifdef DEBUG
	fprintf(stderr, "Binding to %s\n", my_ip_addr);
	inet_pton(AF_INET, my_ip_addr, &(addr.sin_addr));
	addr.sin_port = htons(0);
#endif

	newPeer->socket = socket(AF_INET, SOCK_STREAM, 0);
	if(newPeer->socket >= 0) {
#ifdef DEBUG
		if( bind( newPeer->socket, (const struct sockaddr*) &addr, sizeof(addr)) != 0) {
			perror("Bind of client socket failed");
		}
#endif
		set_socket_options(newPeer->socket);
	} else {
		perror("Unable to create a socket to connect to a peer.");
	}



	inet_pton(AF_INET, newPeer->ip_addr, &(addr.sin_addr));
	addr.sin_port = htons(6666);



	int try;
	int sucess = 0;
	for(try = 0; try < 5; try++) {
		//while(1) {
		if( connect(newPeer->socket, (struct sockaddr*) &addr, sizeof(addr)) == 0 ) {
			sucess = 1;
			break;
		} else {
#ifdef DEBUG
			fprintf(stderr, "Failed connection attempt to %s -> %d times\n", newPeer->ip_addr, try+1);
			perror(newPeer->ip_addr);
#endif
			usleep(rand() % 100000);
		}
	}
	if(sucess) {
		int hostsize;

		readfully(newPeer->socket, &hostsize, sizeof(int));
		newPeer->hostname = malloc(hostsize);
		readfully(newPeer->socket, newPeer->hostname, hostsize);

		writefully(newPeer->socket, my_ip_addr, 16);
		hostsize = strlen(my_hostname) + 1;
		writefully(newPeer->socket, &hostsize, sizeof(int));
		writefully(newPeer->socket, my_hostname, hostsize);

		lock = lock_peer(newPeer);

		newPeer->state = CONNECTED;
		newPeer->tree = ACTIVE;
		newPeer->next_retry_connection = time(NULL) + TEST_CONNECTION_INTERVAL;
		unlock(lock);
		ygg_log_stdout("CONTROL_COMMAND_TREE","New Peer Connection (init)", newPeer->ip_addr);
	} else {
		lock = lock_peer(newPeer);
		newPeer->state = ERROR;
		unlock(lock);
		handle_peer_exception(newPeer);
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection failed on connect()", newPeer->ip_addr);
	}

	return NULL;
}

static void extablish_new_connection_to_peer(char ip[16]) {
#ifdef DEBUG
	ygg_log_stdout("CONTROL_COMMAND_TREE", "Attempt to create connection", ip);
#endif
	peer_connection* newPeer = malloc(sizeof(peer_connection));
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&(newPeer->peer_lock), &attr);
	memcpy(newPeer->ip_addr, ip, 16);
	newPeer->state = CONNECTING;
	newPeer->is_in_fdset = 0;
	newPeer->tree = HOLD;
	newPeer->hostname = NULL;

	pthread_mutex_t* head = lock_peers();
	pthread_mutex_t* lock = peers == NULL ? NULL : lock_peer(peers);
	newPeer->next = peers;
	peers = newPeer;
	if(lock != NULL) unlock(lock);
	unlock(head);

	pthread_t connectAndHandshake;
	pthread_create(&connectAndHandshake, NULL, extablishConnectionAndDoHandshake, newPeer);
	pthread_detach(connectAndHandshake);

}

static void handle_listen_exception() {
	printf("CONTROL COMMAND TREE Protocol - Listen socket has failed, stopping everything.\n");
	shutdown_protocol();
}

static void handle_new_connection() {
#ifdef DEBUG
	ygg_log_stdout("CONTROL_COMMAND_TREE", "ACCEPT THREAD", "Attempting to accept incoming TCP connection.");
#endif
	peer_connection* newPeer = malloc(sizeof(peer_connection));
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&newPeer->peer_lock, &attr);
	newPeer->hostname = NULL;
	newPeer->state = CONNECTING;
	newPeer->is_in_fdset = 0;
	newPeer->tree = HOLD;
	struct sockaddr_in address;
	unsigned int length = sizeof(address);
	newPeer->socket = accept(listen_socket, (struct sockaddr *) &address, &length);

	int error = 1;

	if(newPeer->socket != -1) {
		error = 0;
		char socket_addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(address.sin_addr), socket_addr, INET_ADDRSTRLEN);
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Incoming socket addr", socket_addr);
#endif

		set_socket_options(newPeer->socket);

		int hostsize = strlen(my_hostname) + 1;
		if(writefully(newPeer->socket, &hostsize, sizeof(int)) <= 0){
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on write hostsize");
			perror("CONTROL COMMAND TREE Protocol - Error writing hostsize: ");
#endif
			error = 1;
		}
		else if(writefully(newPeer->socket, my_hostname, hostsize) <= 0){
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on write hostname");
			perror("CONTROL COMMAND TREE Protocol - Error writing hostname: ");
#endif
			error = 1;
		}
		else if(readfully(newPeer->socket, newPeer->ip_addr, INET_ADDRSTRLEN) <= 0) {
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on read ip addr");
			perror("CONTROL COMMAND TREE Protocol - Error reading ip addr: ");
#endif
			error = 1;
		}
		else if(readfully(newPeer->socket, &hostsize, sizeof(int)) <= 0) {
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on read hostsize");
			perror("CONTROL COMMAND TREE Protocol - Error reading hostsize: ");
#endif
			error = 1;
		}
		else
			newPeer->hostname = malloc(hostsize);
		if(!error && readfully(newPeer->socket, newPeer->hostname, hostsize) <= 0) {
#ifdef DEBUG
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on read hostname");
			perror("CONTROL COMMAND TREE Protocol - Error reading hostname: ");
#endif
			error = 1;
		}

	} else {
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on accept()");
		perror("CONTROL COMMAND TREE Protocol - Error accepting connection: ");
#endif
		pthread_mutex_destroy(&(newPeer->peer_lock));
		free(newPeer);
		//handle_listen_exception();
	}

	if(error) {
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Peer connection reception failed", "on accept()");
		//perror("CONTROL COMMAND TREE Protocol - Error accepting connection: ");
		pthread_mutex_destroy(&(newPeer->peer_lock));
		if(newPeer->hostname != NULL)
			free(newPeer->hostname);
		free(newPeer);
	} else {
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Incoming peer addr", newPeer->ip_addr);
#endif
		peer_connection* found = findPeerConnection(newPeer->ip_addr);
		if(found == NULL) { //Effective new peer
			newPeer->state = CONNECTED;
			newPeer->tree = ACTIVE;
			newPeer->next_retry_connection = time(NULL) + TEST_CONNECTION_INTERVAL;
			pthread_mutex_t* head = lock_peers();
			pthread_mutex_t* lock = peers == NULL ? NULL : lock_peer(peers);
			newPeer->next = peers;
			peers = newPeer;
			if(lock != NULL) unlock(lock);
			unlock(head);
		} else { //Peer already has a structure
			pthread_mutex_t* lock = lock_peer(found);
			found->socket = newPeer->socket;
			found->state = CONNECTED;
			found->tree = ACTIVE;
			found->next_retry_connection = time(NULL) + TEST_CONNECTION_INTERVAL;
			if(found->hostname != NULL)
				free(found->hostname);
			found->hostname = newPeer->hostname;
			pthread_mutex_destroy(&(newPeer->peer_lock));
			free(newPeer);
			newPeer = found;
			unlock(lock);
		}
		ygg_log_stdout("CONTROL_COMMAND_TREE", "New Peer Connection (recv)", newPeer->ip_addr);
	}
}

static void propagate(tree_command* cmd, peer_connection* exception) {
	ygg_log_stdout("CONTROL_COMMAND_TREE", "PROPAGATE - START", my_ip_addr);

	pthread_mutex_t* previous = lock_peers();

	peer_connection* p = peers;

	while(p != NULL) {
		pthread_mutex_t* current = lock_peer(p);
		if(p->state == CONNECTED) {
			ygg_log_stdout("CONTROL_COMMAND_TREE", "PROPAGATE - CONNECTED", p->ip_addr);
			if(p->tree == ACTIVE && p != exception) {
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PROPAGATE - SENT", p->ip_addr);
				write_command_header(p->socket, cmd);
				write_command_body(p->socket, cmd);
			} else if(p->tree == PASSIVE && p != exception) {
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PROPAGATE - ANNOUNCE", p->ip_addr);
				write_command_annouce(p->socket, cmd);
			}
		}
		p = p->next;
		unlock(previous);
		previous = current;
	}

	unlock(previous);
	ygg_log_stdout("CONTROL_COMMAND_TREE", "PROPAGATE", "END");
}

static short process_msg(peer_connection* p, tree_command* msg, short protoID) {
	YggRequest request;
	is_up_operation* instance;

	switch(msg->command_code) {
	case SETUP:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "SETUP msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		//Nothing extra to do
		return 1;
		break;
	case IS_UP:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "IS UP msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
#ifdef DEBUG
		char id[37];
		bzero(id, 37);
		uuid_unparse(msg->id, id);
		ygg_log_stdout("CONTROL_COMMAND_TREE", "IS_UP target instance", id);
#endif
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	case TEAR_DOWN:
		break;
	case DO_COMMAND:
		//PROCESS ARBITRARY BASH COMMAND
		break;
	case GET_FILE:
		break;
	case RECONFIGURE:
		break;
	case MOVE_FILE:
		break;
	case KILL:
		//Kill the nodes in the cmd payload, or all if no payload
		ygg_log_stdout("CONTROL_COMMAND_TREE", "KILL msg", msg->command);
		if(mode)
			stop_experience(msg->command);
		else
			old_stop_experience(msg->command);
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	case RUN:
		//begin the test
		ygg_log_stdout("CONTROL_COMMAND_TREE", "RUN msg", msg->command);
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		char* bashcommand = msg->command;
		msg->command_size = 0;
		msg->command = NULL;
		pthread_t launch_child;
		if(mode)
			pthread_create(&launch_child, NULL, (void * (*)(void *)) start_experience, (void*) bashcommand);
		else
			pthread_create(&launch_child, NULL, (void * (*)(void *)) old_start_experience, (void*) bashcommand);
		pthread_detach(launch_child);
		//start_experience(msg->command);
		return 0;
		break;
	case REMOTE_CHANGE_LINK:
		//change the link state of pi1 and pi2
		ygg_log_stdout("CONTROL_COMMAND_TREE", "REMOTE_CHANGE_LINK msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		process_change_link(msg->command);
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	case REMOTE_CHANGE_VAL:
		//change the input val on pi to newval
		ygg_log_stdout("CONTROL_COMMAND_TREE", "REMOTE_CHANGE_VAL msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		process_change_val(msg->command);
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	case REBOOT:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "REBOOT msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		sudo_reboot();
		return 0;
		break;
	case LOCAL_ENABLE_DISC:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "LOCAL_ENABLE_DISC msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		request.proto_origin = protoID;
		request.proto_dest = controlDiscoveryProtoID;
		request.length = 0;
		request.payload = NULL;
		request.request = REQUEST;
		request.request_type = ENABLE_DISCOVERY;
		deliverRequest(& request);
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	case LOCAL_DISABLE_DISC:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "LOCAL_DISABLE_DISC msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		request.proto_origin = protoID;
		request.proto_dest = controlDiscoveryProtoID;
		request.length = 0;
		request.payload = NULL;
		request.request = REQUEST;
		request.request_type = DISABLE_DISCOVERY;
		deliverRequest(& request);
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	case SHUTDOWN:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "SHUTDOWN msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_is_up_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		sudo_shutdown();
		return 0;
		break;
	case GET_NEIGHBORS:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "GET_NEIGHBORS msg", p != NULL ? p->ip_addr : "LOCAL REQUEST");
#ifdef DEBUG
		bzero(id, 37);
		uuid_unparse(msg->id, id);
		ygg_log_stdout("CONTROL_COMMAND_TREE", "GET NEIGHBORS target instance", id);
#endif
		instance = create_is_up_operation_instance(msg, p, protoID);
		if(check_is_up_terminated(instance)) {
			reply_to_get_neighbors_operation(instance);
			destroy_is_up_operation(instance);
		} else {
			append_to_is_up_running_operations(instance);
		}
		return 0;
		break;
	}

	return 1;
}

static void printneighbortable() {
	pthread_mutex_t* previous = lock_peers();
	peer_connection* p = peers;

	while(p != NULL) {
		pthread_mutex_t* lock = lock_peer(p);

		ygg_log_stdout(p->hostname, p->ip_addr, p->state == CONNECTED ? ( p->tree == ACTIVE ? "CONNECTED - TREE ACTIVE" : "CONNECTED - TREE PASSIVE") : "NOT CONNECTED");
		fflush(stdout);
		p = p->next;
		unlock(previous);
		previous = lock;
	}

	unlock(previous);
}

static void handle_queue_request() {
#ifdef DEBUG
	ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", "HANDLING QUEUE REQUEST");
#endif
	queue_t_elem elem;
	read(local_pipe[0], &elem, sizeof(queue_t_elem));
	is_up_operation* instance;

	if(elem.type == YGG_MESSAGE) {

	} else if(elem.type == YGG_TIMER) {
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", "YGG_TIMER");
#endif
		if(uuid_compare(elem.data.timer.id, history_garbage_collector.id) == 0) {
			garbageCollectHistory();
		}else if(uuid_compare(elem.data.timer.id, missed_messages_check.id) == 0) {
			check_missed_messages();
		}else {
			ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", "Unknown timer");
		}

		if(elem.data.timer.length > 0 && elem.data.timer.payload != NULL)
			free(elem.data.timer.payload);
	} else if(elem.type == YGG_REQUEST) {
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", "YGG_REQUEST");
#endif
		//Most likely a request from a client that is being handled by the control_protocol_server
		YggRequest* req = &elem.data.request;
		if(req->request == REQUEST) {
			switch(req->request_type) {
			tree_command* cmd = NULL;
			case SETUP:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "SETUP");
				cmd = init_command_header(SETUP);
				append_to_history(create_log_entry(cmd));
				propagate(cmd, NULL);
				destroy_command_header(cmd);
				req->request = REPLY;
				req->proto_dest = req->proto_origin;
				req->proto_origin = protoID;
				if(req->length > 0 && req->payload != NULL)
					free(req->payload);
				req->length = 1;
				req->payload = malloc(1);
				((char*) req->payload)[0] = '0';
				deliverReply(req);
				break;
			case IS_UP:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "IS_UP");
				cmd = init_command_header(IS_UP);
				append_to_history(create_log_entry(cmd));
				instance = create_is_up_operation_instance(cmd, NULL, req->proto_origin);
				destroy_command_header(cmd);
#ifdef DEBUG
				char id[37];
				bzero(id,37);
				uuid_unparse(instance->instance_id, id);
				ygg_log_stdout("CONTROL_COMMAND_TREE", "Starting IS_UP operation", id);
#endif
				if(check_is_up_terminated(instance)) {
					reply_to_is_up_operation(instance);
					destroy_is_up_operation(instance);
				} else {
					append_to_is_up_running_operations(instance);
				}
				break;
			case DO_COMMAND:
				//TODO EXEC LOCALLY
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "DO_COMMND");
				cmd = init_command_header(DO_COMMAND);
				add_command_body(cmd, elem.data.request.payload, elem.data.request.length);
				append_to_history(create_log_entry(cmd));
				break;
			case KILL:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "KILL");
				cmd = init_command_header(KILL);
				add_command_body(cmd, elem.data.request.payload, elem.data.request.length);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
#ifdef DEBUG
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PROCESSED KILL", cmd->command);
#endif
				destroy_command_header(cmd);
				break;
			case RUN:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "RUN");
				cmd = init_command_header(RUN);
				add_command_body(cmd, elem.data.request.payload, elem.data.request.length);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
#ifdef DEBUG
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PROCESSED RUN", cmd->command);
#endif
				destroy_command_header(cmd);
				break;
			case REMOTE_CHANGE_LINK:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "REMOTE_CHANGE_LINK");
				cmd = init_command_header(REMOTE_CHANGE_LINK);
				add_command_body(cmd, elem.data.request.payload, elem.data.request.length);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
#ifdef DEBUG
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PROCESSED CHANGE LINK", cmd->command);
#endif
				destroy_command_header(cmd);
				break;
			case REMOTE_CHANGE_VAL:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "REMOTE_CHANGE_VAL");
				cmd = init_command_header(REMOTE_CHANGE_VAL);
				add_command_body(cmd, elem.data.request.payload, elem.data.request.length);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
#ifdef DEBUG
				ygg_log_stdout("CONTROL_COMMAND_TREE", "PROCESSED CHANGE LINK", cmd->command);
#endif
				destroy_command_header(cmd);
				break;
			case REBOOT:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "REBOOT");
				cmd = init_command_header(REBOOT);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
				break;
			case LOCAL_ENABLE_DISC:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "ENABLE_DISC");
				cmd = init_command_header(LOCAL_ENABLE_DISC);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
				destroy_command_header(cmd);
				break;
			case LOCAL_DISABLE_DISC:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "DISABLE_DISC");
				cmd = init_command_header(LOCAL_DISABLE_DISC);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
				destroy_command_header(cmd);
				break;
			case SHUTDOWN:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "DISABLE_DISC");
				cmd = init_command_header(SHUTDOWN);
				append_to_history(create_log_entry(cmd));
				process_msg(NULL, cmd, req->proto_origin);
				destroy_command_header(cmd);
				break;
			case GET_NEIGHBORS:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "GET_NEIGHBORS");
				cmd = init_command_header(GET_NEIGHBORS);
				append_to_history(create_log_entry(cmd));
				instance = create_is_up_operation_instance(cmd, NULL, req->proto_origin);
				destroy_command_header(cmd);
#ifdef DEBUG
				bzero(id,37);
				uuid_unparse(instance->instance_id, id);
				ygg_log_stdout("CONTROL_COMMAND_TREE", "Starting GET_NEIGHBORS operation", id);
#endif
				if(check_is_up_terminated(instance)) {
					reply_to_get_neighbors_operation(instance);
					destroy_is_up_operation(instance);
				} else {
					append_to_is_up_running_operations(instance);
				}
				break;
			case DEBUG_NEIGHBOR_TABLE:
				ygg_log_stdout("CONTROL_COMMAND_TREE", "YGG_REQUEST", "DEBUG_NEIGHBOR_TABLE");
				req->request = REPLY;
				req->proto_dest = req->proto_origin;
				req->proto_origin = protoID;
				if(req->length > 0 && req->payload != NULL)
					free(req->payload);
				req->length = 1;
				req->payload = malloc(1);
				((char*) req->payload)[0] = '0';
				deliverReply(req);
				printneighbortable();
				break;
			default:
				break;
			}
		} else if(req->request == REPLY) {
			//This is a reply to a previous request over the discovery layer.
		}

		if(req->payload != NULL) free(req->payload);

	} else if(elem.type == YGG_EVENT) {
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", "YGG_EVENT");
#endif
		YggEvent* event = &elem.data.event;
#ifdef DEBUG
		char s[1000];
		sprintf(s, "PROTO %d %d; NOTF_ID %d %d", event->proto_origin, controlDiscoveryProtoID, event->notification_id, NEW_NEIGHBOR_IP_NOTIFICATION );
		ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", s);
#endif
		if(event->proto_origin == controlDiscoveryProtoID && event->notification_id == NEW_NEIGHBOR_IP_NOTIFICATION) {
			if(isIPSmaller(my_ip_addr, (char*) event->payload) && isNotKnwonPeer((char*)event->payload)) {
				extablish_new_connection_to_peer((char*)event->payload);
			}
#ifdef DEBUG
			else {
				ygg_log_stdout("CONTROL_COMMAND_TREE", "MAIN PROTOCOL", "IGNORING PEER, NOT MY RESPONSABILITY OR ALREADY EXISTS");
			}
#endif
		}

		if(elem.data.event.length > 0 && elem.data.event.payload != NULL)
			free(elem.data.event.payload);
	}
#ifdef DEBUG
	else {
		printf("CONTROL COMMAND TREE Protocol - Received unknown element type through queue: %d\n", elem.type);
	}
#endif
}

static int read_message_body(tree_command* msg, peer_connection* p) {
	switch(msg->command_code) {
	case DO_COMMAND:
	case KILL:
	case RUN:
	case REMOTE_CHANGE_LINK:
	case REMOTE_CHANGE_VAL:
		if(read_command_body(p->socket, msg) <= 0)
			return -1;
		break;
	}
	return 0;
}

static void handle_peer_communication(peer_connection* p) {
	ygg_log_stdout("CONTROL_COMMAND_TREE", "HANDLE PEER CONNECTION", p->ip_addr);

	short resend = 0;

	tree_msgs_history* target = NULL;
	tree_command* prune = NULL;
	uuid_t msg_id_to_retransmit;

	pthread_mutex_t* lock = lock_peer(p);

	tree_command* msg = read_command_header(p->socket);

#ifdef DEBUG
	char s[10];
	bzero(s, 10);
	sprintf(s, "%d",msg->command_code);
	ygg_log_stdout("CONTROL_COMMAND_TREE", "HANDLE PEER CONNECTION -> CMD CODE:", s);
#endif

	is_up_operation* instance;

	unsigned int value;
	uuid_t is_up_instance_id;
	int payloadsize;
	char* payload = NULL;
	void* ptr = NULL;

	if(msg == NULL) { //In this case the EoF of the socket was reached.
		handle_peer_exception(p);
		unlock(lock);
		return;
	}

	switch(msg->command_code) {
	case NO_OP:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle NO_OP (keep alive)", p->ip_addr);
		resend = 0;
		break;
	case LOCAL_PRUNE:
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_PRUNE message", p->ip_addr);
		p->tree = PASSIVE;
		check_running_operations_completion();
		break;
	case LOCAL_ATTACH:
		p->tree = ACTIVE;
		p->next_retry_connection = next_keep_alive_time();
		readfully(p->socket, msg_id_to_retransmit, sizeof(uuid_t));
		target = findEntry(msg_id_to_retransmit);
		if(target != NULL && target->cmd != UNDEFINED)
			retransmit(p, target);
		else
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_ATTACH message", "Received request to retransmit something that is not in my historic");
		break;
	case LOCAL_ANNOUNCE:
		if(p->tree != PASSIVE) {
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_ANNOUNCE message", "Received announce from a neighbor that is not passive (correcting)");
			p->tree = PASSIVE;
		}
		target = findEntry(msg->id);
		if(target == NULL) {
			append_to_history(create_known_entry(msg->id, p));
		} else {
			if(target->cmd == UNDEFINED) {
				add_announce_for_msg(target, p);
			} //else nothing to be done cause we already have the message locally
		}
		break;
	case LOCAL_IS_UP_REPLY:
	case LOCAL_RUN_REPLY:
	case LOCAL_KILL_REPLY:
	case LOCAL_CHANGE_VAL_REPLY:
	case LOCAL_CHANGE_LINK_REPLY:
	case LOCAL_REBOOT_REPLY:
	case LOCAL_SHUTDOWN_REPLY:
	case LOCAL_ENABLE_DISC_REPLY:
	case LOCAL_DISABLE_DISC_REPLY:
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_IS_UP_REPLY", "Going to receive message payload.");
#endif
		if(read_command_body(p->socket, msg) == -1) {
			//Error condition;
			handle_peer_exception(p);
			unlock(lock);
			return;
		}
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_IS_UP_REPLY", "Receive payload.");
#endif
		ptr = msg->command;

		memcpy(&value, ptr , sizeof(unsigned int));
		ptr += sizeof(unsigned int);
		memcpy(is_up_instance_id, ptr, sizeof(uuid_t));
		ptr += sizeof(uuid_t);
		memcpy(&payloadsize, ptr, sizeof(int));
		ptr += sizeof(int);
		payload = malloc(payloadsize);
		memcpy(payload, ptr, payloadsize);

		instance = get_is_up_instance(is_up_instance_id);
		if(instance != NULL) {
			is_up_instance_register_peer_reply(instance, p, value, payload);
			if(check_is_up_terminated(instance)) {
				reply_to_is_up_operation(instance);
				instance = remove_is_up_operation(instance);
				if(instance != NULL)
					destroy_is_up_operation(instance);
				else
					ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_IS_UP_REPLY", "Could not remove is_up_instance from list.");
			}
		} else {
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_IS_UP_REPLY", "No valid instance");
		}
		break;
	case LOCAL_GET_NEIGHBORS_REPLY:
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_GET_NEIGHBORS_REPLY", "Going to receive payload.");
#endif
		if(read_command_body(p->socket, msg) == -1) {
			//Error condition;
			handle_peer_exception(p);
			unlock(lock);
			return;
		}
#ifdef DEBUG
		ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_GET_NEIGHBORS_REPLY", "Receive payload.");
#endif

		ptr = msg->command;

		memcpy(&value, ptr , sizeof(unsigned int));
		ptr += sizeof(unsigned int);
		memcpy(is_up_instance_id, ptr, sizeof(uuid_t));
		ptr += sizeof(uuid_t);
		memcpy(&payloadsize, ptr, sizeof(int));
		ptr += sizeof(int);
		payload = malloc(payloadsize);
		memcpy(payload, ptr, payloadsize);

		instance = get_is_up_instance(is_up_instance_id);
		if(instance != NULL) {
			is_up_instance_register_peer_reply(instance, p, value, payload);
			if(check_is_up_terminated(instance)) {
				reply_to_get_neighbors_operation(instance);
				instance = remove_is_up_operation(instance);
				if(instance != NULL)
					destroy_is_up_operation(instance);
				else
					ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_GET_NEIGHBORS_REPLY", "Could not remove is_up_instance from list.");
			}
		} else {
			ygg_log_stdout("CONTROL_COMMAND_TREE", "Handle LOCAL_GET_NEIGHBORS_REPLY", "No valid instance");
		}
		break;
	default:
		if(read_message_body(msg, p) == -1) {
			handle_peer_exception(p);
			unlock(lock);
			return;
		}
		p->tree = ACTIVE;
		p->next_retry_connection = next_keep_alive_time();
		target = findEntry(msg->id);
		if(target == NULL) {
			append_to_history(create_log_entry(msg));
			resend = process_msg(p, msg, 0);
		} else {
			if(target->cmd == UNDEFINED) {
				register_msg_reception(target, msg);
				resend = process_msg(p, msg, 0);
			} else {
				//Redundant message, lets issue a prune.
				prune = init_command_header(LOCAL_PRUNE);
				write_command_header(p->socket, prune);
				p->tree = PASSIVE;
				destroy_command_header(prune);
			}
		}
		break;
	}
	unlock(lock);

	if(resend) {
		propagate(msg, p);
		destroy_command_header(msg);
	}


}

static void handle_queue_exception() {
	printf("CONTROL COMMAND TREE Protocol - Pipe has failed, unable to contact secondary thread, stopping everything.\n");
	shutdown_protocol();
}

static void* accept_thread(void* arg) {
	while(1) {
		handle_new_connection();
	}
}

typedef void* (*gen_function)(void*);

static void* control_main_loop(main_loop_args* args) { //argument is the time for the announcement

	queue_t* inBox = args->inBox;
	YggTimer_init(&history_garbage_collector, protoID, protoID); //repeat every hour (in us)
	YggTimer_set(&history_garbage_collector, ((unsigned long)1*60*60), 0, ((unsigned long)1*60*60), 0);
	setupTimer(&history_garbage_collector);

	YggTimer_init(&missed_messages_check, protoID, protoID);
	YggTimer_set(&missed_messages_check, (2), 0, (2), 0); //every 2 seconds, check tree
	setupTimer(&missed_messages_check);

	pthread_create(&queueMonitor, NULL, (gen_function) &queue_monitor_thread, (void*)inBox);

	listen_socket = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	bzero(&address, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	//address.sin_addr.s_addr = htonl(INADDR_ANY);
	inet_pton(AF_INET, my_ip_addr, &(address.sin_addr));
	address.sin_port = htons((unsigned short) 6666);

	if(bind(listen_socket, (const struct sockaddr*) &address, sizeof(address)) == 0 ) {

		if(listen(listen_socket, 20) < 0) {
			perror("CONTROL COMMAND TREE Protocol - Unable to setup listen on socket: ");
			pthread_cancel(queueMonitor);
			return NULL;
		}

		pthread_create(&acceptThread, NULL, accept_thread, NULL);

		fd_set readfds;
		fd_set exceptionfds;

		int largest_fd;

		while(1) {
			FD_ZERO(&readfds);
			FD_ZERO(&exceptionfds);

			//FD_SET(listen_socket, &readfds);
			//FD_SET(listen_socket, &exceptionfds);
			//largest_fd = listen_socket;

			FD_SET(local_pipe[0], &readfds);
			FD_SET(local_pipe[0], &exceptionfds);
			//if(largest_fd < local_pipe[0])
			largest_fd = local_pipe[0];

			pthread_mutex_t* previous = lock_peers();

			peer_connection* p = peers;

			time_t now = time(NULL);

			while(p != NULL) {
				pthread_mutex_t* current = lock_peer(p);
				if(p->state == CONNECTED) {
					FD_SET(p->socket, &readfds);
					FD_SET(p->socket, &exceptionfds);
					p->is_in_fdset = p->socket;
					if(largest_fd < p->socket) largest_fd = p->socket;
				} else if(p->state == CLOSED && p->next_retry_connection < now && isIPSmaller(my_ip_addr, p->ip_addr)) {
					pthread_t connectAndHandshake;
					p->state = CONNECTING;
					pthread_create(&connectAndHandshake, NULL, extablishConnectionAndDoHandshake, p);
					pthread_detach(connectAndHandshake);
				}
				p = p->next;
				unlock(previous);
				previous = current;
			}

			unlock(previous);

			int s = select(largest_fd+1, &readfds, NULL, &exceptionfds, NULL);

			if(s > 0) {

				//Handle read operations
				//if(FD_ISSET(listen_socket, &readfds) != 0) {
				//	handle_new_connection();
				//}
				if(FD_ISSET(local_pipe[0], &readfds) != 0) {
					handle_queue_request();
				}

				//Handle exceptions
				if(FD_ISSET(listen_socket, &exceptionfds) != 0) {
					handle_listen_exception();
				}
				if(FD_ISSET(local_pipe[0], &exceptionfds) != 0) {
					handle_queue_exception();
				}

				//Handle peer connections

				previous = lock_peers();
				p = peers;
				while(p != NULL) {
					//int receivedMessage = 0;
					pthread_mutex_t* current = lock_peer(p);
					if(p->state == CONNECTED && p->is_in_fdset == p->socket) {
						if(FD_ISSET(p->socket, &readfds) != 0) {
							//handle messages
							if(FD_ISSET(p->socket, &exceptionfds) != 0)
								handle_peer_exception(p);
							else {
								handle_peer_communication(p);
								//receivedMessage = 1;
							}
						}
						else if(FD_ISSET(p->socket, &exceptionfds) != 0) {
							//handle exceptions
							handle_peer_exception(p);
						}
					}
					/**
					//Send keep alive messages
					if(receivedMessage == 0) {
						if(p->state == CONNECTED && p->tree == ACTIVE) {
							if(p->next_retry_connection <= now) {
								fd_set writeready;
								FD_ZERO(&writeready);
								FD_SET(p->socket, &writeready);
								struct timeval timeout;
								timeout.tv_sec = 0;
								timeout.tv_usec = 0;
								int canWrite = select(p->socket+1, NULL, &writeready, NULL, &timeout);
								if(canWrite == 1) {
									tree_command* keepalive = init_command_header(NO_OP);
									if(write_command_header(p->socket, keepalive) <= 0)
										handle_peer_exception(p);
									else
										p->next_retry_connection = next_keep_alive_time();
								} else if(canWrite == 0) {
									handle_peer_exception(p);
								}
							}
						} else if(p->state == CONNECTED){
							p->next_retry_connection = next_keep_alive_time();
						}
					}else {
						p->next_retry_connection = next_keep_alive_time();
					}
					 **/

					p->is_in_fdset = 0;
					p = p->next;
					unlock(previous);
					previous = current;
				}
				unlock(previous);
			}
		}

	} else {
		perror("CONTROL COMMAND TREE Protocol - Unable to setup listen socket: ");
		pthread_cancel(queueMonitor);
		return NULL;
	}

	return NULL;
}

proto_def* control_command_tcp_tree_init(void* args) {
	peers = NULL;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&peers_lock, &attr);

	history = NULL;

	protoID = PROTO_CONTROL_TCP_TREE;

	control_args* cargs = (control_args*) args;
	controlDiscoveryProtoID = cargs->discov_id; //PROTO_CONTROL_DISCOVERY
	mode = cargs->mode;

	while(my_ip_addr == NULL || my_ip_addr[0] == '\0')
		my_ip_addr = (char*) getChannelIpAddress();

	my_hostname = (char*) getHostname();


	if( pipe(local_pipe) != 0 ) {
		perror("CONTROL COMMAND TREE Protocol - Unable to setup internal pipe: ");
		return NULL;
	}

	proto_def* control = create_protocol_definition(protoID, "Control Tree", NULL, NULL);
	proto_def_add_consumed_event(control, controlDiscoveryProtoID, NEW_NEIGHBOR_IP_NOTIFICATION);
	proto_def_add_protocol_main_loop(control, &control_main_loop);

	return control;
}
