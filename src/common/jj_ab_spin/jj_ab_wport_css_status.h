#ifndef JJ_AB_WPORT_CSS_STATUS_H_
#define JJ_AB_WPORT_CSS_STATUS_H_

#include "common_macro.h"
#include "atomic_int_op.h"
#include "persist_cas.h"
#if defined(__KERNEL__) || defined(MODULE)
#include <asm/bug.h>
#else
#include <assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bit 0-55 seq 56 bit for sequence
 * bit 56-61 port_id 6 bit 0-63
 * bit 62 b taken or not taken
 * bit 63 DIRTY bit for pcas
*/
struct wport_csst {
    bool b;
    uint8_t owner;
    uint64_t s;
};

static const uint64_t WPORT_CSST_B_SHIFT = 62;
static const uint64_t WPORT_CSST_B_MASK  = (1UL << WPORT_CSST_B_SHIFT);
static const uint64_t WPORT_CSST_PORTID_SHIFT = 56;
static const uint64_t WPORT_CSST_PORTID_MASK = ((uint64_t)0x3f << WPORT_CSST_PORTID_SHIFT);
static const uint64_t WPORT_CSST_S_SHIFT = 0;
static const uint64_t WPORT_CSST_S_MASK = (((uint64_t)1 << WPORT_CSST_PORTID_SHIFT) - 1);
static const uint64_t WPORT_CSST_S_MAX = WPORT_CSST_S_MASK;

static WARN_UNUSED_RESULT force_inline uint64_t compose_wport_csst(bool b, uint64_t s, uint8_t port)
{
    uint64_t taken = 0;
#if defined(__KERNEL__) || defined(MODULE)
    BUG_ON(port > 63);
#else
    assert(port < 64);
#endif
    if (s > WPORT_CSST_S_MAX) {
        PRINT_ERR("seq number overflow; wrap to 0\n");
        s = 0;
    }
    if (b) {
        taken = WPORT_CSST_B_MASK;
    }
    return taken | (uint64_t)(s << WPORT_CSST_S_SHIFT) | (uint64_t)((uint64_t)port << WPORT_CSST_PORTID_SHIFT);
}

static WARN_UNUSED_RESULT force_inline struct wport_csst decompose_wport_csst(uint64_t csst)
{
    return (struct wport_csst) {
        .b = (csst & WPORT_CSST_B_MASK) > 0,
        .owner = (uint8_t)((csst & WPORT_CSST_PORTID_MASK) >> WPORT_CSST_PORTID_SHIFT),
        .s = (csst & WPORT_CSST_S_MASK) >> WPORT_CSST_S_SHIFT,
    };
}

static WARN_UNUSED_RESULT force_inline struct wport_csst read_decompose_wport_csst(_Atomic_uint64_t *csst)
{
    return decompose_wport_csst(pcas_read_uint64(csst));
}

#ifdef __cplusplus
}
#endif

#endif // JJ_AB_WPORT_CSS_STATUS_H_
