#ifndef __UCL_H
#define __UCL_H

#include <limits.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/***********************************************************************
// error codes and prototypes
************************************************************************/
#define UCL_E_OK                    0
#define UCL_E_ERROR                 (-1)
#define UCL_E_INVALID_ARGUMENT      (-2)
#define UCL_E_OUT_OF_MEMORY         (-3)
/* compression errors */
#define UCL_E_NOT_COMPRESSIBLE      (-101)
/* decompression errors */
#define UCL_E_INPUT_OVERRUN         (-201)
#define UCL_E_OUTPUT_OVERRUN        (-202)
#define UCL_E_LOOKBEHIND_OVERRUN    (-203)
#define UCL_E_EOF_NOT_FOUND         (-204)
#define UCL_E_INPUT_NOT_CONSUMED    (-205)
#define UCL_E_OVERLAP_OVERRUN       (-206)

/***********************************************************************
// UCL requires a conforming <limits.h>
************************************************************************/
#if !defined(CHAR_BIT) || (CHAR_BIT != 8)
#  error "invalid CHAR_BIT"
#endif
#if !defined(UCHAR_MAX) || !defined(UINT_MAX) || !defined(ULONG_MAX)
#  error "check your compiler installation"
#endif
#if (USHRT_MAX < 1) || (UINT_MAX < 1) || (ULONG_MAX < 1)
#  error "your limits.h macros are broken"
#endif

#if !defined(UCL_UINT32_C)
#  if (UINT_MAX < UCL_0xffffffffL)
#    define UCL_UINT32_C(c)     c ## UL
#  else
#    define UCL_UINT32_C(c)     ((c) + 0U)
#  endif
#endif

/***********************************************************************
// Compression fine-tuning configuration.
//
// Pass a NULL pointer to the compression functions for default values.
// Otherwise set all values to -1 [i.e. initialize the struct by a
// `memset(x,0xff,sizeof(x))'] and then set the required values.
************************************************************************/
struct ucl_compress_config_t
{
    int bb_size;
    unsigned int max_offset;
    unsigned int max_match;
};

/* a progress indicator callback function */
typedef struct
{
    void (*callback) (unsigned int, unsigned int, int, void*);
    void* user;
}
ucl_progress_callback_t;

typedef struct
{
    int init;

    unsigned int look;          /* bytes in lookahead buffer */

    unsigned int m_len;
    unsigned int m_off;

    unsigned int last_m_len;
    unsigned int last_m_off;

    const unsigned char* bp;
    const unsigned char* ip;
    const unsigned char* in;
    const unsigned char* in_end;
    unsigned char* out;

    uint32_t bb_b;
    unsigned bb_k;
    unsigned bb_c_s;
    unsigned bb_c_s8;
    unsigned char* bb_p;
    unsigned char* bb_op;

    struct ucl_compress_config_t conf;
    unsigned int* result;

    ucl_progress_callback_t* cb;

    unsigned int textsize;      /* text size counter */
    unsigned int codesize;      /* code size counter */
    unsigned int printcount;    /* counter for reporting progress every 1K bytes */

    /* some stats */
    unsigned long lit_bytes;
    unsigned long match_bytes;
    unsigned long rep_bytes;
    unsigned long lazy;
}
UCL_COMPRESS_T;

/***********************************************************************
// compressors
//
// Pass NULL for `cb' (no progress callback), `conf' (default compression
// configuration) and `result' (no statistical result).
************************************************************************/
int ucl_nrv2b_99_compress      ( const unsigned char* src, unsigned int src_len,
                                   unsigned char* dst, unsigned int* dst_len,
                                   ucl_progress_callback_t* cb,
                                   int level,
                             const struct ucl_compress_config_t* conf,
                                   unsigned int* result );

/***********************************************************************
// decompressors
************************************************************************/
int ucl_nrv2b_decompress_8          ( const unsigned char* src, unsigned int src_len,
                                        unsigned char* dst, unsigned int* dst_len,
                                        void* wrkmem );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */

