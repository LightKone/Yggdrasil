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

#ifndef YGG_LL_ERRORS_H_
#define YGG_LL_ERRORS_H_

#include <netlink/errno.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <time.h>

int nl_error;

//TODO review: Error codes

/*********************************************************
 * Netlink related error codes
 *********************************************************/
#define NETCTL_PHYLST_ERROR 1
#define NETCTL_INTERFACE_LIST_ERROR 2
#define NETCTL_INTERFACE_CREATE_ERROR 3
#define NETCTL_INTERFACE_DELETE_ERROR 4
#define NETCTL_MESH_JOIN_ERROR 5
#define NETCTL_ADHOC_JOIN_ERROR 6
#define NETCTL_ADHOC_LEAVE_ERROR 7
#define NETCTL_MESH_LEAVE_ERROR 8


/*********************************************************
 * Error codes
 *********************************************************/
#define NO_IF_INDEX_ERR 1
#define NO_IF_ADDR_ERR 2
#define NO_IF_MTU_ERR 3

#define SOCK_BIND_ERR 4

#define CHANNEL_SENT_ERROR 5
#define CHANNEL_RECV_ERROR 6
#define CHANNEL_FILTER_ERROR 7

/**********************************************************
 * Print Error auxiliary functions
 **********************************************************/
void printError(int errorCode);
void printNetlinkError(int errorCode, int netlinkErrorCode);

void logError(char* error);

#endif /* YGG_LL_ERRORS_H_ */
