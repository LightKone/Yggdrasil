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

#include "hw.h"


/*********************************************************
 * Auxiliary global variables
 *********************************************************/
extern Phy* physicalDevices;
extern Interface* interfaces;

/*********************************************************
 * Interface related
 *********************************************************/

int removeInterface(Interface* itf) {
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	int ret = 0;
	struct nl_msg *msg  = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_DEL_INTERFACE, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, itf->id);
	ret = nl_send_auto(socket, msg);  // Send the message
	if(ret < 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_DELETE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for interface removal sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer.
	if(ret != 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_DELETE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Successfully deleted interface with id: %d\n",itf->id);
#endif
	nlmsg_free(msg);
	nl_socket_free(socket);
	return ret;
}

int createInterface(Phy* phy, Interface* itf) {

	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.
	int ret = 0;
	struct nl_msg *msg  = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_NEW_INTERFACE, 0);
	nla_put_u32(msg, NL80211_ATTR_WIPHY, phy->id);
	nla_put(msg, NL80211_ATTR_IFNAME, strlen(itf->name)+1, itf->name);
	nla_put_u32(msg, NL80211_ATTR_IFTYPE, itf->type);
	ret = nl_send_auto(socket, msg);  // Send the message.
	if(ret < 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_CREATE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for interface creation sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer.
	if(ret != 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_CREATE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Successfully deleted interface with id: %d\n",itf->id);
#endif

	nlmsg_free(msg);
	nl_socket_free(socket);
	return ret;
}


/**
 * Support function for externally accessible method: int listInterfaces(Interface** interfaces, Phy* physicalDev)
 */
static int interface_callback_dump(struct nl_msg *msg, void *arg) {
	struct nlattr* tb_msg[NL80211_ATTR_MAX+1];
	struct genlmsghdr* gnhl = nlmsg_data(nlmsg_hdr(msg));

	Interface* this = alloc_interface();

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnhl, 0), genlmsg_attrlen(gnhl, 0), NULL);

	if(tb_msg[NL80211_ATTR_IFINDEX] && tb_msg[NL80211_ATTR_IFNAME]) {
		this->id = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
		char* name = nla_get_string(tb_msg[NL80211_ATTR_IFNAME]);
		int namelen = strlen(name);
		this->name = (char*) malloc(namelen + 1);
		memset(this->name, 0, namelen + 1);
		strcpy(this->name, name);
	} else {
#ifdef DEBUG
		printf("Incomplete information in interface record received: IFINDEX -> %s, IFNAME -> %s",(tb_msg[NL80211_ATTR_IFINDEX] ? "OK":"NOT OK"), (tb_msg[NL80211_ATTR_IFNAME] ? "OK":"NOT OK"));
#endif
		free_interface(this);
		return 1;
	}

	if(tb_msg[NL80211_ATTR_IFTYPE]) {
		this->type = nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE]);
	}

	this->next = interfaces;
	interfaces = this;
	return 0;
}

int listInterfaces(Interface** itfs, Phy* physicalDev) {

	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	nl_socket_disable_seq_check(socket);
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, interface_callback_dump, NULL);
	int ret = 0;

	interfaces = NULL;

	struct nl_msg* msg = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0);
	nla_put_u32(msg, NL80211_ATTR_WIPHY, physicalDev->id);
	ret = nl_send_auto(socket, msg);  // Send the message
	if(ret < 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_LIST_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for interface list sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer.

	if(ret != 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_LIST_ERROR;
	}

	*itfs = interfaces;
	interfaces = NULL;

	nlmsg_free(msg);
	nl_socket_free(socket);
	return ret;
}


int set_ip_addr(Channel* ch)
{
	struct ifreq ifr;
	ifr.ifr_ifindex = ch->ifindex;

	if(ioctl(ch->sockid, SIOCGIFNAME, &ifr) == 0 && ioctl(ch->sockid, SIOCGIFADDR, &ifr) == 0){


		if (inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, ch->ip_addr, INET_ADDRSTRLEN) != NULL) {
			return SUCCESS;
		}

	}

	bzero(ch->ip_addr, INET_ADDRSTRLEN);
	return FAILED;

}


int set_if_flags(Channel* ch, char *interface, short flags)
{
	struct ifreq ifr;

	ifr.ifr_flags = flags;
	strcpy(ifr.ifr_name, interface);

	return ioctl(ch->sockid, SIOCSIFFLAGS, &ifr);

}

int setInterfaceUP(Channel* ch, char* interface)
{
	return set_if_flags(ch, interface, IFF_UP);
}

int setInterfaceDOWN(Channel* ch, char* interface)
{
	return set_if_flags(ch, interface, ~IFF_UP);
}

int checkInterfaceUP(Channel* ch, char* interface){
	struct ifreq if_req;
	strcpy(if_req.ifr_name, interface);
	int rv = ioctl(ch->sockid, SIOCGIFFLAGS, &if_req);

	if ( rv == -1) return -1;

	return (if_req.ifr_flags & IFF_UP);
}

int checkInterfaceConnected(Channel* ch, char* interface){
	struct ifreq if_req;
	strcpy(if_req.ifr_name, interface);
	int rv = ioctl(ch->sockid, SIOCGIFFLAGS, &if_req);

	if ( rv == -1) return -1;

	return (if_req.ifr_flags & IFF_RUNNING);
}

