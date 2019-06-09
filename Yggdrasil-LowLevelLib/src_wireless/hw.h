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

#ifndef YGG_LL_HW_H_
#define YGG_LL_HW_H_

#include <errno.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <linux/filter.h>

#include "utils.h"
#include "data_struct.h"
#include "constants.h"
#include "errors.h"

/**********************************************************
 * Interface
 **********************************************************/
int removeInterface(Interface* itf);
int createInterface(Phy* phy, Interface* itf);
int listInterfaces(Interface** itfs, Phy* physicalDev);

int set_ip_addr(Channel* ch);
int set_if_flags(Channel* ch, char *interface, short flags);
int setInterfaceUP(Channel* ch, char* interface);
int setInterfaceDOWN(Channel* ch, char* interface);
int checkInterfaceUP(Channel* ch, char* interface);
int checkInterfaceConnected(Channel* ch, char* interface);

int setInterfaceType(int id, int type);
/**********************************************************
 * Physical Device
 **********************************************************/
int listDevices(Phy** phy, int target_mode);
Phy* filterDevicesByModesSupported(Phy* devices);

/**********************************************************
 * Kernel filter
 **********************************************************/
int defineLKMFilter(Channel* ch);
int defineFilter(Channel* ch, struct sock_filter* filter);


/**********************************************************
 * Retrive sys info
 **********************************************************/
double get_cpu_temp();

#endif /* YGG_LL_HW_H_ */
