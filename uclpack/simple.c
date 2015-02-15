#include <stdio.h>
#include <stdlib.h>
#include <ucl.h>

/*************************************************************************
// This program shows the basic usage of the UCL library.
// We will compress a block of data and decompress again.
**************************************************************************/

int main(int argc, char *argv[])
{
    int r;
    unsigned char* in;
    unsigned char* out;
    unsigned int in_len;
    unsigned int out_len;
    unsigned int new_len;
    int level = 5;                  /* compression level (1-10) */

    if (argc < 0 && argv == NULL)   /* avoid warning about unused args */
        return 0;

    printf("UCL data compression library.\n");


/*
 * Step 2: setup memory
 *
 * We want to compress the data block at `in' with length `in_len' to
 * the block at `out'. Because the input block may be incompressible,
 * we must provide a little more output space in case that compression
 * is not possible.
 */
    in_len = 256 * 1024;

    out_len = in_len + in_len / 8 + 256;

    in = (unsigned char*) malloc(in_len);
    out = (unsigned char*) malloc(out_len);
    if (in == NULL || out == NULL)
    {
        printf("out of memory\n");
        return 2;
    }


/*
 * Step 3: prepare the input block that will get compressed.
 *         We just fill it with zeros in this example program,
 *         but you would use your real-world data here.
 */
    memset(in, 0, in_len);


/*
 * Step 4: compress from `in' to `out' with UCL NRV2B
 */
    r = ucl_nrv2b_99_compress(in,in_len,out,&out_len,NULL,level,NULL,NULL);
    if (r == UCL_E_OUT_OF_MEMORY)
    {
        printf("out of memory in compress\n");
        return 3;
    }
    if (r == UCL_E_OK)
        printf("compressed %lu bytes into %lu bytes\n",
            (unsigned long) in_len, (unsigned long) out_len);
    else
    {
        /* this should NEVER happen */
        printf("internal error - compression failed: %d\n", r);
        return 4;
    }
    /* check for an incompressible block */
    if (out_len >= in_len)
    {
        printf("This block contains incompressible data.\n");
        return 0;
    }


/*
 * Step 5: decompress again, now going back from `out' to `in'
 */
    new_len = in_len;
    r = ucl_nrv2b_decompress_8(out,out_len,in,&new_len,NULL);
    if (r == UCL_E_OK && new_len == in_len)
        printf("decompressed %lu bytes back into %lu bytes\n",
            (unsigned long) out_len, (unsigned long) in_len);
    else
    {
        /* this should NEVER happen */
        printf("internal error - decompression failed: %d\n", r);
        return 5;
    }

    free(out);
    free(in);
    printf("Simple compression test passed.\n");
    return 0;
}

