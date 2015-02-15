#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ucl.h>

/***********************************************************************
//
************************************************************************/

#define M2_MAX_OFFSET       0xd00

#define UCL_MAX(a,b)        ((a) >= (b) ? (a) : (b))
#define UCL_MIN(a,b)        ((a) <= (b) ? (a) : (b))

#define SWD_USE_MALLOC 1

#define SWD_HMASK       (UCL_UINT32_C(65535))
#define SWD_HSIZE       (SWD_HMASK + 1)

#define SWD_N           (8*1024*1024ul) /* max. size of ring buffer */
#define SWD_F           2048            /* upper limit for match length */
#define SWD_THRESHOLD   1               /* lower limit for match length */

#ifndef SWD_MAX_CHAIN
#define SWD_MAX_CHAIN   2048
#endif

typedef struct
{
/* public - "built-in" */
    unsigned int n;
    unsigned int f;

/* public - configuration */
    unsigned int max_chain;
    unsigned int nice_length;
    unsigned int lazy_insert;

/* public - output */
    unsigned int m_len;
    unsigned int m_off;
    unsigned int look;
    int b_char;

/* semi public */
    UCL_COMPRESS_T *c;
    unsigned int m_pos;

/* private */
    const unsigned char* dict;
    const unsigned char* dict_end;
    unsigned int dict_len;

/* private */
    unsigned int ip;                /* input pointer (lookahead) */
    unsigned int bp;                /* buffer pointer */
    unsigned int rp;                /* remove pointer */
    unsigned int b_size;

    unsigned char* b_wrap;

    unsigned int node_count;
    unsigned int first_rp;

#if defined(SWD_USE_MALLOC)
    unsigned char* b;
    unsigned int* head3;
    unsigned int* succ3;
    unsigned int* best3;
    unsigned int* llen3;
#else
    unsigned char b [ SWD_N + SWD_F + SWD_F ];
    unsigned int head3 [ SWD_HSIZE ];
    unsigned int succ3 [ SWD_N + SWD_F ];
    unsigned int best3 [ SWD_N + SWD_F ];
    unsigned int llen3 [ SWD_HSIZE ];
#endif
}
ucl_swd_t;


#if (UINT_MAX < 0xffffffffL)
#  error "UINT_MAX"
#endif


/***********************************************************************
//
************************************************************************/

#define getbyte(c)  ((c).ip < (c).in_end ? *((c).ip)++ : (-1))
#define HEAD3(b,p) \
    (((0x9f5f*(((((uint32_t)b[p]<<5)^b[p+1])<<5)^b[p+2]))>>5) & SWD_HMASK)
#define s_get_head3(s,key)    s->head3[key]


/***********************************************************************
//
************************************************************************/

static void
swd_initdict(ucl_swd_t *s, const unsigned char* dict, unsigned int dict_len)
{
    s->dict = s->dict_end = NULL;
    s->dict_len = 0;

    if (!dict || dict_len <= 0)
        return;
    if (dict_len > s->n)
    {
        dict += dict_len - s->n;
        dict_len = s->n;
    }

    s->dict = dict;
    s->dict_len = dict_len;
    s->dict_end = dict + dict_len;
    memcpy(s->b,dict,dict_len);
    s->ip = dict_len;
}


static void
swd_insertdict(ucl_swd_t *s, unsigned int node, unsigned int len)
{
    unsigned int key;

    s->node_count = s->n - len;
    s->first_rp = node;

    while (len-- > 0)
    {
        key = HEAD3(s->b,node);
        s->succ3[node] = s_get_head3(s,key);
        s->head3[key] = (unsigned int)(node);
        s->best3[node] = (unsigned int)(s->f + 1);
        s->llen3[key]++;
        assert(s->llen3[key] <= s->n);

        node++;
    }
}


/***********************************************************************
//
************************************************************************/

#define alloc(a,b)  (malloc((a) * (b)))

