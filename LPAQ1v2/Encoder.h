#ifndef __ENCODER_H_
#define __ENCODER_H_

#include <stdint.h>
#include <stdio.h>

typedef enum { COMPRESS, DECOMPRESS } Mode;
struct Encoder {
	int pr;
	Mode mode;
	FILE *archive;
	uint32_t x1, x2;
	uint32_t x;
	void (*compress) (struct Encoder *, int);
	void (*flush) (struct Encoder *);
	int (*decompress) (struct Encoder *);
};

void Encoder(struct Encoder *, Mode, FILE *);

extern int MEM;

#endif
