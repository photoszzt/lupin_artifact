#ifndef JJ_ABORTABLE_SPINLOCK_H_
#define JJ_ABORTABLE_SPINLOCK_H_

#include "spinlock_kern_name.h"

#define JJ_SPIN_NAME JOIN(jj_spin, JJ_AB_MAX_PROC_SHIFT)
#define JJ_SPIN_LOCKNAME JOIN(jj_spinlock, JJ_AB_MAX_PROC_SHIFT)

#define jj_spin_lock_f slock(JJ_SPIN_NAME)
#define jj_spin_lock_bh_f slock_bh(JJ_SPIN_NAME)
#define jj_spin_lock_irq_f slock_irq(JJ_SPIN_NAME)

#define jj_spin_unlock_f sunlock(JJ_SPIN_NAME)
#define jj_spin_trylock_f JOIN(JJ_SPIN_NAME, trylock)
#define jj_spin_unlock_irqrestore_f sunlock_irqrestore(JJ_SPIN_NAME)
#define jj_spin_unlock_irq_f sunlock_irq(JJ_SPIN_NAME)
#define jj_spin_unlock_bh_f sunlock_bh(JJ_SPIN_NAME)

#if defined(__KERNEL__) || defined(MODULE)
#include "jj_abortable_spinlock_kern.h"

#else
#include <assert.h>
#include <stdint.h>
#include "jj_abortable_spin.h"

#ifdef __cplusplus
extern "C" {
#endif

static bool spin_no_abort(uint32_t counter) {
    (void)counter;
    return false;
}

static inline void jj_spin_lock_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    bool ret;
    jj_spinlock_recover_f(lock, process_id);
    ret = jj_spinlock_enter_f(lock, process_id, spin_no_abort);
    assert(ret);
    (void)ret;
}

static inline void jj_spin_unlock_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    uint64_t peer;
    bool ret;
    ret = jj_spinlock_exit_f(lock, process_id, &peer);
    SUPPRESS_UNUSED(peer, ret);
}

static WARN_UNUSED_RESULT inline bool jj_spin_trylock_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    jj_spinlock_recover_f(lock, process_id);
    return jj_spinlock_tryenter_f(lock, process_id);
}

#define jj_spin_lock_irqsave(lock, machine_id, flags) (void)flags; jj_spin_lock_f(lock, machine_id)

static inline void jj_spin_lock_bh_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    jj_spin_lock_f(lock, process_id);
}

static inline void jj_spin_lock_irq_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    jj_spin_lock_f(lock, process_id);
}

static inline void jj_spin_unlock_irq_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    jj_spin_unlock_f(lock, process_id);
}

static inline void jj_spin_unlock_irqrestore_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id, unsigned long flags)
{
    (void)flags;
    jj_spin_unlock_f(lock, process_id);
}

static inline void jj_spin_unlock_bh_f(struct JJ_SPIN_LOCKNAME *lock, uint16_t process_id)
{
    jj_spin_unlock_f(lock, process_id);
}

#endif

#ifdef __cplusplus
}
#endif

#endif // JJ_ABORTABLE_SPINLOCK_H_
