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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "remote_control/utils/control_protocol_utils.h"
#include "remote_control/utils/cmdio.h"

#define BUFSIZE 50

char* read_line(void)
{
	int bufsize = BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;

	if (!buffer) {
		fprintf(stderr, "allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		// Read a character
		c = getchar();

		// If we hit EOF, replace it with a null character and return.
		if (c == EOF || c == '\n') {
			buffer[position] = '\0';
			return buffer;
		} else {
			buffer[position] = c;
		}
		position++;

		// If we have exceeded the buffer, reallocate.
		if (position >= bufsize) {
			bufsize += BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				fprintf(stderr, "allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

int executeCommandSetup(int sock) {
	int command = SETUP;
	return writefully(sock, &command, sizeof(int));
}

int executeCommandCheck(int sock) {
	int command = IS_UP;
	return writefully(sock, &command, sizeof(int));
}

int verifyCommandKill(char* order) {
	char l[100];
	bzero(l, 100);
	memcpy(l, order, strlen(order));
	char* l2 = l;
	char* tostore = strsep(&l2, " ");
	if(tostore != NULL && strlen(tostore) > 0){
		char* pi;
		while((pi = strsep(&l2, " ")) != NULL){

			int pi_number = atoi(pi);
			if(pi_number <= 0  && pi_number > 24)
				return 0;
		}
		return 1;
	}
	return 0;
}

void executeCommandKill(int sock) {
	int command = KILL;
	printf("prompt path to store log file and pi numbers to kill (none if all):\n");
	char* kill_order = read_line();
	if(verifyCommandKill(kill_order) == 1) {
		writefully(sock, &command, sizeof(int));
		int kill_order_size = strlen(kill_order) + 1;
		printf("%s\n", kill_order);
		writefully(sock, &kill_order_size, sizeof(int));
		writefully(sock, kill_order, kill_order_size);
	} else {
		printf("Invalid format received\n");
	}
}

int verifyCommandRun(char* order) {
	char l[50];
	bzero(l, 50);
	memcpy(l, order, strlen(order));
	char* l2 = l;
	char* toexec = strsep(&l2, " ");
	if(toexec != NULL && strlen(toexec) > 0 )
		return 1;
	return 0;
}

void executeCommandRun(int sock) {
	int command = RUN;
	printf("prompt executable path and args:\n");
	char* exec = read_line();
	if(verifyCommandRun(exec) == 1) {
		writefully(sock, &command, sizeof(int));
		int exec_size = strlen(exec) + 1;
		printf("%s\n", exec);
		writefully(sock, &exec_size, sizeof(int));
		writefully(sock, exec, exec_size);
	} else {
		printf("No exec path\n");
	}
}

int verifyCommandChangeLink(char* order) {
	char l[50];
	bzero(l, 50);
	memcpy(l, order, strlen(order));
	char* l2 = l;
	int p1 = atoi(strsep(&l2, " "));
	int p2 = atoi(strsep(&l2, " "));
	char* eos = strsep(&l2, " ");

	if(p1 > 0 && p1 <= 24 && p2 > 0 && p2 <= 24 && eos == NULL)
		return 1;
	return 0;
}

void executeCommandChangeLink(int sock) {
	int command = REMOTE_CHANGE_LINK;
	printf("prompt link to change (pi1 pi2) :\n");
	char* change_order = read_line();
	if(verifyCommandChangeLink(change_order) == 1) {
		writefully(sock, &command, sizeof(int));
		int change_order_size = strlen(change_order) + 1;
		printf("%s %d\n", change_order, change_order_size);
		writefully(sock, &change_order_size, sizeof(int));
		writefully(sock, change_order, change_order_size);
	}else
		printf("wrong format should be: pi_number pi_number");
}

int verifyCommandChangeVal(char* order) {
	char l[50];
	bzero(l, 50);
	memcpy(l, order, strlen(order));
	char* l2 = l;
	int p1 = atoi(strsep(&l2, " "));
	int newval = atoi(strsep(&l2, " "));
	char* eos = strsep(&l2, " ");

	if(p1 > 0 && p1 <= 24 && eos == NULL)
		return 1;
	return 0;
}

void executeCommandChangeVal(int sock) {
	int command = REMOTE_CHANGE_VAL;
	printf("prompt pi and value to change:\n");
	char* change_order = read_line();

	if(verifyCommandChangeVal(change_order) == 1){
		writefully(sock, &command, sizeof(int));
		int change_order_size = strlen(change_order) + 1;
		printf("%s %d\n", change_order, change_order_size);
		writefully(sock, &change_order_size, sizeof(int));
		writefully(sock, change_order, change_order_size);
	}else
		printf("wrong format should be: pi_number new val");
}

void executeCommandReboot(int sock) {
	int command = REBOOT;
	writefully(sock, &command, sizeof(int));
}

int prompt(int sock) {
	printf("Available operations:\n");
	printf("0: initialize control topology\n");
	printf("1: check\n");
	printf("2: destroy control topology\n");
	printf("3: execute command\n");
	printf("4: gather file\n");
	printf("5: reconfigure\n");
	printf("6: move file\n");
	printf("7: kill process\n");
	printf("8: run experience\n");
	printf("9: change link\n");
	printf("10: change value\n");
	printf("11: reboot system\n");
	printf("12: Enable discovery service\n");
	printf("13: Disable discovery service\n");
	printf("14: Shutdown\n");
	printf("15: Get Neighbors\n");
	printf("16: exit\n");

	char* command = read_line();

	int code = atoi(command);
	free(command);

	char* reply = NULL;

	switch(code) {
	case 0:
		if(executeCommandSetup(sock) >= 0) {
			reply = getResponse(sock);
		}
		break;
	case 1:
		if(executeCommandCheck(sock) >= 0) {
			reply = getResponse(sock);
		}
		break;
	case 7: //kill tostore pis
		executeCommandKill(sock);
		reply = getResponse(sock);
		break;
	case 8: //run path/to/exec args
		executeCommandRun(sock);
		reply = getResponse(sock);
		break;
	case 9: //change link pi1 pi2
		executeCommandChangeLink(sock);
		reply = getResponse(sock);
		break;
	case 10: //change val pi newval
		executeCommandChangeVal(sock);
		reply = getResponse(sock);
		break;
	case 11:
		executeCommandReboot(sock);
		reply = getResponse(sock);
		break;
	case 12:
		executeCommand(LOCAL_ENABLE_DISC, sock);
		reply = getResponse(sock);
		break;
	case 13:
		executeCommand(LOCAL_DISABLE_DISC, sock);
		reply = getResponse(sock);
		break;
	case 14:
		executeCommand(SHUTDOWN, sock);
		reply = getResponse(sock);
		break;
	case 15:
		executeCommand(GET_NEIGHBORS, sock);
		reply = getResponse(sock);
		break;
	case 16:
		return 1;
		break;
	}

	if(reply != NULL) {
		printf("Response: %s\n", reply);
		free(reply);
		return 0;
	} else {
		printf("An error has occurred\n");
		return 1;
	}

	return 0;
}

int main(int argc, char* argv[]) {

	if(argc != 3) {
		printf("Usage: %s IP PORT\n", argv[0]);
		return 1;
	}

	int sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	bzero(&address, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	inet_aton(argv[1], &address.sin_addr);
	address.sin_port = htons( atoi(argv[2]) );
	int success = connect(sock, (const struct sockaddr*) &address, sizeof(address));

	if(success == 0) {

		while(prompt(sock) == 0);

		close(sock);
	} else {
		printf("Unable to connect to server.\n");
		return 1;
	}

	return 0;
}
