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

#ifndef PROTOCOLS_COMMUNICATION_POINT_TO_POINT_RELIABLE_H_
#define PROTOCOLS_COMMUNICATION_POINT_TO_POINT_RELIABLE_H_

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "core/ygg_runtime.h"
#include "Yggdrasil_lowlvl.h"

#include "core/utils/utils.h"

#include "data_structures/generic/list.h"

#define PROTO_P2P_RELIABLE_DELIVERY 150

typedef enum reliable_p2p_events_{
	FAILED_DELIVERY
}reliable_p2p_events;

proto_def * reliable_point2point_init(void * args);


#endif /* PROTOCOLS_COMMUNICATION_POINT_TO_POINT_RELIABLE_H_ */
