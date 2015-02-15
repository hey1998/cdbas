/*************************************************************************
// NOTE: this is an example program, so do not use to backup your data.
//
// This program lacks things like sophisticated file handling but is
// pretty complete regarding compression - it should provide a good
// starting point for adaption for you applications.
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ucl.h>

#define xfread(f,b,s)       (fread(b,1,s,f))
#define xfwrite(f,b,s)      (fwrite(b,1,s,f))

static unsigned long total_in = 0;
static unsigned long total_out = 0;

/* magic file header for compressed files */
static const unsigned char magic[8] =
    { 0x00, 0xe9, 0x55, 0x43, 0x4c, 0xff, 0x01, 0x1a };


/*************************************************************************
// file IO
**************************************************************************/

static unsigned int
xread(FILE *f, void *buf, unsigned int len, int allow_eof)
{
    unsigned int l;

    l = (unsigned int) xfread(f,buf,len);
    if (l != len && !allow_eof || l > len)
    {
        fprintf(stderr,"read error - premature end of file\n");
        exit(1);
    }
    total_in += l;
    return l;
}

static unsigned int
xwrite(FILE *f, const void *buf, unsigned int len)
{
    unsigned int l;

    if (f != NULL)
    {
        l = (unsigned int) xfwrite(f,buf,len);
        if (l != len)
        {
            fprintf(stderr,"write error [%lu %lu]  (disk full ?)\n",
                   (unsigned long)len, (unsigned long)l);
            exit(1);
        }
    }
    total_out += len;
    return len;
}


static int xgetc(FILE *f)
{
    unsigned char c;
    xread(f,(void*) &c,1,0);
    return c;
}

static void xputc(FILE *f, int c)
{
    unsigned char cc = (unsigned char) c;
    xwrite(f,(const void*) &cc,1);
}

/* read and write portable 32-bit integers */

static uint32_t xread32(FILE *f)
{
    unsigned char b[4];
    uint32_t v;

    xread(f,b,4,0);
    v  = (uint32_t) b[3] <<  0;
    v |= (uint32_t) b[2] <<  8;
    v |= (uint32_t) b[1] << 16;
    v |= (uint32_t) b[0] << 24;
    return v;
}

static void xwrite32(FILE *f, uint32_t v)
{
    unsigned char b[4];

    b[3] = (unsigned char) (v >>  0);
    b[2] = (unsigned char) (v >>  8);
    b[1] = (unsigned char) (v >> 16);
    b[0] = (unsigned char) (v >> 24);
    xwrite(f,b,4);
}


/*************************************************************************
// util
**************************************************************************/

#define UCL_BASE 65521u /* largest prime smaller than 65536 */
#define UCL_NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define UCL_DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define UCL_DO2(buf,i)  UCL_DO1(buf,i); UCL_DO1(buf,i+1);
#define UCL_DO4(buf,i)  UCL_DO2(buf,i); UCL_DO2(buf,i+2);
#define UCL_DO8(buf,i)  UCL_DO4(buf,i); UCL_DO4(buf,i+4);
#define UCL_DO16(buf,i) UCL_DO8(buf,i); UCL_DO8(buf,i+8);

static uint32_t
xadler32(uint32_t adler, const unsigned char* buf, unsigned int len)
{
    uint32_t s1 = adler & 0xffff;
    uint32_t s2 = (adler >> 16) & 0xffff;
    int k;

    if (buf == NULL)
        return 1;

    while (len > 0)
    {
        k = len < UCL_NMAX ? (int) len : UCL_NMAX;
        len -= k;
        if (k >= 16) do
        {
            UCL_DO16(buf,0);
            buf += 16;
            k -= 16;
        } while (k >= 16);
        if (k != 0) do
        {
            s1 += *buf++;
            s2 += s1;
        } while (--k > 0);
        s1 %= UCL_BASE;
        s2 %= UCL_BASE;
    }
    return (s2 << 16) | s1;
}


/*************************************************************************
// compress
**************************************************************************/

