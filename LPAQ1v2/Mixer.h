#ifndef __MIXER_H_
#define __MIXER_H_

struct Mixer {
	int MI;
	int MC;
	int *wx;
	int cxt;
	int pr;
	void (*update) (struct Mixer *, int);
	void (*set) (struct Mixer *, int);
	int (*p) (struct Mixer *);
};

void Mixer(struct Mixer *, int, int);

extern int *mxr_tx;

#endif
