#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "MatchModel.h"
#include "StateMap.h"
#include "Mixer.h"

int c0 = 1;
uint8_t *buf;
uint32_t h1, h2;
int pos;
int N, HN;

#define MAXLEN 62

static int p(struct MatchModel *mm, int y)
{
	++mm->bcount;
	if (mm->bcount == 8) {
		mm->bcount = 0;
		if (mm->len > 0) {
			++mm->match;
			mm->match &= N;
			if (mm->len < MAXLEN)
				++mm->len;
		} else {
			mm->match = mm->ht[h1];
			if (mm->match != pos) {
				int i;
				while (mm->len < MAXLEN && (i = (mm->match - mm->len - 1) & N) != pos && buf[i] == buf[(pos - mm->len - 1) & N])
					++mm->len;
			}
		}
		if (mm->len < 2) {
			mm->len = 0;
			mm->match = mm->ht[h2];
			if (mm->match != pos) {
				int i;
				while (mm->len < MAXLEN && (i = (mm->match - mm->len - 1) & N) != pos && buf[i] == buf[(pos - mm->len - 1) & N])
					++mm->len;
			}
		}
		mm->ht[h1] = pos;
		mm->ht[h2] = pos;
	}
	int c = c0;
	if (mm->len > 0 && ((buf[mm->match] + 256) >> (8 - mm->bcount)) == c) {
		int b = buf[mm->match] >> (7 - mm->bcount) & 1;
		if (mm->len < 16)
			c = mm->len * 2 + b;
		else
			c = (mm->len >> 2) * 2 + b + 24;
		c = c * 256 + buf[(pos - 1) & N];
	} else
		mm->len = 0;
	m_add(0, mm->sm.p(&(mm->sm), y, c, 1023));
	return mm->len;
}

void MatchModel(struct MatchModel *mm, int n)
{
	mm->ht = 0;
	mm->match = 0;
	mm->len = 0;
	mm->bcount = 0;
	StateMap(&mm->sm, 56 << 8);
	N = n / 2 - 1;
	HN = n / 8 - 1;
	pos = h1 = h2 = 0;
	buf = calloc(N + 1, sizeof(uint8_t));
	mm->ht = calloc(HN + 1, sizeof(int));
	if (buf == NULL || mm->ht == NULL)
		perror(__FUNCTION__), abort();
	mm->p = p;
}
