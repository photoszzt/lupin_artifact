#ifndef JJ_AB_WPORT_SPINLOCK_H_
#define JJ_AB_WPORT_SPINLOCK_H_

#include "spinlock_kern_name.h"
#include "jj_ab_wport_spin.h"

#define jj_wport_spin_lock_f(psize) slock(jj_wport(psize))
#define jj_wport_spin_lock_bh_f(psize) slock_bh(jj_wport(psize))
#define jj_wport_spin_lock_irq_f(psize) slock_irq(jj_wport(psize))

#define jj_wport_spin_unlock_f(psize) sunlock(jj_wport(psize))
#define jj_wport_spin_unlock_bh_f(psize) sunlock_bh(jj_wport(psize))
#define jj_wport_spin_unlock_irq_f(psize) sunlock_irq(jj_wport(psize))
#define jj_wport_spin_unlock_irqrestore_f(psize) sunlock_irqrestore(jj_wport(psize))

#if defined(__KERNEL__) || defined(MODULE)
#include "jj_ab_wport_spinlock_kern.h"

#else
#include <assert.h>
#include <stdint.h>
#include "jj_ab_wport_spin.h"

#ifdef __cplusplus
extern "C" {
#endif

static bool spin_no_abort(uint32_t counter) {
    (void)counter;
    return false;
}

static inline void jj_wport_spin_lock_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id)
{
    bool ret;
    jj_wport_spin_recover_f(wsize)(lock, (uint8_t)process_id);
    ret = jj_wport_spin_enter_f(wsize)(lock, (uint8_t)process_id, spin_no_abort);
    assert(ret);
    (void)ret;
}

static inline void jj_wport_spin_unlock_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id)
{
    jj_wport_spin_exit_f(wsize)(lock, (uint8_t)process_id);
}

#define jj_wport_spin_lock_irqsave(lock, machine_id, flags) (void)flags; jj_wport_spin_lock_f(wsize)(lock, machine_id)

static inline void jj_wport_spin_lock_bh_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id)
{
    jj_wport_spin_lock_f(wsize)(lock, process_id);
}

static inline void jj_wport_spin_lock_irq_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id)
{
    jj_wport_spin_lock_f(wsize)(lock, process_id);
}

static inline void jj_wport_spin_unlock_irq_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id)
{
    jj_wport_spin_unlock_f(wsize)(lock, process_id);
}

static inline void jj_wport_spin_unlock_irqrestore_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id, unsigned long flags)
{
    (void)flags;
    jj_wport_spin_unlock_f(wsize)(lock, process_id);
}

static inline void jj_wport_spin_unlock_bh_f(wsize)(struct jj_wport_spin(wsize) *lock, uint16_t process_id)
{
    jj_wport_spin_unlock_f(wsize)(lock, process_id);
}

#ifdef __cplusplus
}
#endif

#endif

#endif // JJ_AB_WPORT_SPINLOCK_H_
