#ifndef __MATCHMODEL_H_
#define __MATCHMODEL_H_

#include "StateMap.h"

struct MatchModel {
	int *ht;
	int match;
	int len;
	int bcount;
	struct StateMap sm;
	int (*p) (struct MatchModel *, int);
};

void MatchModel(struct MatchModel *, int);

#define m_add(a,b) mxr_tx[a]=stretch_t[b];

extern int c0;
extern uint8_t *buf;
extern uint32_t h1, h2;
extern int pos;
extern int N, HN;

#endif
