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

#ifndef DATA_STRUCTURES_GENERIC_LIST_H_
#define DATA_STRUCTURES_GENERIC_LIST_H_

#include "core/utils/utils.h"
#include <stdlib.h>

typedef struct _list_item {
	void* data;
	struct _list_item* next;
}list_item;

typedef struct _list {
	short size;
	list_item* head;
	list_item* tail;
}list;

typedef bool (*comparator_function)(void*, void*);

list* list_init();

void list_add_item_to_head(list* l, void* item);

void list_add_item_to_tail(list* l, void* item);

void* list_remove(list* l, list_item* previous);

void* list_remove_item(list* l, comparator_function equal, void* to_remove);

void* list_find_item(list* l, comparator_function equal, void* to_find);

void* list_remove_head(list* l);

#endif /* DATA_STRUCTURES_GENERIC_LIST_H_ */