static int
swd_init(ucl_swd_t *s, const unsigned char* dict, unsigned int dict_len)
{
#if defined(SWD_USE_MALLOC)
    s->b = NULL;
    s->head3 = NULL;
    s->succ3 = NULL;
    s->best3 = NULL;
    s->llen3 = NULL;
#endif

    if (s->n == 0)
        s->n = SWD_N;
    if (s->f == 0)
        s->f = SWD_F;
    if (s->n > SWD_N || s->f > SWD_F)
        return UCL_E_INVALID_ARGUMENT;

#if defined(SWD_USE_MALLOC)
    s->b = (unsigned char*) malloc(s->n + s->f + s->f);
    s->head3 = (unsigned int*) alloc(SWD_HSIZE, sizeof(*s->head3));
    s->succ3 = (unsigned int*) alloc(s->n + s->f, sizeof(*s->succ3));
    s->best3 = (unsigned int*) alloc(s->n + s->f, sizeof(*s->best3));
    s->llen3 = (unsigned int*) alloc(SWD_HSIZE, sizeof(*s->llen3));
    if (!s->b || !s->head3  || !s->succ3 || !s->best3 || !s->llen3)
        return UCL_E_OUT_OF_MEMORY;
#endif

    /* defaults */
    s->max_chain = SWD_MAX_CHAIN;
    s->nice_length = s->f;
    s->lazy_insert = 0;

    s->b_size = s->n + s->f;
    if (s->b_size + s->f >= UINT_MAX)
        return UCL_E_ERROR;
    s->b_wrap = s->b + s->b_size;
    s->node_count = s->n;

    memset(s->llen3, 0, (unsigned int)sizeof(s->llen3[0]) * SWD_HSIZE);

    s->ip = 0;
    swd_initdict(s,dict,dict_len);
    s->bp = s->ip;
    s->first_rp = s->ip;

    assert(s->ip + s->f <= s->b_size);
    s->look = (unsigned int) (s->c->in_end - s->c->ip);
    if (s->look > 0)
    {
        if (s->look > s->f)
            s->look = s->f;
        memcpy(&s->b[s->ip],s->c->ip,s->look);
        s->c->ip += s->look;
        s->ip += s->look;
    }
    if (s->ip == s->b_size)
        s->ip = 0;

    if (s->look >= 2 && s->dict_len > 0)
        swd_insertdict(s,0,s->dict_len);

    s->rp = s->first_rp;
    if (s->rp >= s->node_count)
        s->rp -= s->node_count;
    else
        s->rp += s->b_size - s->node_count;

    return UCL_E_OK;
}


static void
swd_exit(ucl_swd_t *s)
{
#if defined(SWD_USE_MALLOC)
    /* free in reverse order of allocations */
    free(s->llen3); s->llen3 = NULL;
    free(s->best3); s->best3 = NULL;
    free(s->succ3); s->succ3 = NULL;
    free(s->head3); s->head3 = NULL;
    free(s->b); s->b = NULL;
#endif
}


#define swd_pos2off(s,pos) \
    (s->bp > (pos) ? s->bp - (pos) : s->b_size - ((pos) - s->bp))


/***********************************************************************
//
************************************************************************/

static void
swd_getbyte(ucl_swd_t *s)
{
    int c;

    if ((c = getbyte(*(s->c))) < 0)
    {
        if (s->look > 0)
            --s->look;
    }
    else
    {
        s->b[s->ip] = (unsigned char)(c);
        if (s->ip < s->f)
            s->b_wrap[s->ip] = (unsigned char)(c);
    }
    if (++s->ip == s->b_size)
        s->ip = 0;
    if (++s->bp == s->b_size)
        s->bp = 0;
    if (++s->rp == s->b_size)
        s->rp = 0;
}


/***********************************************************************
// remove node from lists
************************************************************************/

static void
swd_remove_node(ucl_swd_t *s, unsigned int node)
{
    if (s->node_count == 0)
    {
        unsigned int key;

        key = HEAD3(s->b,node);
        assert(s->llen3[key] > 0);
        --s->llen3[key];
    }
    else
        --s->node_count;
}


/***********************************************************************
//
************************************************************************/

static void
swd_accept(ucl_swd_t *s, unsigned int n)
{
    assert(n <= s->look);

    if (n > 0) do
    {
        unsigned int key;

        swd_remove_node(s,s->rp);

        /* add bp into HEAD3 */
        key = HEAD3(s->b,s->bp);
        s->succ3[s->bp] = s_get_head3(s,key);
        s->head3[key] = (unsigned int)(s->bp);
        s->best3[s->bp] = (unsigned int)(s->f + 1);
        s->llen3[key]++;
        assert(s->llen3[key] <= s->n);

        swd_getbyte(s);
    } while (--n > 0);
}


/***********************************************************************
//
************************************************************************/

