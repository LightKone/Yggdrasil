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

#ifndef CONTROL_PROTOCOL_UTILS_H_
#define CONTROL_PROTOCOL_UTILS_H_

#include <stdlib.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "core/utils/utils.h"

typedef enum CONTROL_COMMAND_TREE_REQUESTS_ {
	SETUP,
	IS_UP,
	TEAR_DOWN,
	DO_COMMAND,
	GET_FILE,
	RECONFIGURE,
	MOVE_FILE,
	KILL,
	RUN,
	REMOTE_CHANGE_LINK,
	REMOTE_CHANGE_VAL,
	LOCAL_PRUNE,
	LOCAL_ATTACH,
	LOCAL_ANNOUNCE,
	LOCAL_IS_UP_REPLY,
	REBOOT,
	LOCAL_ENABLE_DISC,
	LOCAL_DISABLE_DISC,
	SHUTDOWN,
	GET_NEIGHBORS,
	LOCAL_GET_NEIGHBORS_REPLY,
	DEBUG_NEIGHBOR_TABLE,
	LOCAL_RUN_REPLY,
	LOCAL_KILL_REPLY,
	LOCAL_CHANGE_VAL_REPLY,
	LOCAL_CHANGE_LINK_REPLY,
	LOCAL_REBOOT_REPLY,
	LOCAL_SHUTDOWN_REPLY,
	LOCAL_ENABLE_DISC_REPLY,
	LOCAL_DISABLE_DISC_REPLY,
	UNDEFINED,
	NO_OP,
} CONTROL_COMMAND_TREE_REQUESTS;

typedef struct tree_command_header_ {
	uuid_t id;
	short command_code;
	unsigned short command_size;
	void* command;
} tree_command;

tree_command* init_command_header(CONTROL_COMMAND_TREE_REQUESTS cmd);

void add_command_body(tree_command* cmd, void* buffer, unsigned short buffer_len);

int write_command_header(int socket, tree_command* cmd);

int write_command_annouce(int socket, tree_command* cmd);

int write_command_body(int socket, tree_command* cmd);

tree_command* read_command_header(int socket);

int read_command_body(int socket, tree_command* cmd);

void destroy_command_header(tree_command* cmd);

int readfully(int fd, void* buf, int len);
int writefully(int fd, void* buf, int len);

#endif /* CONTROL_PROTOCOL_UTILS_H_ */
