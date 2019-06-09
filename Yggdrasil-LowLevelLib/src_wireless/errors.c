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

#include "errors.h"

void printError(int errorCode) {
	char err[200];
	bzero(err, 200);
	switch(errorCode) {
	case NO_IF_INDEX_ERR:
		sprintf(err, "Could not extract the id for the wireless interface: %s",strerror(errno));
		logError(err);
		break;
	case NO_IF_ADDR_ERR:
		sprintf(err, "Could not extract the physical address of the interface: %s",strerror(errno));
		logError(err);
		break;
	case NO_IF_MTU_ERR:
		sprintf(err, "Could not extract the MTU of the interface: %s",strerror(errno));
		logError(err);
		break;
	case SOCK_BIND_ERR:
		sprintf(err, "Could not bind the socket adequately: %s",strerror(errno));
		logError(err);
		break;
	case CHANNEL_SENT_ERROR:
		sprintf(err, "Error transmitting message: %s",strerror(errno));
		logError(err);
		break;
	case CHANNEL_RECV_ERROR:
		sprintf(err, "Error receiving message: %s",strerror(errno));
		logError(err);
		break;
	default:
		sprintf(err, "An error has occurred within the COMM layer: %s",strerror(errno));
		logError(err);
		break;
	}
}

void printNetlinkError(int errorCode, int netlinkErrorCode) {
	char err[200];
	bzero(err, 200);
	printf("errono: %d errorCode: %d printing netlink error: %d %d\n", errno, errorCode, netlinkErrorCode, -netlinkErrorCode);
	perror("ERROR: ");
	switch(errorCode) {
	case NETCTL_PHYLST_ERROR:
		sprintf(err, "Error accessing the list of physical interfaces: %s",nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_INTERFACE_LIST_ERROR:
		sprintf(err, "Error accessing the list of interfaces: %s",nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_INTERFACE_CREATE_ERROR:
		sprintf(err, "Error creating an interface: %s",nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_INTERFACE_DELETE_ERROR:
		sprintf(err, "Error deleting an interfaces: %s",nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_MESH_JOIN_ERROR:
		sprintf(err, "Error joining a mesh network: %s", nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_ADHOC_JOIN_ERROR:
		sprintf(err, "Error joining an adhoc network: %s", nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_ADHOC_LEAVE_ERROR:
		sprintf(err, "Error leaving an adhoc network: %s", nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	case NETCTL_MESH_LEAVE_ERROR:
		sprintf(err, "Error leaving a mesh network: %s", nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	default:
		sprintf(err, "An error has occurred within the MONITOR/NET_CTRL layer: %s",nl_geterror(-netlinkErrorCode));
		logError(err);
		break;
	}
}

void logError(char* error) {

	char buffer[26];
	struct tm* tm_info;

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	tm_info = localtime(&tp.tv_sec);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);

	fprintf(stderr, "%s %ld :: %s\n", buffer, tp.tv_nsec, error);
}
