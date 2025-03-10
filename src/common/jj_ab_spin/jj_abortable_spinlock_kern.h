#ifndef JJ_ABORTABLE_SPINLOCK_KERN_H_
#define JJ_ABORTABLE_SPINLOCK_KERN_H_

#include <linux/preempt.h>
#include <linux/irqflags.h>
#include <linux/compiler.h>
#include <linux/bottom_half.h>
#include <linux/typecheck.h>
#include "process_id.h"
#include "jj_abortable_spin.h"
#include "spinlock_kern_macro.h"

#ifndef JJ_SPIN_NAME
#define JJ_SPIN_NAME JOIN(jj_spin, JJ_AB_MAX_PROC_SHIFT)
#endif

#ifndef JJ_SPIN_LOCKNAME
#define JJ_SPIN_LOCKNAME JOIN(jj_spinlock, JJ_AB_MAX_PROC_SHIFT)
#endif

/* machine_id starts from 1 */
static inline void slock_kern(JJ_SPIN_LOCKNAME)(struct JJ_SPIN_LOCKNAME *lock, uint16_t machine_id)
{
    uint16_t process_id;
    bool ret;
    BUG_ON(machine_id < 1);
    ret = get_process_identifier(machine_id, &process_id);
    BUG_ON(!ret);
    jj_spinlock_recover_f(lock, process_id);
    ret = jj_spinlock_enter_f(lock, process_id, spin_no_abort);
    BUG_ON(!ret);
}

static inline void sunlock_kern(JJ_SPIN_LOCKNAME)(struct JJ_SPIN_LOCKNAME *lock, uint16_t machine_id)
{
    uint16_t process_id;
    bool ret;
    uint64_t peer;
    BUG_ON(machine_id < 1);
    ret = get_process_identifier(machine_id, &process_id);
    BUG_ON(!ret);
    ret = jj_spinlock_exit_f(lock, process_id, &peer);
    (void)peer;
}

static inline void do_slock(JJ_SPIN_NAME)(struct JJ_SPIN_LOCKNAME *lock, uint16_t machine_id) __acquires(lock)
{
    __acquire(lock);
    slock_kern(JJ_SPIN_LOCKNAME)(lock, machine_id);
    mmiowb_spin_lock();
}

static inline void do_sunlock(JJ_SPIN_NAME)(struct JJ_SPIN_LOCKNAME *lock, uint16_t machine_id) __releases(lock)
{
    mmiowb_spin_unlock();
    sunlock_kern(JJ_SPIN_LOCKNAME)(lock, machine_id);
    __release(lock);
}

def_slock_irqsave(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)

#define jj_spin_lock_irqsave(lock, machine_id, flags)            \
    do {                                                         \
        typecheck(unsigned long, flags);                         \
        flags = slock_irqsave(JJ_SPIN_NAME)(lock, machine_id);   \
    } while(0)

def_slock_irq(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)
def_slock_bh(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)

/**
 * In linux kernel, the process id is the CPU id in the CXL memory pool.
 * The cpu id is calculated inside the lock
 */
def_slock(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)
def_sunlock(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)
def_sunlock_irqrestore(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)
def_sunlock_irq(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)
def_sunlock_bh(JJ_SPIN_NAME, JJ_SPIN_LOCKNAME)

#endif // JJ_ABORTABLE_SPINLOCK_KERN_H_
