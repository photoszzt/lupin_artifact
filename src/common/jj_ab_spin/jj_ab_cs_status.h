#ifndef JJ_AB_cs_status_H_
#define JJ_AB_cs_status_H_

#include "persist_cas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bit 0-61 seq
 * bit 62 b
 * bit 63 DIRTY bit for pcas
*/
struct cs_status {
    uint64_t b;
    uint64_t s;
};

#define CSST_B_SHIFT 62
#define CSST_B_MASK ((uint64_t)1 << CSST_B_SHIFT)
#define CSST_S_MASK (((uint64_t)1 << CSST_B_SHIFT)-1)
#define CSST_S_MAX CSST_S_MASK

static WARN_UNUSED_RESULT force_inline uint64_t compose_cs_status(bool b, uint64_t s)
{
    if (b) {
        return CSST_B_MASK | s;
    } else {
        return CSST_S_MASK & s;
    }
}

static force_inline void decompose_cs_status(uint64_t cs_status, struct cs_status* out)
{
    out->b = cs_status & CSST_B_MASK;
    out->s = cs_status & CSST_S_MASK;
}

static WARN_UNUSED_RESULT force_inline uint64_t read_cs_status(_Atomic_uint64_t *cs_status)
{
    return pcas_read_uint64(cs_status);
}

static force_inline void write_cs_status(_Atomic_uint64_t *cs_status, uint64_t val)
{
    persist_atomic_write_uint64(cs_status, val);
}

static WARN_UNUSED_RESULT force_inline bool atomic_cmpxchg_cs_status(_Atomic_uint64_t *cs_status, uint64_t* exp, uint64_t desire)
{
    return persist_cas_uint64(cs_status, exp, desire);
}

static force_inline void read_decompose_cs_status(_Atomic_uint64_t *cs_status, struct cs_status* out)
{
    decompose_cs_status(read_cs_status(cs_status), out);
}

#ifdef __cplusplus
}
#endif

#endif // JJ_AB_cs_status_H_
