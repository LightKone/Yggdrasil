# Bloom Filter in C

A simple and fast bloom filter in C.

This code is originally by SirCmpwn and can be found at https://gogs.sr.ht/SirCmpwn/bloom with an accompanying walkthrough at http://sircmpwn.github.io/2016/04/12/How-to-write-a-better-bloom-filter-in-C.html

# Original Post

This is in response to
[How to write a bloom filter in C++](http://blog.michaelschmatz.com/2016/04/11/how-to-write-a-bloom-filter-cpp/),
which has good intentions, but is ultimately a less than ideal bloom filter
implementation. I put together a better one in C in a few minutes, and I'll
explain the advantages of it.

The important differences are:

* You bring your own hashing functions
* You can add arbitrary data types, not just bytes
* It uses bits directly instead of relying on the `std::vector<bool>`
    being space effecient

I chose C because (1) I prefer it over C++ and (2) I just think it's a better
choice for implementing low level data types, and C++ is better used in high
level code.

I'm not going to explain the mechanics of a bloom filter or most of the details
of why the code looks this way, since I think the original post did a fine job
of that. I'll just present my alternate implementation:

## Header

```C
#ifndef _BLOOM_H
#define _BLOOM_H
#include <stddef.h>
#include <stdbool.h>

typedef unsigned int (*hash_function)(const void *data);
typedef struct bloom_filter * bloom_t;

/* Creates a new bloom filter with no hash functions and size * 8 bits. */
bloom_t bloom_create(size_t size);
/* Frees a bloom filter. */
void bloom_free(bloom_t filter);
/* Adds a hashing function to the bloom filter. You should add all of the
 * functions you intend to use before you add any items. */
void bloom_add_hash(bloom_t filter, hash_function func);
/* Adds an item to the bloom filter. */
void bloom_add(bloom_t filter, const void *item);
/* Tests if an item is in the bloom filter.
 *
 * Returns false if the item has definitely not been added before. Returns true
 * if the item was probably added before. */
bool bloom_test(bloom_t filter, const void *item);

#endif
```

## Implementation

The implementation of this is pretty straightfoward. First, here's the actual
structs behind the opaque bloom_t type:

```C
struct bloom_hash {
    hash_function func;
    struct bloom_hash *next;
};

struct bloom_filter {
    struct bloom_hash *func;
    void *bits;
    size_t size;
};
```

The hash functions are a linked list, but this isn't important. You can make
that anything you want. Otherwise we have a bit of memory called "bits" and the
size of it. Now, for the easy functions:

```C
bloom_t bloom_create(size_t size) {
    bloom_t res = calloc(1, sizeof(struct bloom_filter));
    res->size = size;
    res->bits = malloc(size);
    return res;
}

void bloom_free(bloom_t filter) {
    if (filter) {
        while (filter->func) {
            struct bloom_hash *h;
            filter->func = h->next;
            free(h);
        }
        free(filter->bits);
        free(filter);
    }
}
```

These should be fairly self explanatory. The first interesting function is here:

```C
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
```

Given a hashing function from the user, this just adds it to our linked list of
hash functions. There's a slightly different code path if we're adding the first
function. The functions so far don't really do anything specific to bloom
filters. The first one that does is this:

```C
void bloom_add(bloom_t filter, const void *item) {
    struct bloom_hash *h = filter->func;
    uint8_t *bits = filter->bits;
    while (h) {
        unsigned int hash = h->func(item);
        hash %= filter->size * 8;
        bits[hash / 8] |= 1 << hash % 8;
        h = h->next;
    }
}
```

This iterates over each of the hash functions the user has provided and computes
the hash of the data for that function (modulo the size of our bloom filter),
then it adds this to the bloom filter with this line:

```C
bits[hash / 8] |= 1 << hash % 8;
```

This just sets the nth bit of the filter where n is the hash. Finally, we have
the test function:

```C
bool bloom_test(bloom_t filter, const void *item) {
    struct bloom_hash *h = filter->func;
    uint8_t *bits = filter->bits;
    while (h) {
        unsigned int hash = h->func(item);
        hash %= filter->size * 8;
        if (!(bits[hash / 8] & 1 << hash % 8)) {
            return false;
        }
        h = h->next;
    }
    return true;
}
```

This function is extremely similar, but instead of setting the nth bit, it
checks the nth bit and returns if it's 0:

```C
if (!(bits[hash / 8] & 1 << hash % 8)) {
```

That's it! You have a bloom filter with arbitrary data types for insert and
user-supplied hash functions. I wrote up some simple test code to demonstrate
this, after googling for a couple of random hash functions:

```C
#include "bloom.h"
#include <stdio.h>

unsigned int djb2(const void *_str) {
    const char *str = _str;
    unsigned int hash = 5381;
    char c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

unsigned int jenkins(const void *_str) {
    const char *key = _str;
    unsigned int hash, i;
    while (*key) {
        hash += *key;
        hash += (hash << 10);
        hash ^= (hash >> 6);
        key++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

int main() {
    bloom_t bloom = bloom_create(8);
    bloom_add_hash(bloom, djb2);
    bloom_add_hash(bloom, jenkins);
    printf("Should be 0: %d\n", bloom_test(bloom, "hello world"));
    bloom_add(bloom, "hello world");
    printf("Should be 1: %d\n", bloom_test(bloom, "hello world"));
    printf("Should (probably) be 0: %d\n", bloom_test(bloom, "world hello"));
    return 0;
}
```

The full code is available [here](https://gogs.sr.ht/SirCmpwn/bloom).
