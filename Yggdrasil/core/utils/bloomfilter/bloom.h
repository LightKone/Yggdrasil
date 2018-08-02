/***************************************************
 * This code was based on:
 * http://sircmpwn.github.io/2016/04/12/How-to-write-a-better-bloom-filter-in-C.html
 ***************************************************/
#ifndef _BLOOM_H
#define _BLOOM_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

typedef unsigned int (*hash_function)(const void *data, unsigned int len);
typedef struct bloom_filter * bloom_t;

/* Creates a new bloom filter with no hash functions and size * 8 bits. */
bloom_t bloom_create(size_t size);
/* Frees a bloom filter. */
void bloom_free(bloom_t filter);
/* Adds a hashing function to the bloom filter. You should add all of the
 * functions you intend to use before you add any items. */
void bloom_add_hash(bloom_t filter, hash_function func);
/* Adds an item to the bloom filter. */
void bloom_add(bloom_t filter, const void *item, unsigned int item_len);
/* Tests if an item is in the bloom filter.
 *
 * Returns false (0) if the item has definitely not been added before. Returns true (1)
 * if the item was probably added before. */
int bloom_test(bloom_t filter, const void *item, unsigned int item_len);

/* Tests if the set is not in the bloom filter.
 *
 * Returns false (0) if the set has definitely an item in the bloom filter. Returns true (1)
 * if no item of the set is in the bloom filter
 */
int bloom_test_disjoin(bloom_t filter, const void* set, size_t size);

/* Tests if the set is equal to the bloom filter
 *
 * Returns false(0) if the set is different. Returns true (1) if they are equal
 */
int bloom_test_equal(bloom_t filter, const void* set, size_t size);

 /*	merges the set to the bloom filter
  *
  * Returns 0 if failed or 1 if succeeded
  */
int bloom_merge(bloom_t filter, const void* set, size_t size);

/* swaps the bloom filter bits with the given set
 *
 * Returns 0 if failed or 1 if succeeded
 */
int bloom_swap(bloom_t filter, const void* set, size_t size);

void * bloom_getBits(bloom_t filter);

size_t bloom_getSize(bloom_t filter);

#endif