static void
swd_search(ucl_swd_t *s, unsigned int node, unsigned int cnt)
{
    const unsigned char* p1;
    const unsigned char* p2;
    const unsigned char* px;
    unsigned int m_len = s->m_len;
    const unsigned char* b  = s->b;
    const unsigned char* bp = s->b + s->bp;
    const unsigned char* bx = s->b + s->bp + s->look;
    unsigned char scan_end1;

    assert(s->m_len > 0);

    scan_end1 = bp[m_len - 1];
    for ( ; cnt-- > 0; node = s->succ3[node])
    {
        p1 = bp;
        p2 = b + node;
        px = bx;

        assert(m_len < s->look);

        if (
#if 1
            p2[m_len - 1] == scan_end1 &&
            p2[m_len] == p1[m_len] &&
#endif
            p2[0] == p1[0] &&
            p2[1] == p1[1])
        {
            unsigned int i;
            assert(memcmp(bp,&b[node],3) == 0);

            p1 += 2; p2 += 2;
            do {} while (++p1 < px && *p1 == *++p2);
            i = (unsigned int) (p1 - bp);

            if (i > m_len)
            {
                s->m_len = m_len = i;
                s->m_pos = node;
                if (m_len == s->look)
                    return;
                if (m_len >= s->nice_length)
                    return;
                if (m_len > (unsigned int) s->best3[node])
                    return;
                scan_end1 = bp[m_len - 1];
            }
        }
    }
}


/***********************************************************************
//
************************************************************************/

static void
swd_findbest(ucl_swd_t *s)
{
    unsigned int key;
    unsigned int cnt, node;
    unsigned int len;

    assert(s->m_len > 0);

    /* get current head, add bp into HEAD3 */
    key = HEAD3(s->b,s->bp);
    node = s->succ3[s->bp] = s_get_head3(s,key);
    cnt = s->llen3[key]++;
    assert(s->llen3[key] <= s->n + s->f);
    if (cnt > s->max_chain && s->max_chain > 0)
         cnt = s->max_chain;
    s->head3[key] = (unsigned int)(s->bp);

    s->b_char = s->b[s->bp];
    len = s->m_len;
    if (s->m_len >= s->look)
    {
        if (s->look == 0)
            s->b_char = -1;
        s->m_off = 0;
        s->best3[s->bp] = (unsigned int)(s->f + 1);
    }
    else
    {
        if (s->look >= 3)
            swd_search(s,node,cnt);
        if (s->m_len > len)
            s->m_off = swd_pos2off(s,s->m_pos);
        s->best3[s->bp] = (unsigned int) (s->m_len);

    }

    swd_remove_node(s,s->rp);
}


/***********************************************************************
//
************************************************************************/

static int
init_match ( UCL_COMPRESS_T *c, ucl_swd_t *s,
             const unsigned char* dict, unsigned int dict_len,
             uint32_t flags )
{
    int r;

    assert(!c->init);
    c->init = 1;

    s->c = c;

    c->last_m_len = c->last_m_off = 0;

    c->textsize = c->codesize = c->printcount = 0;
    c->lit_bytes = c->match_bytes = c->rep_bytes = 0;
    c->lazy = 0;

    r = swd_init(s,dict,dict_len);
    if (r != UCL_E_OK)
    {
        swd_exit(s);
        return r;
    }

    return UCL_E_OK;
}


/***********************************************************************
//
************************************************************************/

static int
find_match ( UCL_COMPRESS_T *c, ucl_swd_t *s,
             unsigned int this_len, unsigned int skip )
{
    assert(c->init);

    if (skip > 0)
    {
        assert(this_len >= skip);
        swd_accept(s, this_len - skip);
        c->textsize += this_len - skip + 1;
    }
    else
    {
        assert(this_len <= 1);
        c->textsize += this_len - skip;
    }

    s->m_len = SWD_THRESHOLD;
    swd_findbest(s);
    c->m_len = s->m_len;
    c->m_off = s->m_off;

    swd_getbyte(s);

    if (s->b_char < 0)
    {
        c->look = 0;
        c->m_len = 0;
        swd_exit(s);
    }
    else
    {
        c->look = s->look + 1;
    }
    c->bp = c->ip - c->look;

    if (c->cb && c->textsize > c->printcount)
    {
        (*c->cb->callback)(c->textsize,c->codesize,3,c->cb->user);
        c->printcount += 1024;
    }

    return UCL_E_OK;
}


/***********************************************************************
// bit buffer
************************************************************************/

