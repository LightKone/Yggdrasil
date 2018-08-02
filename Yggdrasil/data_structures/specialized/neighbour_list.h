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

#ifndef DATA_STRUCTURES_SPECIALIZED_NEIGHBOUR_LIST_H_
#define DATA_STRUCTURES_SPECIALIZED_NEIGHBOUR_LIST_H_

#include "data_structures/generic/list.h"
#include "Yggdrasil_lowlvl.h"

#include <uuid/uuid.h>

typedef struct _neighbour{
	uuid_t id;
	WLANAddr addr;
	void* attribute;
	destroy_function attribute_destroy;
}neighbour_item;

neighbour_item* new_neighbour(uuid_t neighbour_id, WLANAddr addr, void* attribute, unsigned short attribute_len, destroy_function attribute_destroy);

neighbour_item* neighbour_set_attribute(neighbour_item* neighbour, void* attribute, unsigned short attribute_len, destroy_function attribute_destroy);

neighbour_item* neighbour_find(list* lst, uuid_t neighbour_id);

neighbour_item* neighbour_find_by_addr(list* lst, WLANAddr* addr);

void neighbour_add_to_list(list* lst, neighbour_item* new_neighbour);

short neighbour_rm_from_list(list* lst, uuid_t neighbour_id);

void neighbour_list_destroy(list* lst);

void neighbour_item_destroy(neighbour_item* to_destroy);

bool equal_neigh_uuid(void* item, void* to_compare);

#endif /* DATA_STRUCTURES_SPECIALIZED_NEIGHBOUR_LIST_H_ */
