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

#ifndef YGG_LL_CONSTANTS_H_
#define YGG_LL_CONSTANTS_H_

#include <linux/nl80211.h>
#include <linux/filter.h>


/*********************************************************
 * Default values
 *********************************************************/

#define DEFAULT_FREQ 2412

/*********************************************************
 * Remaping of interface modes support from nl80211.h
 *********************************************************/
typedef enum _IFTYPE {
	IFTYPE_UNSPEC = NL80211_IFTYPE_UNSPECIFIED,
	IFTYPE_ADHOC = NL80211_IFTYPE_ADHOC,
	IFTYPE_STATION = NL80211_IFTYPE_STATION,
	IFTYPE_AP = NL80211_IFTYPE_AP,
	IFTYPE_AP_VLAN = NL80211_IFTYPE_AP_VLAN,
	IFTYPE_WDS = NL80211_IFTYPE_WDS,
	IFTYPE_MONITOR = NL80211_IFTYPE_MONITOR,
	IFTYPE_MESH = NL80211_IFTYPE_MESH_POINT,
	IFTYPE_P2P_CLI = NL80211_IFTYPE_P2P_CLIENT,
	IFTYPE_P2P_GO = NL80211_IFTYPE_P2P_GO,
	IFTYPE_P2P_DEV = NL80211_IFTYPE_P2P_DEVICE,
	IFTYPE_MAX = 11
} IFTYPE;


/**********************************************************
 * Filters
 *********************************************************/

const struct sock_filter YGG_filter[8];

#endif /* YGG_LL_CONSTANTS_H_ */
