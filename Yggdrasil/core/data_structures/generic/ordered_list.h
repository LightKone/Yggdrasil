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
	void* data; //a generic pointer to the data type stored in the list
	struct __ordered_list_item* next; //a pointer to the next element in the list (NULL if this item is the last element)

	struct __ordered_list_item* prev; //a pointer to the previous element in the list (NULL if this item is the first element)
}ordered_list_item;

typedef struct _ordered_list {
	short size; //number of elements in the list
	ordered_list_item* head; //pointer to the head of the list
	ordered_list_item* tail; //pointer to the tail of the list

	compare_function compare; //a function to compare the elements in the list
}ordered_list;


/**
 * Initialize an empty ordered list
 * @param compare the function used to compare (and order) the elements in the list
 * @return a pointer to a an empty ordered list configured to order by the provided function
 */
ordered_list* ordered_list_init(compare_function compare);

/**
 * Add an item to the ordered list
 * @param l the ordered list to be added to
 * @param item the item to be added
 */
void ordered_list_add_item(ordered_list* l, void* item);

/**
 * Remove the item pointed by item from the ordered list
 * @param l the ordered list to be removed from
 * @param item the item to be removed
 * @return the removed item data or NULL if no item was removed
 */
void* ordered_list_remove(ordered_list* l, ordered_list_item* item);

/**
 * Remove a specific item from the ordered list
 * @param l the ordered list to be removed from
 * @param equal a function to compare items in the list (exact match boolean)
 * @param to_remove the item to be removed
 * @return the removed item data or NULL if no item was removed
 */
void* ordered_list_remove_item(ordered_list* l, equal_function equal, void* to_remove);

/**
 * Find a specific item from the ordered list
 * @param l the ordered list
 * @param equal a function to compare items in the list (exact match boolean)
 * @param to_find the item to be found
 * @return the found item data or NULL if no item was found
 */
void* ordered_list_find_item(ordered_list* l, equal_function equal, void* to_find);

/**
 * Return the item in ordered list that has the given index
 * @param l the ordered list
 * @param index the index of the desired item
 * @return the retrieved item data or NULL if no item was found
 */
void* ordered_list_get_item_by_index(ordered_list* l, int index);

/**
 * Remove from the head of the ordered list
 * @param l the ordered list
 * @return the removed item data or NULL if no item was removed
 */
void* ordered_list_remove_head(ordered_list* l);

/**
 * Remove from the tail of the ordered list
 * @param l the ordered list
 * @return the removed item data or NULL if no item was removed
 */
void* ordered_list_remove_tail(ordered_list* l);

/**
 * Update the item pointed by newdata in the ordered list.
 * Newdata will be added to list, while the old data will be removed.
 * If newdata does not exist in the ordered list the item is added to the ordered list
 * @param l the ordered list
 * @param equal a function to compare items in the ordered list (exact match boolean)
 * @param newdata the data to be updated
 * @return a pointer to the old data or NULL if newdata was not in the list.
 */
void* ordered_list_update_item(ordered_list* l, equal_function equal, void* newdata);

#endif /* DATA_STRUCTURES_GENERIC_ORDERED_LIST_H_ */