static int
do_compress(FILE *fi, FILE *fo, int level, unsigned int block_size)
{
    int r = 0;
    unsigned char* in = NULL;
    unsigned char* out = NULL;
    unsigned int in_len;
    unsigned int out_len;
    int method = 0x2b;
    uint32_t flags = 1;
    uint32_t checksum;

    total_in = total_out = 0;

/*
 * Step 1: write magic header, flags & block size, init checksum
 */
    xwrite(fo,magic,sizeof(magic));
    xwrite32(fo,flags);
    xputc(fo,method);           /* compression method */
    xputc(fo,level);            /* compression level */
    xwrite32(fo,block_size);
    checksum = xadler32(0,NULL,0);

/*
 * Step 2: allocate compression buffers and work-memory
 */
    in = (unsigned char*) malloc(block_size);
    out = (unsigned char*) malloc(block_size + block_size / 8 + 256);
    if (in == NULL || out == NULL)
    {
        printf("uclpack: out of memory\n");
        r = 1;
        goto err;
    }

/*
 * Step 3: process blocks
 */
    for (;;)
    {
        /* read block */
        in_len = xread(fi,in,block_size,1);
        if (in_len <= 0)
            break;

        /* update checksum */
        checksum = xadler32(checksum,in,in_len);

        /* compress block */
        r = UCL_E_ERROR;
        out_len = 0;
        r = ucl_nrv2b_99_compress(in,in_len,out,&out_len,0,level,NULL,NULL);
        if (r == UCL_E_OUT_OF_MEMORY)
        {
            printf("out of memory in compress\n");
            r = 1;
            goto err;
        }
        if (r != UCL_E_OK || out_len > in_len + in_len / 8 + 256)
        {
            /* this should NEVER happen */
            printf("internal error - compression failed: %d\n", r);
            r = 2;
            goto err;
        }

        /* write uncompressed block size */
        xwrite32(fo,in_len);

        if (out_len < in_len)
        {
            /* write compressed block */
            xwrite32(fo,out_len);
            xwrite(fo,out,out_len);
        }
        else
        {
            /* not compressible - write uncompressed block */
            xwrite32(fo,in_len);
            xwrite(fo,in,in_len);
        }
    }

    /* write EOF marker */
    xwrite32(fo,0);

    /* write checksum */
    xwrite32(fo,checksum);

    r = 0;
err:
    free(out);
    free(in);
    return r;
}


/*************************************************************************
// decompress
**************************************************************************/

static int
do_decompress(FILE *fi, FILE *fo)
{
    int r = 0;
    unsigned char* buf = NULL;
    unsigned int buf_len;
    unsigned char m [ sizeof(magic) ];
    uint32_t flags;
    int method;
    int level;
    unsigned int block_size;
    uint32_t checksum;

    total_in = total_out = 0;

/*
 * Step 1: check magic header, read flags & block size, init checksum
 */
    if (xread(fi,m,sizeof(magic),1) != sizeof(magic) ||
        memcmp(m,magic,sizeof(magic)) != 0)
    {
        printf("uclpack: header error - this file is not compressed by uclpack\n");
        r = 1;
        goto err;
    }
    flags = xread32(fi);
    method = xgetc(fi);
    level = xgetc(fi);
    block_size = xread32(fi);
    if (block_size < 1024 || block_size > 8*1024*1024)
    {
        printf("uclpack: header error - invalid block size %ld\n",
                (long) block_size);
        r = 3;
        goto err;
    }
    printf("uclpack: block-size is %ld bytes\n", (long)block_size);

    checksum = xadler32(0,NULL,0);

/*
 * Step 2: allocate buffer for in-place decompression
 */
    buf_len = block_size + block_size / 8 + 256;
    buf = (unsigned char*) malloc(buf_len);
    if (buf == NULL)
    {
        printf("uclpack: out of memory\n");
        r = 4;
        goto err;
    }

/*
 * Step 3: process blocks
 */
    for (;;)
    {
        unsigned char* in;
        unsigned char* out;
        unsigned int in_len;
        unsigned int out_len;

        /* read uncompressed size */
        out_len = xread32(fi);

        /* exit if last block (EOF marker) */
        if (out_len == 0)
            break;

        /* read compressed size */
        in_len = xread32(fi);

        /* sanity check of the size values */
        if (in_len > block_size || out_len > block_size ||
            in_len == 0 || in_len > out_len)
        {
            printf("uclpack: block size error - data corrupted\n");
            r = 5;
            goto err;
        }

        /* place compressed block at the top of the buffer */
        in = buf + buf_len - in_len;
        out = buf;

        /* read compressed block data */
        xread(fi,in,in_len,0);

        if (in_len < out_len)
        {
            /* decompress - use safe decompressor as data might be corrupted */
            unsigned int new_len = out_len;

            if (method == 0x2b)
                r = ucl_nrv2b_decompress_8(in,in_len,out,&new_len,NULL);
            if (r != UCL_E_OK || new_len != out_len)
            {
                printf("uclpack: compressed data violation: error %d (0x%x: %ld/%ld/%ld)\n", r, method, (long) in_len, (long) out_len, (long) new_len);
                r = 6;
                goto err;
            }
            /* write decompressed block */
            xwrite(fo,out,out_len);
            /* update checksum */
            checksum = xadler32(checksum,out,out_len);
        }
        else
        {
            /* write original (incompressible) block */
            xwrite(fo,in,in_len);
            /* update checksum */
            checksum = xadler32(checksum,in,in_len);
        }
    }

    /* read and verify checksum */
    if (checksum != xread32(fi))
    {
        printf("uclpack: checksum error - data corrupted\n");
        r = 7;
        goto err;
    }

    r = 0;
err:
    free(buf);
    return r;
}


