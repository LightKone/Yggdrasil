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

#include "neighbour_list.h"


neighbour_item* new_neighbour(uuid_t neighbour_id, WLANAddr addr, void* attribute, unsigned short attribute_len, destroy_function attribute_destroy) {
	neighbour_item* newnei = malloc(sizeof(neighbour_item));
	memcpy(newnei->id, neighbour_id, sizeof(uuid_t));
	memcpy(newnei->addr.data, addr.data, WLAN_ADDR_LEN);
	newnei->attribute_destroy = NULL;

	if(attribute != NULL && attribute_len > 0) {
		newnei->attribute = malloc(attribute_len);
		memcpy(newnei->attribute, attribute, attribute_len);
		newnei->attribute_destroy = attribute_destroy;
	}
	else
		newnei->attribute = NULL;

	return newnei;
}

neighbour_item* neighbour_set_attribute(neighbour_item* neighbour, void* attribute, unsigned short attribute_len, destroy_function attribute_destroy) {
	if(neighbour->attribute == NULL && attribute != NULL && attribute_len > 0) {
		neighbour->attribute = malloc(attribute_len);
		memcpy(neighbour->attribute, attribute, attribute_len);
		neighbour->attribute_destroy = attribute_destroy;
	}

	return neighbour;
}

bool equal_neigh_uuid(void* item, void* to_compare) {
	return uuid_compare(((neighbour_item*)item)->id, to_compare) == 0;
}

neighbour_item* neighbour_find(list* lst, uuid_t neighbour_id) {
	void* item = list_find_item(lst, equal_neigh_uuid, neighbour_id);
	if(item)
		return (neighbour_item*) item;
	return NULL;
}

static bool equal_neigh_addr(void* item, void* to_compare) {
	return memcmp(((neighbour_item*)item)->addr.data, ((WLANAddr*)to_compare)->data, WLAN_ADDR_LEN) == 0;
}

neighbour_item* neighbour_find_by_addr(list* lst, WLANAddr* addr) {
	void* item = list_find_item(lst, equal_neigh_addr, addr);
	if(item)
		return (neighbour_item*) item;

	return NULL;
}

void neighbour_add_to_list(list* lst, neighbour_item* new_neighbour) {
	list_add_item_to_head(lst, (void*) new_neighbour);
}

short neighbour_rm_from_list(list* lst, uuid_t neighbour_id) {

	void* item = list_remove_item(lst, equal_neigh_uuid, neighbour_id);
	if(item) {
		neighbour_item_destroy((neighbour_item*) item);
		return SUCCESS;
	}
	return FAILED;

}

void neighbour_item_destroy(neighbour_item* to_destroy) {
	if(to_destroy->attribute) {
		if(to_destroy->attribute_destroy)
			to_destroy->attribute_destroy(to_destroy->attribute);
		else
			free(to_destroy->attribute);
	}
	free(to_destroy);
}

void neighbour_list_destroy(list* lst) {
	while(lst->size > 0) {
		neighbour_item* item = (neighbour_item*)list_remove_head(lst);
		neighbour_item_destroy(item);
	}
}
