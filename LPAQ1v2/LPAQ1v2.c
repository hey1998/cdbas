#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "Encoder.h"

int main(int argc, char **argv)
{
	clock_t start = clock();
	if (argc != 4 || (!isdigit(argv[1][0]) && argv[1][0] != 'd')) {
		fprintf(stdout, "LPAQ1v2 single file compressor\n"
			"\tTo compress:   lpaq1 N input output  (N=0..9, uses 3+3*2^N MB)\n"
			"\tTo decompress: lpaq1 d input output  (needs same memory)\n");
		exit(EXIT_FAILURE);
	}
	FILE *in, *out;
	if (!(in = fopen(argv[2], "rb")) || !(out = fopen(argv[3], "wb")))
		fprintf(stderr, "Failed open file\n"), abort();
	fseek(in, 0, SEEK_END);
	long size = ftell(in);
	if (size < 0 || size >= 0x7FFFFFFF)
		fprintf(stderr, "input file too big\n"), abort();
	fseek(in, 0, SEEK_SET);
	struct Encoder e;
	if (isdigit(argv[1][0])) {
		MEM = 1 << (argv[1][0] - '0' + 20);
		Encoder(&e, COMPRESS, out);
		fprintf(out, "pQ%c%c%c%c%c%c", 1, argv[1][0], size >> 24, size >> 16, size >> 8, size);
		int c;
		while ((c = getc(in)) != EOF)
			e.compress(&e, c);
		e.flush(&e);
	} else {
		if (getc(in) != 'p' || getc(in) != 'Q' || getc(in) != 1)
			fprintf(stderr, "Not a compress file\n"), abort();
		MEM = getc(in);
		if (MEM < '0' || MEM > '9')
			fprintf(stderr, "Bad memory option (not 0..9)\n"), abort();
		MEM = 1 << (MEM - '0' + 20);
		size = getc(in) << 24;
		size |= getc(in) << 16;
		size |= getc(in) << 8;
		size |= getc(in);
		if (size < 0)
			fprintf(stderr, "Bad file size\n"), abort();
		Encoder(&e, DECOMPRESS, in);
		while (size-- > 0)
			putc(e.decompress(&e), out);
	}
	fprintf(stdout, "%ld -> %ld in %.3f sec.\n", ftell(in), ftell(out), (double)(clock() - start) / CLOCKS_PER_SEC);
	exit(EXIT_SUCCESS);
}
