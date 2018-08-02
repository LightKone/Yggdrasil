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

#ifndef YGG_LL_API_H_
#define YGG_LL_API_H_

#include <unistd.h>

#include "data_struct.h"
#include "hw.h"
#include "network.h"
#include "utils.h"
#include "errors.h"


/*********************************************************
 * Setup
 *********************************************************/

int setupSimpleChannel(Channel* ch, NetworkConfig* ntconf);
int setupChannelNetwork(Channel* ch, NetworkConfig* ntconf);

/*********************************************************
 * Basic I/O
 *********************************************************/

/**
 * Send a message through the channel to the destination defined
 * in the message
 * @param ch The channel
 * @param message The message to be sent
 * @return The number of bytes sent through the channel
 */
int chsend(Channel* ch, YggPhyMessage* message);

/**
 * Send a message through the channel to the given address
 * @param ch The channel
 * @param message The message to be sent
 * @param addr The mac address of the destination
 * @return The number of bytes sent through the channel
 */
int chsendTo(Channel* ch, YggPhyMessage* message, char* addr);

/**
 * Send a message through the channel to the broadcast address
 * (one hop broadcast)
 * @param ch The channel
 * @param message The message to be sent
 * @return The number of bytes sent through the channel
 */
int chbroadcast(Channel* ch, YggPhyMessage* message);

/**
 * Receive a message through the channel
 * @param ch The channel
 * @param message The message to be received
 * @return The number of bytes received
 */
int chreceive(Channel* ch, YggPhyMessage* message);

#endif /* YGG_LL_API_H_ */
