#ifndef JJ_AB_WPORT_SPINLOCK_KERN_H_
#define JJ_AB_WPORT_SPINLOCK_KERN_H_

#include <linux/preempt.h>
#include <linux/irqflags.h>
#include <linux/compiler.h>
#include <linux/bottom_half.h>
#include <linux/typecheck.h>
#include "jj_ab_wport_spin.h"
#include "process_id.h"
#include "spinlock_kern_macro.h"

/* machine_id starts from 1 */
static inline void slock_kern(jj_wport(wsize))(struct jj_wport_spin(wsize) *lock, uint16_t machine_id)
{
    uint16_t process_id;
    bool ret;
    BUG_ON(machine_id < 1);
    ret = get_process_identifier(machine_id, &process_id);
    BUG_ON(!ret);
    BUG_ON(process_id >= wsize);
    jj_wport_spin_recover_f(wsize)(lock, (uint8_t)process_id);
    ret = jj_wport_spin_enter_f(wsize)(lock, (uint8_t)process_id, spin_no_abort);
    BUG_ON(!ret);
}

static inline void sunlock_kern(jj_wport(wsize))(struct jj_wport_spin(wsize) *lock, uint16_t machine_id)
{
    uint16_t process_id;
    bool ret;
    BUG_ON(machine_id < 1);
    ret = get_process_identifier(machine_id, &process_id);
    BUG_ON(!ret);
    BUG_ON(process_id >= wsize);
    jj_wport_spin_exit_f(wsize)(lock, (uint8_t)process_id);
}

static inline void do_slock(jj_wport(wsize))(struct jj_wport_spin(wsize) *lock, uint16_t machine_id) __acquires(lock)
{
    __acquire(lock);
    slock_kern(jj_wport(wsize))(lock, machine_id);
    mmiowb_spin_lock();
}

static inline void do_sunlock(jj_wport(wsize))(struct jj_wport_spin(wsize) *lock, uint16_t machine_id) __releases(lock)
{
    mmiowb_spin_unlock();
    sunlock_kern(jj_wport(wsize))(lock, machine_id);
    __release(lock);
}

def_slock_irqsave(jj_wport(wsize), jj_wport_spin(wsize))

#define jj_wport_spin_lock_irqsave(lock, machine_id, flags)         \
    do {                                                            \
        typecheck(unsigned long, flags);                            \
        flags = slock_irqsave(jj_wport(wsize))(lock, machine_id);   \
    } while(0)

def_slock_irq(jj_wport(wsize), jj_wport_spin(wsize))
def_slock_bh(jj_wport(wsize), jj_wport_spin(wsize))

/**
 * In linux kernel, the process id is the CPU id in the CXL memory pool.
 * The cpu id is calculated inside the lock
 */
def_slock(jj_wport(wsize), jj_wport_spin(wsize))
def_sunlock(jj_wport(wsize), jj_wport_spin(wsize))
def_sunlock_irqrestore(jj_wport(wsize), jj_wport_spin(wsize))
def_sunlock_irq(jj_wport(wsize), jj_wport_spin(wsize))
def_sunlock_bh(jj_wport(wsize), jj_wport_spin(wsize))

#endif // JJ_AB_WPORT_SPINLOCK_KERN_H_
