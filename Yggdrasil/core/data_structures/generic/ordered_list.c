/*
 * ordered_list.c
 *
 *  Created on: Apr 3, 2019
 *      Author: akos
 */


#include "ordered_list.h"

ordered_list* ordered_list_init(compare_function compare) {
	ordered_list* l = malloc(sizeof(ordered_list));
	l->compare = compare;
	l->head = NULL;
	l->tail = l->head;

	return l;
}

void ordered_list_add_item(ordered_list* l, void* item) {
	ordered_list_item* new_item = malloc(sizeof(ordered_list_item));
	new_item->data = item;

	new_item->prev = NULL;
	new_item->next = NULL;

	if(l->size == 0) {
		l->head = new_item;
		l->tail = new_item;

	} else {
		ordered_list_item* it = l->head;
		ordered_list_item* prev = NULL;
		while(it) {
			if(l->compare(it->data, item) > 0)
				break;

			prev = it;
			it = it->next;
		}

		if(!prev) { //put in head
			new_item->next = l->head;
			l->head->prev = new_item;
			l->head = new_item;
		}else if(!it) { //put in tail
			new_item->prev = l->tail;
			l->tail->next = new_item;
			l->tail = new_item;
		} else { //put in middle
			prev->next = new_item;
			new_item->prev = prev;

			it->prev = new_item;
			new_item->next = it;
		}

	}


	l->size ++;
}


void* ordered_list_remove(ordered_list* l, ordered_list_item* item) {
	void* data = NULL;

	if(item) {
		ordered_list_item* prev = item->prev;
		ordered_list_item* next = item->next;

		if(prev && next) { //middle
			prev->next = next;
			next->prev = prev;
		} else if(next && !prev) { //head
			next->prev = NULL;
			l->head = next;
		} else if(prev && !next) { //tail
			prev->next = NULL;
			l->tail = prev;
		} else { //1 element
			l->head = NULL;
			l->tail = NULL;
		}

		item->next = NULL;
		item->prev = NULL;
		data = item->data;
		free(item);
		l->size --;
	}

	return data;
}

void* ordered_list_remove_item(ordered_list* l, equal_function equal, void* to_remove) {
	void* data = NULL;

	ordered_list_item* it = l->head;
	while(it) {
		if(equal(it->data, to_remove)) {
			data = ordered_list_remove(l, it);
			break;
		}
		it = it->next;
	}

	return data;
}

void* ordered_list_find_item(ordered_list* l, equal_function equal, void* to_find) {
	ordered_list_item* it = l->head;
	void* data_item = NULL;
	while(it != NULL) {
		if(equal(it->data, to_find)) {
			return it->data;
		}
		it = it->next;
	}
	return data_item;
}

void* ordered_list_get_item_by_index(ordered_list* l, int index) {
	void* data = NULL;

	int i = 0;
	ordered_list_item* it = l->head;
	for(; it != NULL && i < index; it = it->next, i++);

	if(it)
		data = it->data;

	return data;
}

void* ordered_list_remove_head(ordered_list* l) {
	return ordered_list_remove(l, l->head);
}

void* ordered_list_remove_tail(ordered_list* l) {
	return ordered_list_remove(l, l->tail);
}


void* ordered_list_update_item(ordered_list* l, equal_function equal, void* newdata) {
	void* old_data = NULL;

	ordered_list_item* new_item = malloc(sizeof(ordered_list_item));
	new_item->data = newdata;

	new_item->prev = NULL;
	new_item->next = NULL;

	if(l->size == 0) {
		l->head = new_item;
		l->tail = new_item;

	} else {
		ordered_list_item* it = l->head;
		ordered_list_item* prev = NULL;
		bool added = false;
		while(it) {
			if(!added && l->compare(it->data, newdata) > 0) { //add new data
				if(!prev) { //put in head
					new_item->next = l->head;
					l->head->prev = new_item;
					l->head = new_item;

				} else { //put in middle
					prev->next = new_item;
					new_item->prev = prev;

					it->prev = new_item;
					new_item->next = it;
				}
				added = true;

				prev = it;
				it = it->next;
			} else if(equal(it->data, newdata)) { //remove old data
				ordered_list_item* torm = it;
				it = it->next;
				old_data = ordered_list_remove(l, torm);
			} else {
				prev = it;
				it = it->next;
			}
		}

		if(!added && l->size > 0) { //put in tail
			new_item->prev = l->tail;
			l->tail->next = new_item;
			l->tail = new_item;
		} else if(!added && l->size == 0) {
			l->head = new_item;
			l->tail = new_item;
		}

	}

	l->size ++;

	return old_data;
}
