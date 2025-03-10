#ifndef JJ_AB_WPORT_SPIN_H_
#define JJ_AB_WPORT_SPIN_H_

#include <linux/types.h>
#include "jj_ab_wport_css_status.h"
#include "template.h"
#include "common_macro.h"
#include "atomic_int_op.h"
#include "output_cacheline.h"
#include "persist_cas.h"
#include "atomic_bitset.h"
#include "load_and_flush.h"

/* Number of bits for active */
#ifndef wsize
#error "need to specify wsize"
#endif

#if wsize == 64
#define wtype uint64_t
#elif wsize == 32
#define wtype uint32_t
#elif wsize == 16
#define wtype uint16_t
#elif wsize == 8
#define wtype uint8_t
#else
#error "unrecognized wsize"
#endif

_Static_assert(sizeof(wtype) == wsize / 8, "sizeof wtype should be wsize");

#ifdef __cplusplus
extern "C" {
#endif

/* csst is the futex word. It needs to be appear at the top. */
#define jj_wport(port_size) JOIN(jj, PASTE(port_size, port))
#define jj_wport_spin(port_size) JOIN(jj_wport(port_size), spin)
struct jj_wport_spin(wsize) {
    _Atomic_uint64_t csst;
    _Atomic_uint64_t seq;
#ifdef __cplusplus
    std::atomic<wtype> active;
#elif defined(__KERNEL__) || defined(MODULE)
    _Atomic wtype active;
#else
    _Atomic(wtype) active;
#endif
};

#define jj_wport_spin_abort_f(port_size) JOIN(jj_wport(port_size), spinlock_abort)
#define jj_wport_spin_promote_f(port_size) JOIN(jj_wport(port_size), spinlock_promote)
#define jj_wport_spin_tryenter_f(port_size) JOIN(jj_wport(port_size), spinlock_tryenter)
#define jj_wport_spin_enter_f(port_size) JOIN(jj_wport(port_size), spinlock_enter)
#define jj_wport_spin_exit_f(port_size) JOIN(jj_wport(port_size), spinlock_exit)
#define jj_wport_spin_init_f(port_size) JOIN(jj_wport(port_size),spinlock_init)

bool jj_wport_spin_abort_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id) WARN_UNUSED_RESULT;
void jj_wport_spin_promote_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id, bool flag);
bool jj_wport_spin_tryenter_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id) WARN_UNUSED_RESULT;
bool jj_wport_spin_enter_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id,
    bool (*abort_signal)(uint32_t)) WARN_UNUSED_RESULT;
void jj_wport_spin_exit_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id);
void jj_wport_spin_init_f(wsize)(struct jj_wport_spin(wsize) *lock);

#define jj_wport_spin_recover_f(port_size) JOIN(jj_wport(port_size), spinlock_recover)
static force_inline void jj_wport_spin_recover_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id)
{
    bool ret;
    struct wport_csst csst = read_decompose_wport_csst(&lock->csst);
    if (csst.b && csst.s == process_id) {
        ret = jj_wport_spin_abort_f(wsize)(lock, process_id);
        (void)ret;
    }
}


#ifdef __cplusplus
}
#endif

#endif // JJ_AB_WPORT_SPIN_H_

#ifdef JJ_WPORT_SPIN_IMPL
#undef JJ_WPORT_SPIN_IMPL


#ifdef __cplusplus
extern "C" {
#endif

void jj_wport_spin_promote_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id, bool flag)
{
    struct wport_csst css_status = read_decompose_wport_csst(&lock->csst);
    uint64_t peer = css_status.s;
    uint64_t exp_css_status, desire_css_status;
    wtype active;

    if (!css_status.b) {
        active = load_and_flush(&lock->active);
        if (active != 0) {
            peer = next_bit(active, css_status.owner);
        } else {
            /* active is zero is equivalent to token == inf */
            if (!flag) {
                return;
            }
            peer = process_id;
        }
        exp_css_status = compose_wport_csst(false, css_status.s, css_status.owner);
        desire_css_status = compose_wport_csst(true, peer, (uint8_t)peer);

        // PRINT_INFO("[promote] exp css 0x%lx, desire css 0x%lx\n", exp_css_status, desire_css_status);
        if (!persist_cas_uint64(&lock->csst, &exp_css_status, desire_css_status)) {
            // PRINT_INFO("[promote] css status is not expected, got 0x%lx\n", exp_css_status);
            return;
        }
    }
}

bool jj_wport_spin_abort_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id)
{
    struct wport_csst cs;
    unset_bit(&lock->active, process_id);
    jj_wport_spin_promote_f(wsize)(lock, process_id, true);
    cs = read_decompose_wport_csst(&lock->csst);
    if (cs.b && cs.s == process_id) {
        /* already in CS */
        return false;
    }
    return true;
}

bool jj_wport_spin_tryenter_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id)
{
    uint64_t cur_css_status = pcas_read_uint64(&lock->csst);

    struct wport_csst css_status = decompose_wport_csst(cur_css_status);
    // PRINT_INFO("cur css status: %lx b: %lu, s: %lx\n", cur_css_status, css_status.b, css_status.s);
    if (css_status.b && css_status.s == process_id) {
        return true;
    }
    set_bit(&lock->active, process_id);
    jj_wport_spin_promote_f(wsize)(lock, process_id, false);
    css_status = read_decompose_wport_csst(&lock->csst);
    return css_status.b && css_status.s == process_id;
}

bool jj_wport_spin_enter_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id,
    bool (*abort_signal)(uint32_t))
{
    bool got_lock = jj_wport_spin_tryenter_f(wsize)(lock, process_id);
    uint32_t counter = 0;
    struct wport_csst cs_st;
    if (!got_lock) {
        while (true) {
            cs_st = read_decompose_wport_csst(&lock->csst);
            if ((cs_st.b && cs_st.s == process_id) || (abort_signal(counter++))) {
                break;
            }
            CPU_PAUSE();
        }
    }
    cs_st = read_decompose_wport_csst(&lock->csst);
    if (cs_st.b && cs_st.s == process_id) {
        /* break out the while loop due to acquired the lock */
        return true;
    } else {
        /* break out the while loop due to abort */
        return !jj_wport_spin_abort_f(wsize)(lock, process_id);
    }
}

/* unlock route; */
void jj_wport_spin_exit_f(wsize)(struct jj_wport_spin(wsize) *lock, uint8_t process_id)
{
    uint64_t s;
    struct wport_csst css = read_decompose_wport_csst(&lock->csst);
    unset_bit(&lock->active, process_id);
    s = pcas_read_uint64(&lock->seq);
    if (unlikely(s == WPORT_CSST_S_MAX)) {
        PRINT_ERR("seq counter overflow\n");
    }
    persist_atomic_write_uint64(&lock->seq, s+1);
    persist_atomic_write_uint64(&lock->csst, compose_wport_csst(false, s+1, css.owner));
    jj_wport_spin_promote_f(wsize)(lock, process_id, false);
}

void jj_wport_spin_init_f(wsize)(struct jj_wport_spin(wsize) *lock)
{
    lock->csst = compose_wport_csst(false, 1, 0);
    lock->seq = 1;
    lock->active = 0;
    shmem_output_cacheline((uint8_t *)lock, sizeof(struct jj_wport_spin(wsize)));
}

#ifdef __cplusplus
}
#endif

#endif // JJ_WPORT_SPIN_IMPL