/*************************************************************************
// misc support functions
**************************************************************************/

static void usage(void)
{
    printf("usage:\n");
    printf("  uclpack [options] input-file output-file      (compress)\n");
    printf("  uclpack -d compressed-file output-file        (decompress)\n");
    printf("\nother options:\n");
    printf("  -1...-9, --10   set compression level [default is `-7']\n");
    printf("  -bxxxx          set block-size for compression [default 262144]\n");
    exit(1);
}


/* open input file */
static FILE *xopen_fi(const char *name)
{
    FILE *f;

    f = fopen(name,"rb");
    if (f == NULL)
    {
        printf("cannot open input file %s\n", name);
        exit(1);
    }
#if defined(HAVE_STAT) && defined(S_ISREG)
    {
        struct stat st;
#if defined(HAVE_LSTAT)
        if (lstat(name,&st) != 0 || !S_ISREG(st.st_mode))
#else
        if (stat(name,&st) != 0 || !S_ISREG(st.st_mode))
#endif
        {
            printf("%s is not a regular file\n", name);
            fclose(f);
            exit(1);
        }
    }
#endif
    return f;
}


/* open output file */
static FILE *xopen_fo(const char *name)
{
    FILE *f;

    /* this is an example program, so make sure we don't overwrite a file */
    f = fopen(name,"rb");
    if (f != NULL)
    {
        printf("file %s already exists -- not overwritten\n", name);
        fclose(f);
        exit(1);
    }
    f = fopen(name,"wb");
    if (f == NULL)
    {
        printf("cannot open output file %s\n", name);
        exit(1);
    }
    return f;
}


/*************************************************************************
// main
**************************************************************************/

int main(int argc, char *argv[])
{
    int i = 1;
    int r = 0;
    FILE *fi = NULL;
    FILE *fo = NULL;
    const char *in_name = NULL;
    const char *out_name = NULL;
    int opt_decompress = 0;
    int opt_level = 7;
    unsigned int opt_block_size;

    printf("UCL data compression library.\n");
    printf("*** WARNING ***\n" "This is an example program, do not use to backup your data!\n");

    opt_block_size = 256 * 1024;

    while (i < argc && argv[i][0] == '-')
    {
        if (strcmp(argv[i],"-d") == 0)
            opt_decompress = 1;
        else if ((argv[i][1] >= '1' && argv[i][1] <= '9') && !argv[i][2])
            opt_level = argv[i][1] - '0';
        else if (strcmp(argv[i],"--10") == 0)
            opt_level = 10;
        else if (argv[i][1] == 'b' && argv[i][2])
        {
            int x = atoi(&argv[i][2]);
            if (x >= 1024 && x <= 8*1024*1024)
                opt_block_size = (unsigned int) x;
            else {
                printf("error: invalid block-size %d\n", x);
                exit(1);
            }
        }
        else
            usage();
        i++;
    }
    if (i + 2 != argc)
        usage();

    if (opt_decompress)
    {
        in_name = argv[i++];
        out_name = argv[i++];
        fi = xopen_fi(in_name);
        fo = xopen_fo(out_name);
        r = do_decompress(fi, fo);
        if (r == 0)
            printf("uclpack: decompressed %lu into %lu bytes\n",
                    total_in, total_out);
    }
    else /* compress */
    {
        printf("uclpack: using block-size of %lu bytes\n", (unsigned long)opt_block_size);
        in_name = argv[i++];
        out_name = argv[i++];
        fi = xopen_fi(in_name);
        fo = xopen_fo(out_name);
        r = do_compress(fi,fo,opt_level,opt_block_size);
        if (r == 0)
        {
            printf("uclpack: compressed %lu into %lu bytes\n",
                    total_in, total_out);
        }
    }
    if (fi) fclose(fi);
    if (fo) fclose(fo);
    return r;
}

