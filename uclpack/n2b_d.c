#include <stdio.h>
#include <stdint.h>
#include <ucl.h>

int ucl_nrv2b_decompress_8          ( const unsigned char* src, unsigned int src_len,
                                        unsigned char* dst, unsigned int* dst_len,
                                        void* wrkmem )
{
    uint32_t bb = 0;
    unsigned int ilen = 0, olen = 0, last_m_off = 1;
    const unsigned int oend = *dst_len;

#define fail(x,r)   if (x) { *dst_len = olen; return r; }
#define getbit_8(bb, src, ilen) \
    (((bb = bb & 0x7f ? bb*2 : ((unsigned)src[ilen++]*2+1)) >> 8) & 1)
#define getbit(bb) getbit_8(bb,src,ilen)

    for (;;)
    {
        unsigned int m_off, m_len;

        while (getbit(bb))
        {
            fail(ilen >= src_len, UCL_E_INPUT_OVERRUN);
            fail(olen >= oend, UCL_E_OUTPUT_OVERRUN);
            dst[olen++] = src[ilen++];
        }
        m_off = 1;
        do {
            m_off = m_off*2 + getbit(bb);
            fail(ilen >= src_len, UCL_E_INPUT_OVERRUN);
            fail(m_off > UCL_UINT32_C(0xffffff) + 3, UCL_E_LOOKBEHIND_OVERRUN);
        } while (!getbit(bb));
        if (m_off == 2)
        {
            m_off = last_m_off;
        }
        else
        {
            fail(ilen >= src_len, UCL_E_INPUT_OVERRUN);
            m_off = (m_off-3)*256 + src[ilen++];
            if (m_off == UCL_UINT32_C(0xffffffff))
                break;
            last_m_off = ++m_off;
        }
        m_len = getbit(bb);
        m_len = m_len*2 + getbit(bb);
        if (m_len == 0)
        {
            m_len++;
            do {
                m_len = m_len*2 + getbit(bb);
                fail(ilen >= src_len, UCL_E_INPUT_OVERRUN);
                fail(m_len >= oend, UCL_E_OUTPUT_OVERRUN);
            } while (!getbit(bb));
            m_len += 2;
        }
        m_len += (m_off > 0xd00);
        fail(olen + m_len > oend, UCL_E_OUTPUT_OVERRUN);
        fail(m_off > olen, UCL_E_LOOKBEHIND_OVERRUN);
        {
            const unsigned char* m_pos;
            m_pos = dst + olen - m_off;
            dst[olen++] = *m_pos++;
            do dst[olen++] = *m_pos++; while (--m_len > 0);
        }
    }
    *dst_len = olen;
    return ilen == src_len ? UCL_E_OK : (ilen < src_len ? UCL_E_INPUT_NOT_CONSUMED : UCL_E_INPUT_OVERRUN);
#undef fail
}

