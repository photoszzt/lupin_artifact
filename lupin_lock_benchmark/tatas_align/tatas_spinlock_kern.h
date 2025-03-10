#ifndef TATAS_SPINLOCK_KERN_H_
#define TATAS_SPINLOCK_KERN_H_

#include <linux/types.h>
#include "tatas_spin.h"
#include "process_id.h"
#include "spinlock_kern_macro.h"

#if defined(__KERNEL__) || defined(MODULE)
#include "spinlock_kern_name.h"
static inline void ttas_lock_kern(struct ttas* lock, uint16_t machine_id)
{
    uint16_t process_id;
    bool ret;
    BUG_ON(machine_id < 1);
    ret = get_process_identifier(machine_id, &process_id);
    BUG_ON(!ret);
    if (ttas_recover(lock, process_id)) {
        return;
    }
    ret = ttas_enter(lock, process_id, spin_no_abort);
    BUG_ON(!ret);
}

static inline void ttas_unlock_kern(struct ttas* lock, uint16_t machine_id)
{
    uint16_t process_id;
    bool ret;
    BUG_ON(machine_id < 1);
    ret = get_process_identifier(machine_id, &process_id);
    BUG_ON(!ret);
    ttas_exit(lock, process_id);
}

static inline void do_slock(ttas)(struct ttas *lock, uint16_t machine_id) __acquires(lock)
{
    __acquire(lock);
    slock_kern(ttas)(lock, machine_id);
    mmiowb_spin_lock();
}

static inline void do_sunlock(ttas)(struct ttas *lock, uint16_t machine_id) __releases(lock)
{
    mmiowb_spin_unlock();
    sunlock_kern(ttas)(lock, machine_id);
    __release(lock);
}

def_slock_irqsave(ttas, ttas)

#define ttas_lock_irqsave(lock, machine_id, flags)            \
    do {                                                         \
        typecheck(unsigned long, flags);                         \
        flags = slock_irqsave(ttas)(lock, machine_id);   \
    } while(0)

def_slock_irq(ttas, ttas)
def_slock_bh(ttas, ttas)

/**
 * In linux kernel, the process id is the CPU id in the CXL memory pool.
 * The cpu id is calculated inside the lock
 */
def_slock(ttas, ttas)
def_sunlock(ttas, ttas)
def_sunlock_irqrestore(ttas, ttas)
def_sunlock_irq(ttas, ttas)
def_sunlock_bh(ttas, ttas)

#endif

#endif // TATAS_SPINLOCK_KERN_H_
