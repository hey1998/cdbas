#ifndef __HASHTABLE_H_
#define __HASHTABLE_H_

#include <stdint.h>

struct HashTable {
	uint8_t *t;
	int B;
	int N;
};

uint8_t *t(struct HashTable *, uint32_t);
void HashTable(struct HashTable *, int, int);

#endif
