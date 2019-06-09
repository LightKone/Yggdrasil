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

#include "data_struct.h"

Phy* physicalDevices = NULL;
Interface* interfaces = NULL;

extern int lk_error_code;

/*********************************************************
 * NetworkConfig
 *********************************************************/

NetworkConfig* defineWirelessNetworkConfig(char* type, int freq, int nscan, short mandatory, char* name, const struct sock_filter* filter){
    NetworkConfig* ntconf = (NetworkConfig*) malloc(sizeof(struct __NetworkConfig));

    ntconf->type = MAC;

    ntconf->config.macntconf.type = nameToType(type);
    ntconf->config.macntconf.freq = freq;
    ntconf->config.macntconf.nscan = nscan;
    ntconf->config.macntconf.mandatoryName = mandatory;
    ntconf->config.macntconf.name = name;
    ntconf->config.macntconf.filter = (struct sock_filter*) filter;

    return ntconf;
}

/*********************************************************
 *  Phy
 *********************************************************/

Phy* alloc_phy() {
	Phy* var = (Phy*) malloc(sizeof(struct _phy));
	memset(var->modes, 0, sizeof(short) * NUM_NL80211_IFTYPES);
	var->next = NULL;
	return var;
}

Phy* free_phy(Phy* var) {
	Phy* tmp = var->next;
	free(var);
	return tmp;
}


int phy_count(Phy* var) {
	if(var == NULL) return 0;
	return 1 + phy_count(var->next);
}

void free_all_phy(Phy* var) {
	if(var->next != NULL)
		free_all_phy(var->next);
	var->next = NULL;
	free_phy(var);
}

Phy* clone_phy(Phy* var) {
	Phy* p = (Phy*) malloc(sizeof(struct _phy));
	p->id = var->id;
	memcpy(p->modes, var->modes, sizeof(short) * NUM_NL80211_IFTYPES);
	p->next = NULL;
	return p;
}

/*********************************************************
 *  Interface
 *********************************************************/

Interface* alloc_interface() {
	Interface* var = (Interface*) malloc(sizeof(struct _interface));
	var->name = NULL;
	var->next = NULL;
	return var;
}

Interface* free_interface(Interface* var) {
	Interface* tmp = var->next;
	if(var->name != NULL)
		free(var->name);
	free(var);
	return tmp;
}

Interface* alloc_named_interface(char* name) {
	Interface* var = (Interface*) malloc(sizeof(struct _interface));
	int len = strlen(name)+1;
	var->name = (char*) malloc(len);
	memset(var->name, 0, len);
	strcpy(var->name, name);
	var->next = NULL;
	return var;
}

void free_all_interfaces(Interface* var) {
	if(var->next != NULL)
		free_all_interfaces(var->next);
	var->next = NULL;
	free_interface(var);
}

/*********************************************************
 *  Network
 *********************************************************/

Network* alloc_network(char* netName) {
	Network* n = (Network*) malloc(sizeof(struct _Network));
	n->ssid = netName;
	n->mesh_info = NULL;
	n->freq = 0;
	n->connected = 0;
	n->next = NULL;
	return n;
}

Network* free_network(Network* net) {
	Network* next = net->next;
	free(net->ssid);
	if(net->mesh_info != NULL) free(net->mesh_info);
	free(net);
	return next;
}

