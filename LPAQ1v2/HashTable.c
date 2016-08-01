#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HashTable.h"

void HashTable(struct HashTable *ht, int b, int n)
{
	ht->B = b;
	ht->N = n;
	ht->t = calloc(ht->N + ht->B * 4 + 64, sizeof(uint8_t));
	if (ht->t == NULL)
		perror(__FUNCTION__), abort();
	ht->t += 64 - (int)(((long)ht->t) & 63);
}

uint8_t *t(struct HashTable *ht, uint32_t i)
{
	i *= 123456791;
	i = i << 16 | i >> 16;
	i *= 234567891;
	int chk = i >> 24;
	i = i * ht->B & (ht->N - ht->B);
	if (ht->t[i] == chk)
		return ht->t + i;
	if (ht->t[i ^ ht->B] == chk)
		return ht->t + (i ^ ht->B);
	if (ht->t[i ^ ht->B * 2] == chk)
		return ht->t + (i ^ ht->B * 2);
	if (ht->t[i + 1] > ht->t[(i + 1) ^ ht->B]
	    || ht->t[i + 1] > ht->t[(i + 1) ^ ht->B * 2])
		i ^= ht->B;
	if (ht->t[i + 1] > ht->t[(i + 1) ^ ht->B ^ ht->B * 2])
		i ^= ht->B ^ ht->B * 2;
	memset(ht->t + i, 0, ht->B);
	ht->t[i] = chk;
	return ht->t + i;
}
