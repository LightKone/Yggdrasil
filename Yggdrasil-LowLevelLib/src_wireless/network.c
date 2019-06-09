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


#include "network.h"

/*********************************************************
 * Mesh Network related functions
 *********************************************************/

int leaveMesh(Interface* itf) {
	// Open socket to kernel.
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	struct nl_msg *msg = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_LEAVE_MESH, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, itf->id);
	int ret = nl_send_auto(socket, msg);  // Send the message.
	if(ret < 0) {
		nlmsg_free(msg);
		nl_socket_free(socket);

		nl_error = ret;
		return NETCTL_MESH_LEAVE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for leaving mesh network sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer. callback_dump() prints SSIDs to stdout.

	nlmsg_free(msg);
	nl_socket_free(socket);

	if(ret != 0) {
		nl_error = ret;
		return NETCTL_MESH_LEAVE_ERROR;
	}

	return ret;
}

int connectToMesh(Interface* itf, Network* net) {
	// Open socket to kernel.
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	nl_socket_disable_seq_check(socket);

	// Now get info for all SSIDs detected.
	struct nl_msg *msg = nlmsg_alloc();  // Allocate a message.

	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_JOIN_MESH, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, itf->id);
	nla_put(msg, NL80211_ATTR_MESH_ID, strlen(net->ssid), net->ssid);
	nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, net->freq);

	int ret = nl_send_auto(socket, msg);
	if(ret < 0) {
		nlmsg_free(msg);
		nl_socket_free(socket);

		nl_error = ret;
		return NETCTL_MESH_JOIN_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for joining mesh network sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);

	nlmsg_free(msg);
	nl_socket_free(socket);

	if(ret != 0) {
		nl_error = ret;
		return NETCTL_MESH_JOIN_ERROR;
	}


	return ret;
}


/*********************************************************
 * AdHoc Network related functions
 *********************************************************/

int leaveAdHoc(Interface* itf) {
	// Open socket to kernel.
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	struct nl_msg *msg = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_LEAVE_IBSS, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, itf->id);
	int ret = nl_send_auto(socket, msg);  // Send the message.
	if(ret < 0) {

		nlmsg_free(msg);
		nl_socket_free(socket);

		nl_error = ret;
		return NETCTL_ADHOC_LEAVE_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for leaving adhoc network sent to kernel, %d bytes sent.\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer. callback_dump() prints SSIDs to stdout.

	nlmsg_free(msg);
	nl_socket_free(socket);

	if(ret != 0) {
		nl_error = ret;
		return NETCTL_ADHOC_LEAVE_ERROR;
	}

	return ret;
}

static int retry = 0;

static int printConnectError(struct sockaddr_nl *nla, struct nlmsgerr *nlerr, void *arg) {

	fprintf(stderr, " ======== > Error returned by kernel: %d %s\n", nlerr->error, strerror(-nlerr->error));
	perror("KERNEL: ");

	if(-nlerr->error == 114) //TODO put macro E (something) and enforce..
		retry = 1;
	return NL_STOP;
}

int connectToAdHoc(Interface* itf, Network* net) {
	// Open socket to kernel.
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	nl_socket_disable_seq_check(socket);

	// Now get info for all SSIDs detected.
	struct nl_msg *msg = nlmsg_alloc();  // Allocate a message.

	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_JOIN_IBSS, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, itf->id);
	nla_put(msg, NL80211_ATTR_SSID, strlen(net->ssid), net->ssid);
	nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, net->freq);

	int ret = nl_send_auto(socket, msg);
	if(ret < 0) {

		nlmsg_free(msg);
		nl_socket_free(socket);

		nl_error = ret;
		fprintf(stderr, "Error sending join ad hoc message nl error %d\n", ret);
		return NETCTL_ADHOC_JOIN_ERROR;
	}
#ifdef DEBUG
	else
		printf("Request for joining adhoc network sent to kernel, %d bytes sent.\n", ret);
