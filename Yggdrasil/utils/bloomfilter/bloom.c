/***************************************************
 * This code was taken from:
 * https://github.com/MatthiasWinkelmann/bloom-filter
 ***************************************************/

#include "bloom.h"

struct bloom_hash {
	hash_function func;
	struct bloom_hash *next;
};

struct bloom_filter {
	struct bloom_hash *func;
	void *bits;
	size_t size;
};

bloom_t bloom_create(size_t size) {
	bloom_t res = calloc(1, sizeof(struct bloom_filter));
	res->size = size;
	res->bits = malloc(size);
	memset(res->bits, 0, size);
	return res;
}

void bloom_free(bloom_t filter) {
	if (filter) {
		while (filter->func) {
			struct bloom_hash *h = filter->func;
			filter->func = h->next;
			free(h);
		}
		free(filter->bits);
		free(filter);
	}
}

void bloom_add_hash(bloom_t filter, hash_function func) {
	struct bloom_hash *h = calloc(1, sizeof(struct bloom_hash));
	h->func = func;
	struct bloom_hash *last = filter->func;
	while (last && last->next) {
		last = last->next;
	}
	if (last) {
		last->next = h;
	} else {
		filter->func = h;
	}
}

void bloom_add(bloom_t filter, const void *item, unsigned int item_len) {
	struct bloom_hash *h = filter->func;
	uint8_t *bits = filter->bits;

	while (h) {
		unsigned int hash = h->func(item, item_len);

		hash %= filter->size * 8;
		bits[hash / 8] |= 1 << hash % 8;
		h = h->next;
	}

}

int bloom_test(bloom_t filter, const void *item, unsigned int item_len) {
	struct bloom_hash *h = filter->func;
	uint8_t *bits = filter->bits;
	while (h) {
		unsigned int hash = h->func(item, item_len);
		hash %= filter->size * 8;
		if (!(bits[hash / 8] & 1 << hash % 8)) {
			return 0;
		}
		h = h->next;
	}
	return 1;
}

int bloom_test_disjoin(bloom_t filter, const void* set, size_t size) {
	if(filter->size != size){
		printf("Bloom filter has a different size than the set provided. Operation test disjoin aborting\n");
		return 0;
	}
	const char * filter_bits = filter->bits;
	const char * set_bits = set;
	int i;
	for(i = 0; i < size; i++){
		if((*filter_bits & *set_bits) != 0){
			return 0;
		}
		filter_bits ++;
		set_bits ++;
	}
	return 1;
}

int bloom_test_equal(bloom_t filter, const void* set, size_t size) {
	if(filter->size != size){
		printf("Bloom filter has a different size than the set provided. Operation test equal aborting\n");
		return 0;
	}
	const char * filter_bits = filter->bits;
	const char * set_bits = set;
	int i;
	for(i = 0; i < size; i++){
		if((*filter_bits ^ *set_bits) != 0){
			return 0;
		}
		filter_bits ++;
		set_bits ++;
	}
	return 1;
}

int bloom_merge(bloom_t filter, const void* set, size_t size) {
	if(filter->size != size){
		printf("Bloom filter has a different size than the set provided. Operation merge aborting\n");
		return 0;
	}
	char * filter_bits = filter->bits;
	const char * set_bits = set;
	int i;
	for(i = 0; i < size; i++){
		*filter_bits = *filter_bits | *set_bits;
		filter_bits ++;
		set_bits ++;
	}

	return 1;

}

int bloom_swap(bloom_t filter, const void* set, size_t size) {
	if(filter->size != size){
		printf("Bloom filter has a different size than the set provided. Operation swap aborting\n");
		return 0;
	}
	memcpy(filter->bits, set, size);

	return 1;
}

void * bloom_getBits(bloom_t filter) {
	return filter->bits;
}

size_t bloom_getSize(bloom_t filter) {
	return filter->size;
}
