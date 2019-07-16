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

#include "../../utils/utils.h"
#include <stdlib.h>


typedef struct _list_item {
	void* data; //a generic pointer to the data type stored in the list
	struct _list_item* next; //a pointer to the next element in the list (NULL if this item is the last element)
}list_item;

typedef struct _list {
	short size; //number of elements in the list
	list_item* head; //pointer to the head of the list
	list_item* tail; //pointer to the tail of the list
}list;

/**
 * Initialize an empty list
 * @return a pointer to a an empty list
 */
list* list_init();

/**
 * Add an item to the head of the list
 * @param l the list to add the item to
 * @param item a generic pointer to the item to be added
 */
void list_add_item_to_head(list* l, void* item);

/**
 * Add an item to the tail of the list
 * @param l the list to add the item to
 * @param item a generic pointer to the item to be added
 */
void list_add_item_to_tail(list* l, void* item);

/**
 * Remove the next item pointed by previous
 * @param l the list to remove from
 * @param previous the pointer to the previous element
 * @return a pointer to the removed item or NULL if no item was removed
 */
void* list_remove(list* l, list_item* previous);

/**
 * Remove a specific item from the list
 * @param l the list to remove from
 * @param equal a function to compare the items in the list in this operation (exact match boolean)
 * @param to_remove the item to remove from the list
 * @return a pointer to the removed item or NULL if no item was removed
 */
void* list_remove_item(list* l, equal_function equal, void* to_remove);

/**
 * Find a specific item in the list
 * @param l the list
 * @param equal a function to compare the items in the list in this operation (exact match boolean)
 * @param to_find the item to find
 * @return a pointer to the item found or NULL if no item was found
 */
void* list_find_item(list* l, equal_function equal, void* to_find);

/**
 * Remove the head of the list
 * @param l the list to remove from
 * @return the item that was removed or NULL if no item was removed
 */
void* list_remove_head(list* l);

#endif /* DATA_STRUCTURES_GENERIC_LIST_H_ */