#endif
	fprintf(stderr, "===============> TRYING TO RECEIVE NETLINK MSG <====================\n");

	 nl_socket_modify_err_cb(socket,  NL_CB_CUSTOM, printConnectError, NULL);

	ret = nl_recvmsgs_default(socket);
	perror("On nl_recvmsgs_default: ");
	nlmsg_free(msg);
	nl_socket_free(socket);

	if(retry) {
		retry = 0;
		fprintf(stderr, "===============> RETRY CONNECT <====================\n");
		leaveAdHoc(itf);
		ret = connectToAdHoc(itf, net);
	}


	if(ret != 0) {
		nl_error = ret;
		fprintf(stderr, "Error receiving join ad hoc message nl error %d\n", ret);
		return NETCTL_ADHOC_JOIN_ERROR;
	}

	return ret;
}

/********************************************************
 * Find nearby networks
 ********************************************************/

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) {
    // Callback for errors.
#ifdef DEBUG
    printf("error_handler() called.\n");
#endif
    int *ret = arg;
    *ret = err->error;
    return NL_STOP;
}


static int finish_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_FINISH.
    int *ret = arg;
    *ret = 0;
    return NL_SKIP;
}


static int ack_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_ACK.
    int *ret = arg;
    *ret = 0;
    return NL_STOP;
}


static int no_seq_check(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_SEQ_CHECK.
    return NL_OK;
}

static int callback_trigger(struct nl_msg *msg, void *arg) {
    // Called by the kernel when the scan is done or has been aborted.
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    struct trigger_results *results = arg;

    //printf("Got something.\n");
    //printf("%d\n", arg);
    //nl_msg_dump(msg, stdout);

    if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
        //printf("Got NL80211_CMD_SCAN_ABORTED.\n");
        results->done = 1;
        results->aborted = 1;
    } else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
        //printf("Got NL80211_CMD_NEW_SCAN_RESULTS.\n");
        results->done = 1;
        results->aborted = 0;
    }  // else probably an uninteresting multicast message.

    return NL_SKIP;
}

int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id) {
    // Starts the scan and waits for it to finish. Does not return until the scan is done or has been aborted.
    struct trigger_results results = { .done = 0, .aborted = 0 };
    struct nl_msg *msg;
    struct nl_cb *cb;
    struct nl_msg *ssids_to_scan;
    int err;
    int ret;
    int mcid = genl_ctrl_resolve_grp(socket, "nl80211", "scan");
    nl_socket_add_membership(socket, mcid);  // Without this, callback_trigger() won't be called.

    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        printf("ERROR: Failed to allocate netlink message for msg.\n");
        return -ENOMEM;
    }
    ssids_to_scan = nlmsg_alloc();
    if (!ssids_to_scan) {
        printf("ERROR: Failed to allocate netlink message for ssids_to_scan.\n");
        nlmsg_free(msg);
        return -ENOMEM;
    }
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        printf("ERROR: Failed to allocate netlink callbacks.\n");
        nlmsg_free(msg);
        nlmsg_free(ssids_to_scan);
        return -ENOMEM;
    }

    // Setup the messages and callback handler.
    genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);  // Setup which command to run.
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attribute, which interface to use.
    nla_put(ssids_to_scan, 1, 0, "");  // Scan all SSIDs.
    nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids_to_scan);  // Add message attribute, which SSIDs to scan for.
    nlmsg_free(ssids_to_scan);  // Copied to `msg` above, no longer need this.
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback_trigger, &results);  // Add the callback.
    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);  // No sequence checking for multicast messages.

    // Send NL80211_CMD_TRIGGER_SCAN to start the scan. The kernel may reply with NL80211_CMD_NEW_SCAN_RESULTS on
    // success or NL80211_CMD_SCAN_ABORTED if another scan was started by another process.
    err = 1;
    ret = nl_send_auto(socket, msg);  // Send the message.
#ifdef DEBUG
    printf("NL80211_CMD_TRIGGER_SCAN sent %d bytes to the kernel.\n", ret);
    printf("Waiting for scan to complete...\n");
#endif
    while (err > 0) ret = nl_recvmsgs(socket, cb);  // First wait for ack_handler(). This helps with basic errors.
#ifdef DEBUG
    if (err < 0) {
        printf("WARNING: err has a value of %d.\n", err);
    }