int setInterfaceType(int id, int type){
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.
	int ret = 0;
	struct nl_msg *msg  = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_SET_INTERFACE, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, id);
	nla_put_u32(msg, NL80211_ATTR_IFTYPE, type);
	ret = nl_send_auto(socket, msg);  // Send the message.
	if(ret < 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_CREATE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for interface creation sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer.
	if(ret != 0) {
		nl_error = ret;
		return NETCTL_INTERFACE_CREATE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Successfully deleted interface with id: %d\n",id);
#endif

	nlmsg_free(msg);
	nl_socket_free(socket);
	return ret;
}
/*********************************************************
 * Physical device related
 *********************************************************/


/**
 * Support function for externally accessible method: int listDevices(Phy** phy, int target_mode)
 */
static int physical_device_callback_dump(struct nl_msg *msg, void *arg) {
	Phy* this = alloc_phy();

	struct nlattr* tb_msg[NL80211_ATTR_MAX+1];
	struct genlmsghdr* gnhl = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr* mode;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnhl, 0), genlmsg_attrlen(gnhl, 0), NULL);
	if ( tb_msg[NL80211_ATTR_WIPHY] ) {
		this->id = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
	} else {
		free_phy(this);
		return 1;
	}
	if ( tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES] ) {
		int remaining_bytes;
		nla_for_each_nested(mode, tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES], remaining_bytes) {
			this->modes[nla_type(mode)] = 1;
		}
	} else {
		free_phy(this);
		return 1;
	}

	this->next = physicalDevices;
	physicalDevices = this;
	return 0;
}

int listDevices(Phy** phy, int target_mode) {

	// Open socket to kernel.
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	nl_socket_disable_seq_check(socket);

	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.
	int ret = 0;
	struct nl_msg* msg = nlmsg_alloc();

	physicalDevices = NULL;

	nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, physical_device_callback_dump, NULL);  // Add the callback.

	genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_WIPHY, 0);
	ret = nl_send_auto(socket, msg);  // Send the message

#ifdef DEBUG
	printf("NL80211_CMD_GET_WIPHY sent %d bytes to the kernel.\n", ret);
#endif

	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer.
	nlmsg_free(msg);
	if(ret != 0) {
		nl_error = ret;
		return NETCTL_PHYLST_ERROR;
	}
#ifdef DEBUG
	else {
		printf("Successfully listed wireless devices. Found %d devices\n\n", phy_count(physicalDevices));
	}
#endif

	Phy** current_addr = &physicalDevices;
	Phy* current = physicalDevices;
	while(current != NULL) {
		if(current->modes[target_mode] == 1) { //This device is compatible
#ifdef DEBUG
			printf("device %d -> OK\n", current->id);
#endif
			current_addr = &(current->next);
			current = current->next;
		} else {
#ifdef DEBUG
			printf("device %d -> NOT_OK\n", current->id);
#endif
			*current_addr = current->next;
			free_phy(current);
			current = *current_addr;
		}
	}

	*phy = physicalDevices;
	physicalDevices = NULL;

#ifdef DEBUG
	int compatible_devices = phy_count(physicalDevices);
	printf("Found %d compatible interfaces\n", compatible_devices);
#endif

	nl_socket_free(socket);
	return 0;
}

Phy* filterDevicesByModesSupported(Phy* devices) {
	Phy* candidateSet = NULL;
	Phy* temp = devices;
	int minimumModesSupported = 1000;
	while(temp != NULL) {
		int localCount = 0;
		int i;
		for(i = 0; i < IFTYPE_MAX; i++) {
			if(temp->modes[i] != 0)
				localCount++;
		}
		if(localCount < minimumModesSupported) {
			if(candidateSet != NULL) {
				free_all_phy(candidateSet);
			}
			candidateSet = clone_phy(temp);
			candidateSet->next = NULL;
			minimumModesSupported = localCount;
		} else if(localCount == minimumModesSupported) {
			Phy* p = clone_phy(temp);
			p->next = candidateSet;
			candidateSet = p;
		}
		temp = temp->next;
	}
	return candidateSet;
}

int defineLKMFilter(Channel* ch) {
	struct sock_filter filter[] = {
			{ 0x30, 0, 0, 0x0000000e },
			{ 0x15, 0, 5, 0x0000004c },
			{ 0x30, 0, 0, 0x0000000f },
			{ 0x15, 0, 3, 0x0000004b },
			{ 0x30, 0, 0, 0x00000010 },
			{ 0x15, 0, 1, 0x00000050 },
			{ 0x6, 0, 0, 0x00040000 },
			{ 0x6, 0, 0, 0x00000000 },
	};
	struct sock_fprog bpf = {
			.len = 8,
			.filter = filter,
	};
	int r = setsockopt(ch->sockid, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf));
	if(r != 0)
		return CHANNEL_FILTER_ERROR;
	return 0;
}

int defineFilter(Channel* ch, struct sock_filter* filter) {
	struct sock_fprog bpf = {
			.len = 8,
			.filter = filter,
	};
	int r = setsockopt(ch->sockid, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf));
	if(r != 0)
		return CHANNEL_FILTER_ERROR;
	return 0;
}

/**************************************************
 * System info
 *************************************************/

double get_cpu_temp(){

	double systemp;
	FILE *thermal;
	int n;

	thermal = fopen("/sys/class/thermal/thermal_zone0/temp","r");
	n = fscanf(thermal,"%f",&systemp);
	fclose(thermal);
	//systemp = millideg / 1000;

	return systemp;

}