void fillMeshInformation(Mesh* mesh_info, unsigned char *ie, int ielen) {
	uint8_t *data;

	while (ielen >= 2 && ielen >= ie[1]) {
		if (ie[0] == 113 && ie[1] > 0 && ie[1] <= 32) {
			data = ie + 2;
			mesh_info->pathSelection = data[0];
			mesh_info->pathSelectionMetric = data[1];
			mesh_info->syncMethod = data[3];
			mesh_info->authProtocol = data[4];
			mesh_info->congestionControl = data[10];
			return;
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
}

/*********************************************************
 * YggPhyMessage
 *********************************************************/
int initYggPhyMessage(YggPhyMessage *msg) {
	msg->phyHeader.type = IP_TYPE;
	char id[] = AF_YGG_ARRAY;
	memcpy(msg->yggHeader.data, id, YGG_HEADER_LEN);

	bzero(msg->data, MAX_PAYLOAD);
	return SUCCESS;
}

int initYggPhyMessageWithPayload(YggPhyMessage *msg, char* buffer, short bufferlen) {

	int len = bufferlen;
	if(len > MAX_PAYLOAD)
		return FAILED;

	msg->phyHeader.type = IP_TYPE;
	char id[] = AF_YGG_ARRAY;
	memcpy(msg->yggHeader.data, id, YGG_HEADER_LEN);

	bzero(msg->data, MAX_PAYLOAD);

	msg->dataLen = len;
	memcpy(msg->data, buffer, len);

	return SUCCESS;
}

int addPayload(YggPhyMessage *msg, char* buffer) {
	int len = strlen(buffer);
	if(len > MAX_PAYLOAD)
		return -1;

	msg->dataLen = len+1;
	memcpy(msg->data, buffer, len+1);

	return len;
}

//Deserialize an Yggdrasil message from buffer to an existing structure
int deserializeYggPhyMessage(YggPhyMessage *msg, unsigned short msglen, void* buffer, int bufferLen) {
	int checkType = isYggMessage(msg, msglen);
	if(checkType != 0) {
		memcpy(buffer, msg->data, (bufferLen < msg->dataLen ? bufferLen : msg->dataLen));
	}
	return checkType;
}


/*********************************************************
 *  Channel
 *********************************************************/

int getInterfaceID(Channel* ch, char* interface){
	struct ifreq ifr;
#ifdef DEBUG
	fprintf(stderr, "Interface: %s\n", interface);
#endif
	strcpy(ifr.ifr_name, interface);
	if (ioctl(ch->sockid, SIOGIFINDEX, &ifr) < 0) {
		lk_error_code = NO_IF_INDEX_ERR;
		return FAILED;
	}
	ch->ifindex=ifr.ifr_ifindex;
	return SUCCESS;
}

int getInterfaceMACAddress(Channel* ch, char* interface){
	struct ifreq ifr;
	strcpy(ifr.ifr_name, interface);
	if (ioctl(ch->sockid, SIOCGIFHWADDR, &ifr) == -1) {
		lk_error_code =  NO_IF_ADDR_ERR;
		return FAILED;
	}
	memcpy(&(ch->hwaddr.data), &(ifr.ifr_hwaddr.sa_data), WLAN_ADDR_LEN);
	return SUCCESS;
}

int getInterfaceMTU(Channel* ch, char* interface) {
	struct ifreq ifr;
	strcpy(ifr.ifr_name, interface);
	if (ioctl(ch->sockid, SIOCGIFMTU, &ifr) == -1) {
		lk_error_code = NO_IF_MTU_ERR;
		return FAILED;
	}
	return SUCCESS;
}

int createChannel(Channel* ch, char* interface) {
	ch->sockid = socket(AF_PACKET, SOCK_RAW, 0);
	if(ch->sockid < 0) {
		return ch->sockid;
	}

	int ret = getInterfaceID(ch, interface);
	if(ret != SUCCESS) return ret;
#ifdef DEBUG
	printf("Interface id is %d\n",ch->ifindex);
#endif

	ret = getInterfaceMACAddress(ch, interface);
	if(ret != SUCCESS) return ret;
#ifdef DEBUG
	char* addr = (char*) malloc(32);
	printf("Interface MAC address is %s\n",wlan2asc(&(ch->hwaddr),addr));
	free(addr);
#endif

	ret = getInterfaceMTU(ch, interface);
	if(ret != SUCCESS) return 0;
#ifdef DEBUG
	printf("Interface MTU is %d\n",ch->mtu);
#endif

	bzero(ch->ip.addr, INET_ADDRSTRLEN);

	return 0;
}

int bindChannel(Channel* ch){
	struct sockaddr_ll sll;
	memset(&sll, 0, sizeof(sll));
	sll.sll_family=AF_PACKET;
	sll.sll_ifindex=ch->ifindex;
	sll.sll_protocol=htons(ETH_P_ALL);
	if (bind(ch->sockid, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
		return SOCK_BIND_ERR;
	}
	return 0;
}
