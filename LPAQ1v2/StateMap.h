#ifndef __STATEMAP_H_
#define __STATEMAP_H_

#include <stdint.h>

struct StateMap {
	int cxt;
	uint32_t *t;
	int (*p) (struct StateMap *, int, int, int);
};

struct APM {
	int cxt;
	uint32_t *t;
	int (*pp) (struct APM *, int, int, int, int);
};

void StateMap(struct StateMap *, int);
void APM(struct APM *, int);

int squash(int);
extern int stretch_t[];
extern int dt[];

#endif
