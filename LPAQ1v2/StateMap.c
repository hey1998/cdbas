#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "StateMap.h"

int squash(int d)
{
	static const int t[33] = {
		1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101,
		1546, 2047, 2549, 2994, 3348, 3607, 3785, 3901, 3975, 4022,
		4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094
	};
	if (d > 2047)
		return 4095;
	if (d < -2047)
		return 0;
	int w = d & 127;
	d = (d >> 7) + 16;
	return (t[d] * (128 - w) + t[(d + 1)] * w + 64) >> 7;
}

int stretch_t[4096];
int dt[1024];

static void update(struct StateMap *sm, int y, int limit)
{
	uint32_t *pb = &sm->t[sm->cxt], p0 = pb[0];
	int n = p0 & 1023, pr = p0 >> 10;
	if (n < limit)
		++p0;
	p0 += ((y - pr) >> 3) * dt[n] & 0xfffffc00;
	pb[0] = p0;
}

static int p(struct StateMap *sm, int y, int c, int limit)
{
	update(sm, y, limit);
	return sm->t[sm->cxt = c] >> 20;
}

void StateMap(struct StateMap *sm, int n)
{
	sm->cxt = 0;
	sm->t = malloc(n * sizeof(uint32_t));
	if (sm->t == NULL)
		perror(__FUNCTION__), abort();
	for (int i = 0; i < n; i++)
		sm->t[i] = 1 << 31;
	sm->p = p;
}

static int pp(struct APM *a, int y, int pr, int cx, int limit)
{
	update((struct StateMap *)a, y, limit);
	pr = (stretch_t[pr] + 2048) * 23;
	int wt = pr & 0xfff;
	cx = cx * 24 + (pr >> 12);
	a->cxt = cx + (wt >> 11);
	pr = ((a->t[cx] >> 13) * (0x1000 - wt) + (a->t[cx + 1] >> 13) * wt) >> 19;
	return pr;
}

void APM(struct APM *a, int n)
{
	a->cxt = 0;
	a->t = malloc(n * 24 * sizeof(uint32_t));
	if (a->t == NULL)
		perror(__FUNCTION__), abort();
	for (int i = 0; i < n * 24; i++) {
		int d = ((i % 24 * 2 + 1) * 4096) / 48 - 2048;
		a->t[i] = ((uint32_t) (squash(d)) << 20) + 6;
	}
	a->pp = pp;
}
