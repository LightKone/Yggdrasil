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

#include "control_protocol_utils.h"

static CONTROL_COMMAND_TREE_REQUESTS announce_code = LOCAL_ANNOUNCE;

tree_command* init_command_header(CONTROL_COMMAND_TREE_REQUESTS cmd) {
	tree_command* cmd_str = malloc(sizeof(tree_command));
	genUUID(cmd_str->id);
	cmd_str->command_code = cmd;
	cmd_str->command_size = 0;
	cmd_str->command = NULL;
	return cmd_str;
}

void add_command_body(tree_command* cmd, void* buffer, unsigned short buffer_len) {
	if(buffer_len > 0) {
		cmd->command = malloc(buffer_len);
		memcpy(cmd->command, buffer, buffer_len);
		cmd->command_size = buffer_len;
	}
}

int write_command_header(int socket, tree_command* cmd) {
	if( writefully(socket, cmd->id, sizeof(uuid_t)) > 0 &&
			writefully(socket, &(cmd->command_code), sizeof(CONTROL_COMMAND_TREE_REQUESTS)) > 0) {
		return 1;
	} else {
		return 0;
	}
}

int write_command_annouce(int socket, tree_command* cmd) {
	if ( writefully(socket, cmd->id, sizeof(uuid_t)) > 0 &&
			writefully(socket, &announce_code, sizeof(CONTROL_COMMAND_TREE_REQUESTS)) > 0) {
		return 1;
	} else {
		return 0;
	}
}

int write_command_body(int socket, tree_command* cmd){
	if(cmd->command_size > 0) {
		if ( writefully(socket, &(cmd->command_size), sizeof(unsigned short)) > 0 &&
				writefully(socket, cmd->command, cmd->command_size) > 0 ) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}

tree_command* read_command_header(int socket) {
	tree_command* cmd = malloc(sizeof(tree_command));
	int r = readfully(socket, cmd->id, sizeof(uuid_t));
	if(r > 0)
		r = readfully(socket, &(cmd->command_code), sizeof(CONTROL_COMMAND_TREE_REQUESTS));
	if(r > 0) {
		cmd->command = NULL;
		cmd->command_size = 0;
		return cmd;
	} else {
		free(cmd);
		return NULL;
	}
}

int read_command_body(int socket, tree_command* cmd) {
	int r = readfully(socket, &(cmd->command_size), sizeof(unsigned short));

	if(r > 0 && cmd->command_size > 0) {
		cmd->command = malloc(cmd->command_size);
	}else
		return -1;

	r = readfully(socket, cmd->command, cmd->command_size);

	if(r <= 0) {
		free(cmd->command);
		return -1;
	}
	return cmd->command_size;
}


void destroy_command_header(tree_command* cmd) {
	if(cmd->command_size > 0 && cmd->command != NULL){
		free(cmd->command);
		cmd->command = NULL;
	}
	free(cmd);
}

int readfully(int fd, void* buf, int len) {
	int missing = len;
	while(missing > 0) {
		int r = read(fd, buf + len - missing, missing);
		if(r <= 0)
			return r;
		missing-=r;
	}
	return len-missing;
}

int writefully(int fd, void* buf, int len) {
	int missing = len;
	while(missing > 0) {
		int w = write(fd, buf + len - missing, missing);
		if(w <= 0)
			return w;
		missing-=w;
	}
	return len-missing;
}