#endif
    if (ret < 0) {
#ifdef DEBUG
        printf("ERROR: nl_recvmsgs() returned %d (%s).\n", ret, nl_geterror(-ret));
#endif
        return ret;
    }
    while (!results.done) nl_recvmsgs(socket, cb);  // Now wait until the scan is done or aborted.
    if (results.aborted) {
#ifdef DEBUG
        printf("ERROR: Kernel aborted scan.\n");
#endif
        return 1;
    }
#ifdef DEBUG
    printf("Scan is done.\n");
#endif

    // Cleanup.
    nlmsg_free(msg);
    nl_cb_put(cb);
    nl_socket_drop_membership(socket, mcid);  // No longer need this.
    return 0;
}

static int scanNetwork_callback_dump(struct nl_msg *msg, void *arg) {
	Network* net = NULL;

	// Called by the kernel with a dump of the successful scan's data. Called for each SSID.
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	//nl_msg_dump(msg, stdout);
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
			[NL80211_BSS_TSF] = { .type = NLA_U64 },
			[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
			[NL80211_BSS_BSSID] = { },
			[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
			[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
			[NL80211_BSS_INFORMATION_ELEMENTS] = { },
			[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
			[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
			[NL80211_BSS_STATUS] = { .type = NLA_U32 },
			[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
			[NL80211_BSS_BEACON_IES] = { },
			[NL80211_BSS_CHAN_WIDTH] = { .type = NLA_U32 },
	};

	// Parse and error check.
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_BSS]) {
		printf("bss info missing!\n");
		return NL_SKIP;
	}

	if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_SKIP;
	}
	if (!bss[NL80211_BSS_BSSID]) return NL_SKIP;
	if (!bss[NL80211_BSS_INFORMATION_ELEMENTS]) return NL_SKIP;

	char* net_ssid = getSSID(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
	if(net_ssid != NULL) {
		net = alloc_network(net_ssid);
		mac_addr_n2a(net->mac_addr, nla_data(bss[NL80211_BSS_BSSID]));
		net->freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
		net->isIBSS = (((nla_get_u16(bss[NL80211_BSS_CAPABILITY]) & (1<<1)) != 0) ? 1:0);

		if (bss[NL80211_BSS_STATUS]) {
				switch (nla_get_u32(bss[NL80211_BSS_STATUS])) {
				case NL80211_BSS_STATUS_AUTHENTICATED:
				case NL80211_BSS_STATUS_ASSOCIATED:
				case NL80211_BSS_STATUS_IBSS_JOINED:
					net->connected = 1;
					break;
				default:
					net->connected = 0;
					break;
				}
			}

		if(isMesh(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]))) {
			net->mesh_info = (Mesh*) malloc(sizeof(Mesh));
			fillMeshInformation(net->mesh_info, nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
		}

		if(existsNetwork(net->ssid) == 0) {
			net->next = knownNetworks;
			knownNetworks = net;
		} else {
			free_network(net);
		}
	}

	return NL_SKIP;
}

Network* scanNetworks(Channel* ch) {

	// Open socket to kernel.
	struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
	genl_connect(socket);  // Create file descriptor and bind socket.
	int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

	nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, scanNetwork_callback_dump, NULL);  // Add the callback.
	nl_socket_disable_seq_check(socket);

	// Now get info for all SSIDs detected.
	struct nl_msg *msg = nlmsg_alloc();  // Allocate a message.
	genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);  // Setup which command to run.
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, ch->ifindex);  // Add message attribute, which interface to use.

	int ret = 0;
	// Issue NL80211_CMD_TRIGGER_SCAN to the kernel and wait for it to finish.
	ret = do_scan_trigger(socket, ch->ifindex, driver_id);
	if (ret != 0) {
#ifdef DEBUG
		printf("do_scan_trigger() failed with %d.\n", ret);
#endif
		return NULL;
	}
	ret = nl_send_auto(socket, msg);  // Send the message.
#ifdef DEBUG
		printf(" ==== NL80211_CMD_GET_SCAN sent %d bytes to the kernel. ====\n", ret);
#endif
	ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer. callback_dump() prints SSIDs to stdout.
	if (ret < 0) {
		fprintf(stderr, "ERROR: nl_recvmsgs_default() returned %d (%s).\n", ret, nl_geterror(-ret));
	}
	nlmsg_free(msg);

	return knownNetworks;
}
