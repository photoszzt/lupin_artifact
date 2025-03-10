#ifndef VCXL_FUTEX_H
#define VCXL_FUTEX_H

#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/sched/wake_q.h>
#include <linux/plist.h>
#include <linux/futex.h>
#include "circular_queue.h"
#include "cxl_dev.h"
#include "uapi/vcxl_shm.h"
#include "task_robust_def.h"

/**
 * The following from linux kernel: https://elixir.bootlin.com/linux/latest/source/kernel/futex/waitwake.c
 * The correct serialization ensures that a waiter either observes
 * the changed user space value before blocking or is woken by a
 * concurrent waker:
 *
 * CPU 0                                 CPU 1
 * val = *futex;
 * sys_futex(WAIT, futex, val);
 *   futex_wait(futex, val);
 *
 *   waiters++; (a)
 *   smp_mb(); (A) <-- paired with -.
 *                                  |
 *   lock(hash_bucket(futex));      |
 *                                  |
 *   uval = *futex;                 |
 *                                  |        *futex = newval;
 *                                  |        sys_futex(WAKE, futex);
 *                                  |          futex_wake(futex);
 *                                  |
 *                                  `--------> smp_mb(); (B)
 *   if (uval == val)
 *     queue();
 *     unlock(hash_bucket(futex));
 *     schedule();                         if (waiters)
 *                                           lock(hash_bucket(futex));
 *   else                                    wake_waiters(futex);
 *     waiters--; (b)                        unlock(hash_bucket(futex));
 *
 * Where (A) orders the waiters increment and the futex value read through
 * atomic operations (see futex_hb_waiters_inc) and where (B) orders the write
 * to futex and the waiters read (see futex_hb_waiters_pending()).
 *
 * This yields the following case (where X:=waiters, Y:=futex):
 *
 *	X = Y = 0
 *
 *	w[X]=1		w[Y]=1
 *	MB		MB
 *	r[Y]=y		r[X]=x
 *
 * Which guarantees that x==0 && y==0 is impossible; which translates back into
 * the guarantee that we cannot both miss the futex variable change and the
 * enqueue.
 */

extern void __vcxl_futex_queue(struct vcxl_futex_q *q, struct vcxl_futex_hash_bucket *hb);

/**
 * vcxl_futex_queue() - Enqueue the vcxl_futex_q on the vcxl_futex_hash_bucket
 * @q: the vcxl_futex_q to enqueue
 * @hb: the vcxl_futex_hash_bucket to enqueue on
 *
 * The hb->lock must be held by the caller, and is released here. A call to
 * vcxl_futex_queue() is typically paired with exactly one call to vcxl_futex_unqueue().
 */
static inline void vcxl_futex_queue(struct vcxl_futex_q *q, struct vcxl_futex_hash_bucket *hb)
    __releases(hb->lock)
{
    __vcxl_futex_queue(q, hb);
    spin_unlock(&hb->lock);
}

/**
 * @brief Reflects a new waiter being added to the waitqueue.
 *
 * @param hb the hash bucket that added the waiter
 */
static inline void vcxl_futex_hb_waiters_inc(struct vcxl_futex_hash_bucket *hb)
{
#ifdef CONFIG_SMP
    atomic_inc(&hb->waiters);
    /**
     * @brief Full barrier (A)
     */
    smp_mb__after_atomic();
#endif
}

/**
 * @brief Reflects a waiter being removed from the waitqueue by wakeup paths.
 *
 */
static inline void vcxl_futex_hb_waiters_dec(struct vcxl_futex_hash_bucket *hb)
{
#ifdef CONFIG_SMP
    atomic_dec(&hb->waiters);
#endif
}

static inline int vcxl_futex_hb_waiters_pending(struct vcxl_futex_hash_bucket *hb)
{
#ifdef CONFIG_SMP
    /**
     * Full barrier (B)
     */
    smp_mb();
    return atomic_read(&hb->waiters);
#else
    return 1;
#endif
}

extern void __vcxl_futex_unqueue(struct vcxl_futex_q *q);
extern int vcxl_futex_unqueue(struct vcxl_futex_q *q);
extern void vcxl_futex_wake_mark(struct wake_q_head *wake_q, struct vcxl_futex_q *q);
extern void vcxl_futex_wait_queue(struct vcxl_futex_hash_bucket *hb, struct vcxl_futex_q *q,
                                  struct hrtimer_sleeper *timeout);
extern struct vcxl_futex_hash_bucket* vcxl_futex_q_lock(struct vcxl_ivpci_device *ivpci_dev, struct vcxl_futex_q *q);
extern void vcxl_futex_q_unlock(struct vcxl_futex_hash_bucket *hb);
extern int vcxl_futex_wait_setup(struct vcxl_ivpci_device *ivpci_dev,
                                 struct vcxl_cond_wait *cond,
                                 void *address,
                                 struct vcxl_futex_q *q,
                                 struct vcxl_futex_hash_bucket **hb);
extern long do_vcxl_cond_wait(struct vcxl_ivpci_device * ivpci,
                              struct vcxl_cond_wait __user *ucond);
extern void vcxl_wake_up_q(struct wake_q_head *head);
extern void vcxl_futex_wake(struct vcxl_ivpci_device *ivpci_dev, enum wake_types wt);
extern long do_vcxl_wake(struct file *filp,
                         struct vcxl_cond_wake __user *uwake);
extern irqreturn_t vcxl_ivpci_interrupt_process_wake_table_next(int irq, void *dev_id);
extern irqreturn_t vcxl_ivpci_interrupt_process_wake_table_all(int irq, void *dev_id);
extern void exit_robust_list(struct task_struct *curr, struct vcxl_task_robust_node *node);

#endif // VCXL_FUTEX_H

