#include <stdio.h>
#include <stdlib.h>
#include "Mixer.h"
#include "StateMap.h"

int *mxr_tx;

static void train(int *w, int err, int n)
{
	int *t = mxr_tx;
	for (int i = 0; i < n; ++i)
		w[i] += (t[i] * err + 0x8000) >> 16;
}

static int dot_product(int *w, int n)
{
	int *t = mxr_tx;
	int sum = 0;
	for (int i = 0; i < n; ++i)
		sum += t[i] * w[i];
	return sum;
}

static void update(struct Mixer *m, int y)
{
	int err = ((y << 12) - m->pr) * 7;
	train(&m->wx[m->cxt * m->MI], err, m->MI);
}

static void set(struct Mixer *m, int cx)
{
	m->cxt = cx;
}

static int p(struct Mixer *m)
{
	return m->pr = squash(dot_product(&m->wx[m->cxt * m->MI], m->MI) >> 16);
}

void Mixer(struct Mixer *m, int mc, int mi)
{
	m->MC = mc;
	m->MI = mi;
	m->cxt = 0;
	m->pr = 2048;
	mxr_tx = calloc(m->MI, sizeof(int));
	m->wx = calloc(m->MI * m->MC, sizeof(int));
	if (mxr_tx == NULL || m->wx == NULL)
		perror(__FUNCTION__), abort();
	m->update = update;
	m->set = set;
	m->p = p;
}