static int bbConfig(UCL_COMPRESS_T *c, int bitsize)
{
    if (bitsize != -1)
    {
        if (bitsize != 8 && bitsize != 16 && bitsize != 32)
            return UCL_E_ERROR;
        c->bb_c_s = bitsize;
        c->bb_c_s8 = bitsize / 8;
    }
    c->bb_b = 0; c->bb_k = 0;
    c->bb_p = NULL;
    c->bb_op = NULL;
    return UCL_E_OK;
}


static void bbWriteBits(UCL_COMPRESS_T *c)
{
    unsigned char* p = c->bb_p;
    uint32_t b = c->bb_b;

    p[0] = (unsigned char)(b >>  0);
    if (c->bb_c_s >= 16)
    {
        p[1] = (unsigned char)(b >>  8);
        if (c->bb_c_s == 32)
        {
            p[2] = (unsigned char)(b >> 16);
            p[3] = (unsigned char)(b >> 24);
        }
    }
}


static void bbPutBit(UCL_COMPRESS_T *c, unsigned bit)
{
    assert(bit == 0 || bit == 1);
    assert(c->bb_k <= c->bb_c_s);

    if (c->bb_k < c->bb_c_s)
    {
        if (c->bb_k == 0)
        {
            assert(c->bb_p == NULL);
            c->bb_p = c->bb_op;
            c->bb_op += c->bb_c_s8;
        }
        assert(c->bb_p != NULL);
        assert(c->bb_p + c->bb_c_s8 <= c->bb_op);

        c->bb_b = (c->bb_b << 1) + bit;
        c->bb_k++;
    }
    else
    {
        assert(c->bb_p != NULL);
        assert(c->bb_p + c->bb_c_s8 <= c->bb_op);

        bbWriteBits(c);
        c->bb_p = c->bb_op;
        c->bb_op += c->bb_c_s8;
        c->bb_b = bit;
        c->bb_k = 1;
    }
}


static void bbPutByte(UCL_COMPRESS_T *c, unsigned b)
{
    /**printf("putbyte %p %p %x  (%d)\n", op, bb_p, x, bb_k);*/
    assert(c->bb_p == NULL || c->bb_p + c->bb_c_s8 <= c->bb_op);
    *c->bb_op++ = (unsigned char)(b);
}


static void bbFlushBits(UCL_COMPRESS_T *c, unsigned filler_bit)
{
    if (c->bb_k > 0)
    {
        assert(c->bb_k <= c->bb_c_s);
        while (c->bb_k != c->bb_c_s)
            bbPutBit(c, filler_bit);
        bbWriteBits(c);
        c->bb_k = 0;
    }
    c->bb_p = NULL;
}

/***********************************************************************
// start-step-stop prefix coding
************************************************************************/

static void code_prefix_ss11(UCL_COMPRESS_T *c, uint32_t i)
{
    if (i >= 2)
    {
        uint32_t t = 4;
        i += 2;
        do {
            t <<= 1;
        } while (i >= t);
        t >>= 1;
        do {
            t >>= 1;
            bbPutBit(c, (i & t) ? 1 : 0);
            bbPutBit(c, 0);
        } while (t > 2);
    }
    bbPutBit(c, (unsigned)i & 1);
    bbPutBit(c, 1);
}


static void
code_match(UCL_COMPRESS_T *c, unsigned int m_len, const unsigned int m_off)
{
    while (m_len > c->conf.max_match)
    {
        code_match(c, c->conf.max_match - 3, m_off);
        m_len -= c->conf.max_match - 3;
    }

    c->match_bytes += m_len;
    if (m_len > c->result[3])
        c->result[3] = m_len;
    if (m_off > c->result[1])
        c->result[1] = m_off;

    bbPutBit(c, 0);

    if (m_off == c->last_m_off)
    {
        bbPutBit(c, 0);
        bbPutBit(c, 1);
    }
    else
    {
        code_prefix_ss11(c, 1 + ((m_off - 1) >> 8));
        bbPutByte(c, (unsigned)m_off - 1);
    }
    m_len = m_len - 1 - (m_off > M2_MAX_OFFSET);
    if (m_len >= 4)
    {
        bbPutBit(c,0);
        bbPutBit(c,0);
        code_prefix_ss11(c, m_len - 4);
    }
    else
    {
        bbPutBit(c, m_len > 1);
        bbPutBit(c, (unsigned)m_len & 1);
    }

    c->last_m_off = m_off;
}


