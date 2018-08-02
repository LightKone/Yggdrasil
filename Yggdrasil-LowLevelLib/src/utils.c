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

#include "utils.h"

/*********************************************************
 * Utilities
 *********************************************************/

// Translate a name to an if_type
int nameToType(char* name) {

	if(strcasecmp(name, "AdHoc") == 0) return IFTYPE_ADHOC;
	if(strcasecmp(name, "Mesh") == 0) return IFTYPE_MESH;
//	if(strcasecmp(name, "Monitor")) return IFTYPE_MONITOR;

	return -1;
}

// Return the address in a human readable form
char * wlan2asc(WLANAddr* addr, char str[]) {
	sprintf(str, "%x:%x:%x:%x:%x:%x",
			addr->data[0],addr->data[1],addr->data[2],
			addr->data[3],addr->data[4],addr->data[5]);
	return str;
}

// Convert a char to a hex digit
int hexdigit(char a) {
	if (a >= '0' && a <= '9') return(a-'0');
	if (a >= 'a' && a <= 'f') return(a-'a'+10);
	if (a >= 'A' && a <= 'F') return(a-'A'+10);
	return -1;
}

// convert an address string to a series of hex digits
int sscanf6(char str[], int *a1, int *a2, int *a3, int *a4, int *a5, int *a6){
	int n;
	*a1 = *a2 = *a3 = *a4 = *a5 = *a6 = 0;
	while ((n=hexdigit(*str))>=0)
		(*a1 = 16*(*a1) + n, str++);
	if (*str++ != ':') return 1;
	while ((n=hexdigit(*str))>=0)
		(*a2 = 16*(*a2) + n, str++);
	if (*str++ != ':') return 2;
	while ((n=hexdigit(*str))>=0)
		(*a3 = 16*(*a3) + n, str++);
	if (*str++ != ':') return 3;
	while ((n=hexdigit(*str))>=0)
		(*a4 = 16*(*a4) + n, str++);
	if (*str++ != ':') return 4;
	while ((n=hexdigit(*str))>=0)
		(*a5 = 16*(*a5) + n, str++);
	if (*str++ != ':') return 5;
	while ((n=hexdigit(*str))>=0)
		(*a6 = 16*(*a6) + n, str++);
	return 6;
}

// Define the address from a human readable form
int str2wlan(char machine[], char human[]) {
	int a[6], i;
	// parse the address
	if (sscanf6(human, a, a+1, a+2, a+3, a+4, a+5) < 6) {
		return -1;
	}
	// make sure the value of every component does not exceed on byte
	for (i=0; i < 6; i++) {
		if (a[i] > 0xff) return -1;
	}
	// assign the result to the member "data"
	for (i=0; i < 6; i++) {
		machine[i]=a[i];
	}
	return 0;
}


void mac_addr_n2a(char *mac_addr, unsigned char *arg) {
	// From http://git.kernel.org/cgit/linux/kernel/git/jberg/iw.git/tree/util.c.
	int i, l;

	l = 0;
	for (i = 0; i < 6; i++) {
		if (i == 0) {
			sprintf(mac_addr+l, "%02x", arg[i]);
			l += 2;
		} else {
			sprintf(mac_addr+l, ":%02x", arg[i]);
			l += 3;
		}
	}
}

/*************************************************
 * Auxiliary Functions
 *************************************************/

void setToAddress(WLANAddr *daddr, int ifindex, struct sockaddr_ll *to) {
   to->sll_family = AF_PACKET;
   to->sll_ifindex = ifindex;
   memmove(&(to->sll_addr), daddr->data, WLAN_ADDR_LEN);
   to->sll_halen=6;
}

//Retrieves the SSID of a network from the given buffer
//To be used with callback dumps from libnl
char* getSSID(unsigned char *ie, int ielen) {
	uint8_t len;
	uint8_t *data;
	char* ssid;
	int i;

	while (ielen >= 2 && ielen >= ie[1]) {
		if ((ie[0] == 0 || ie[0] == 114) && ie[1] > 0 && ie[1] <= 32) {
			len = ie[1];
			data = ie + 2;
			ssid = (char*) malloc(len+1);
			memset(ssid, 0, len+1);
			char* p = ssid;
			for (i = 0; i < len; i++) {
				if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\') { sprintf(p, "%c", data[i]); p++;}
			}
			return ssid;
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
	return NULL;
}


/*************************************************
 * Verification Functions
 *************************************************/

// Checks if a message within a buffer belongs to Yggdrasil
int isYggMessage(void* buffer, int bufferLen) {
	if(bufferLen < WLAN_HEADER_LEN+YGG_HEADER_LEN)
		return 0;

	unsigned char* p = (buffer+WLAN_HEADER_LEN);
	unsigned char ygg[YGG_HEADER_LEN] = AF_YGG_ARRAY;
	if(memcmp(p,ygg,3)==0) {
		return 1;
	}

	return 0;
}

//Checks if the given buffer contains information about a Mesh Network
//To be used with callback dumps from libnl
char isMesh(unsigned char *ie, int ielen) {
	while (ielen >= 2 && ielen >= ie[1]) {
		if (ie[0] == 114 && ie[1] > 0 && ie[1] <= 32) {
			return 1;
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
	return 0;
}

int existsNetwork(char* netName) {
	Network* tmp = knownNetworks;
	while(tmp != NULL) {
		if(strcmp(netName, tmp->ssid) == 0) return 1;
		tmp = tmp->next;
	}
	return 0;
}
