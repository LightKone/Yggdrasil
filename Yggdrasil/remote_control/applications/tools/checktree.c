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
			if( executeCommand(IS_UP, sock) > 0)  {
				//Success
				char* reply = getResponse(sock);
				if(reply != NULL) {
					printf("%s", reply);
					free(reply);
					close(sock);
					return 0;
				} else {
					printf("Failed to receive answer\n");
					close(sock);
					return 3;
				}
			} else {
				printf("Failed to send request\n");
				close(sock);
				return 2;
			}
		} else {
			printf("Unable to connect to server.\n");
			return 1;
		}

	return 0;
}