static void
code_run(UCL_COMPRESS_T *c, const unsigned char* ii, unsigned int lit)
{
    if (lit == 0)
        return;
    c->lit_bytes += lit;
    if (lit > c->result[5])
        c->result[5] = lit;
    do {
        bbPutBit(c, 1);
        bbPutByte(c, *ii++);
    } while (--lit > 0);
}


/***********************************************************************
//
************************************************************************/

static int
len_of_coded_match(UCL_COMPRESS_T *c, unsigned int m_len, unsigned int m_off)
{
    int b;
    if (m_len < 2 || (m_len == 2 && (m_off > M2_MAX_OFFSET))
        || m_off > c->conf.max_offset)
        return -1;
    assert(m_off > 0);

    m_len = m_len - 2 - (m_off > M2_MAX_OFFSET);

    if (m_off == c->last_m_off)
        b = 1 + 2;
    else
    {
        b = 1 + 10;
        m_off = (m_off - 1) >> 8;
        while (m_off > 0)
        {
            b += 2;
            m_off >>= 1;
        }
    }

    b += 2;
    if (m_len < 3)
        return b;
    m_len -= 3;

    do {
        b += 2;
        m_len >>= 1;
    } while (m_len > 0);

    return b;
}


/***********************************************************************
//
************************************************************************/

