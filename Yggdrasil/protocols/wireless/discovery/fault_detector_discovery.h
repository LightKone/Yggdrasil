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


#ifndef PROTOCOLS_DISCOVERY_FAULT_DETECTOR_DISCOVERY_H_
#define PROTOCOLS_DISCOVERY_FAULT_DETECTOR_DISCOVERY_H_

#include "core/ygg_runtime.h"
#include "interfaces/discovery/discovery_events.h"
#include "data_structures/specialized/neighbour_list.h"

#define PROTO_FAULT_DETECTOR_DISCOVERY_ID 101

typedef struct _fault_detector_discovery_args {
	time_t announce_period_s;
	unsigned long announce_period_ns;

	unsigned short mgs_lost_per_fault; //0 fault detector off; number of messages to be missed before suspect;
	unsigned short black_list_links; //0 off; number of consecutive faults before black list;
}fault_detector_discovery_args;

fault_detector_discovery_args* fault_detector_discovery_args_init(time_t announce_period_s, unsigned long announce_period_ns, unsigned short mgs_lost_per_fault, unsigned short black_list_links);
void fault_detector_discovery_args_destroy(fault_detector_discovery_args* fdargs);

proto_def* fault_detector_discovery_init(void* args);

#endif /* PROTOCOLS_DISCOVERY_FAULT_DETECTOR_DISCOVERY_H_ */
