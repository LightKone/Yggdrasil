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

#include "list.h"

list* list_init() {
	list* l = malloc(sizeof(list));
	l->size = 0;
	l->head = NULL;
	l->tail = l->head;

	return l;
}

void list_add_item_to_head(list* l, void* item) {
	list_item* new_item = malloc(sizeof(list_item));
	new_item->data = item;
	new_item->next = l->head;
	l->head = new_item;

	if(l->size == 0)
		l->tail = l->head;

	l->size ++;
}

void list_add_item_to_tail(list* l, void* item) {
	list_item* new_item = malloc(sizeof(list_item));
	new_item->data = item;
	new_item->next = NULL;
	if(l->size == 0) {
		new_item->next = l->head;
		l->head = new_item;
		l->tail = l->head;
	} else {
		new_item->next = NULL;
		l->tail->next = new_item;
		l->tail = new_item;
	}

	l->size ++;
}

void* list_remove(list* l, list_item* previous) {
	void* data_torm = NULL;
	if(previous != NULL && l != NULL) {
		list_item* torm = previous->next;
		if(torm == l->tail) {
			l->tail = previous;
			l->tail->next = NULL;
		} else {
			previous->next = torm->next;
			torm->next = NULL;
		}

		data_torm = torm->data;
		free(torm);

		l->size--;
	}
	return data_torm;
}

void* list_remove_item(list* l, equal_function equal, void* to_remove) {
	list_item* it = l->head;
	void* data_torm = NULL;

	if(it != NULL) {
		if(equal(it->data, to_remove)){
			data_torm = it->data;
			list_item* torm = it;
			l->head = torm->next;
			if(torm == l->tail) {
				l->tail = l->head;
			}
			torm->next = NULL;
			free(torm);
			l->size --;
		} else {
			while(it->next != NULL) {
				if(equal(it->next->data, to_remove)) {
					data_torm = list_remove(l, it);
					break;
				}
				it = it->next;
			}
		}
	}

	return data_torm;
}

void* list_find_item(list* l, equal_function equal, void* to_find) {
	list_item* it = l->head;
	void* data_item = NULL;
	while(it != NULL) {
		if(equal(it->data, to_find)) {
			return it->data;
		}
		it = it->next;
	}
	return data_item;
}

void* list_remove_head(list* l) {
	list_item* it = l->head;
	void* data_torm = NULL;

	if(it != NULL) {
		data_torm = it->data;
		list_item* torm = it;
		l->head = torm->next;
		if(torm == l->tail) {
			l->tail = l->head;
		}
		free(torm);
		l->size --;
	}

	return data_torm;
}