int ucl_nrv2b_99_compress    ( const unsigned char* in, unsigned int in_len,
                                   unsigned char* out, unsigned int* out_len,
                                   ucl_progress_callback_t* cb,
                                   int level,
                             const struct ucl_compress_config_t* conf,
                                   unsigned int* result)
{
    const unsigned char* ii;
    unsigned int lit;
    unsigned int m_len, m_off;
    UCL_COMPRESS_T c_buffer;
    UCL_COMPRESS_T * const c = &c_buffer;
#undef s
#if defined(SWD_USE_MALLOC)
    ucl_swd_t the_swd;
#   define s (&the_swd)
#else
    ucl_swd_p s;
#endif
    unsigned int result_buffer[16];
    int r;

    struct swd_config_t
    {
        unsigned try_lazy;
        unsigned int good_length;
        unsigned int max_lazy;
        unsigned int nice_length;
        unsigned int max_chain;
        uint32_t flags;
        uint32_t max_offset;
    };
    const struct swd_config_t *sc;
    static const struct swd_config_t swd_config[10] = {
#define F SWD_F
        /* faster compression */
        {   0,   0,   0,   8,    4,   0,  48*1024L },
        {   0,   0,   0,  16,    8,   0,  48*1024L },
        {   0,   0,   0,  32,   16,   0,  48*1024L },
        {   1,   4,   4,  16,   16,   0,  48*1024L },
        {   1,   8,  16,  32,   32,   0,  48*1024L },
        {   1,   8,  16, 128,  128,   0,  48*1024L },
        {   2,   8,  32, 128,  256,   0, 128*1024L },
        {   2,  32, 128,   F, 2048,   1, 128*1024L },
        {   2,  32, 128,   F, 2048,   1, 256*1024L },
        {   2,   F,   F,   F, 4096,   1, SWD_N }
        /* max. compression */
#undef F
    };

    if (level < 1 || level > 10)
        return UCL_E_INVALID_ARGUMENT;
    sc = &swd_config[level - 1];

    memset(c, 0, sizeof(*c));
    memset(&c->conf, 0xff, sizeof(c->conf));
    c->ip = c->in = in;
    c->in_end = in + in_len;
    c->out = out;
    if (cb && cb->callback)
        c->cb = cb;
    cb = NULL;
    c->result = result ? result : (unsigned int*) result_buffer;
    result = NULL;
    memset(c->result, 0, 16*sizeof(*c->result));
    c->result[0] = c->result[2] = c->result[4] = UINT_MAX;
    if (conf)
        memcpy(&c->conf, conf, sizeof(c->conf));
    conf = NULL;
    r = bbConfig(c, 8);
    if (r == 0)
        r = bbConfig(c, c->conf.bb_size);
    if (r != 0)
        return UCL_E_INVALID_ARGUMENT;
    c->bb_op = out;

    ii = c->ip;             /* point to start of literal run */
    lit = 0;

#if !defined(s)
    s = (ucl_swd_p) ucl_malloc(ucl_sizeof(*s));
    if (!s)
        return UCL_E_OUT_OF_MEMORY;
#endif
    s->f = UCL_MIN((unsigned int)SWD_F, c->conf.max_match);
    s->n = UCL_MIN((unsigned int)SWD_N, sc->max_offset);
    if (c->conf.max_offset != UINT_MAX)
        s->n = UCL_MIN(SWD_N, c->conf.max_offset);
    if (in_len < s->n)
        s->n = UCL_MAX(in_len, 256);
    if (s->f < 8 || s->n < 256)
        return UCL_E_INVALID_ARGUMENT;
    r = init_match(c,s,NULL,0,sc->flags);
    if (r != UCL_E_OK)
    {
#if !defined(s)
        ucl_free(s);
#endif
        return r;
    }
    if (sc->max_chain > 0)
        s->max_chain = sc->max_chain;
    if (sc->nice_length > 0)
        s->nice_length = sc->nice_length;
    if (c->conf.max_match < s->nice_length)
        s->nice_length = c->conf.max_match;

    if (c->cb)
        (*c->cb->callback)(0,0,-1,c->cb->user);

    c->last_m_off = 1;
    r = find_match(c,s,0,0);
    if (r != UCL_E_OK)
        return r;
    while (c->look > 0)
    {
        unsigned int ahead;
        unsigned int max_ahead;
        int l1, l2;

        c->codesize = (unsigned int) (c->bb_op - out);

        m_len = c->m_len;
        m_off = c->m_off;

        assert(c->bp == c->ip - c->look);
        assert(c->bp >= in);
        if (lit == 0)
            ii = c->bp;
        assert(ii + lit == c->bp);
        assert(s->b_char == *(c->bp));

        if (m_len < 2 || (m_len == 2 && (m_off > M2_MAX_OFFSET))
            || m_off > c->conf.max_offset)
        {
            /* a literal */
            lit++;
            s->max_chain = sc->max_chain;
            r = find_match(c,s,1,0);
            assert(r == 0);
            continue;
        }

        /* shall we try a lazy match ? */
        ahead = 0;
        if (sc->try_lazy <= 0 || m_len >= sc->max_lazy || m_off == c->last_m_off)
        {
            /* no */
            l1 = 0;
            max_ahead = 0;
        }
        else
        {
            /* yes, try a lazy match */
            l1 = len_of_coded_match(c,m_len,m_off);
            assert(l1 > 0);
            max_ahead = UCL_MIN((unsigned int)sc->try_lazy, m_len - 1);
        }

        while (ahead < max_ahead && c->look > m_len)
        {
            if (m_len >= sc->good_length)
                s->max_chain = sc->max_chain >> 2;
            else
                s->max_chain = sc->max_chain;
            r = find_match(c,s,1,0);
            ahead++;

            assert(r == 0);
            assert(c->look > 0);
            assert(ii + lit + ahead == c->bp);

            if (c->m_len < 2)
                continue;
            l2 = len_of_coded_match(c,c->m_len,c->m_off);
            if (l2 < 0)
                continue;
            if (l1 + (int)(ahead + c->m_len - m_len) * 5 > l2 + (int)(ahead) * 9)
            {
                c->lazy++;
                {
                    lit += ahead;
                    assert(ii + lit == c->bp);
                }
                goto lazy_match_done;
            }
        }

        assert(ii + lit + ahead == c->bp);

        /* 1 - code run */
        code_run(c,ii,lit);
        lit = 0;

        /* 2 - code match */
        code_match(c,m_len,m_off);
        s->max_chain = sc->max_chain;
        r = find_match(c,s,m_len,1+ahead);
        assert(r == 0);

lazy_match_done: ;
    }

    /* store final run */
    code_run(c,ii,lit);

    /* EOF */
    bbPutBit(c, 0);
    code_prefix_ss11(c, UCL_UINT32_C(0x1000000));
    bbPutByte(c, 0xff);

    bbFlushBits(c, 0);

    assert(c->textsize == in_len);
    c->codesize = (unsigned int) (c->bb_op - out);
    *out_len = (unsigned int) (c->bb_op - out);
    if (c->cb)
        (*c->cb->callback)(c->textsize,c->codesize,4,c->cb->user);

    assert(c->lit_bytes + c->match_bytes == in_len);

    swd_exit(s);
#if !defined(s)
    ucl_free(s);
#endif
    return UCL_E_OK;
#undef s
}

