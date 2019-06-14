/*
 * ordered_list.h
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */

#ifndef DATA_STRUCTURES_GENERIC_ORDERED_LIST_H_
#define DATA_STRUCTURES_GENERIC_ORDERED_LIST_H_

#include "utils/utils.h"
#include "list.h"

#include <stdlib.h>

typedef struct __ordered_list_item {
	void* data;
	struct __ordered_list_item* next;

	struct __ordered_list_item* prev;
}ordered_list_item;

typedef struct _ordered_list {
	short size;
	ordered_list_item* head;
	ordered_list_item* tail;

	compare_function compare;
}ordered_list;


ordered_list* ordered_list_init(compare_function compare);

void ordered_list_add_item(ordered_list* l, void* item);

void* ordered_list_remove(ordered_list* l, ordered_list_item* item);

void* ordered_list_remove_item(ordered_list* l, equal_function equal, void* to_remove);

void* ordered_list_find_item(ordered_list* l, equal_function equal, void* to_find);

void* ordered_list_get_item_by_index(ordered_list* l, int index);

void* ordered_list_remove_head(ordered_list* l);

void* ordered_list_remove_tail(ordered_list* l);

void* ordered_list_update_item(ordered_list* l, equal_function equal, void* newdata);

#endif /* DATA_STRUCTURES_GENERIC_ORDERED_LIST_H_ */
