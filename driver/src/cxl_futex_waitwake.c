#include <linux/types.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/sched/wake_q.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/dev_printk.h>
#include <linux/futex.h>
#include <linux/list.h>
#include "cxl_dev.h"
#include "cxl_futex.h"
#include "uapi/vcxl_shm.h"
#include "circular_queue.h"
#include "vcxl_def.h"
#include "atomic_int_op.h"
#include "task_robust_def.h"

struct vcxl_task_robust_bucket task_robust_table[TASK_ROBURST_TAB_SIZE];

static struct vcxl_futex_hash_bucket *
cxl_proc_hashbucket(struct vcxl_futex_hash_bucket futex_hash_table[],
		    __u64 offset)
{
	unsigned long hash;
	hash = hash_long(offset, WAIT_TABLE_BITS) % WAIT_TABLE_SIZE;
	return &futex_hash_table[hash];
}

// static wait_queue_head_t *cxl_proc_waitqueue(struct vcxl_ivpci_device * ivpci_dev, __u64 offset)
// {
//     unsigned long hash;
//     hash = hash_long(offset, WAIT_TABLE_BITS);
//     return &ivpci_dev->futex_wait_queue_table[hash];
// }

void init_vcxl_task_robust_hashtable(void)
{
	int i;
	for (i = 0; i < TASK_ROBURST_TAB_SIZE; i++) {
		INIT_HLIST_HEAD(&task_robust_table[i].chain);
		spin_lock_init(&task_robust_table[i].lock);
	}
}

void exit_vcxl_task_robust_hashtable(void)
{
	int i;
	struct vcxl_task_robust_bucket *slot;
	struct vcxl_task_robust_node *node;
	struct hlist_node *n;
	for (i = 0; i < TASK_ROBURST_TAB_SIZE; i++) {
		slot = &task_robust_table[i];
		hlist_for_each_entry_safe(node, n, (struct hlist_head *)slot,
					  hlink) {
			kfree(node);
		}
	}
}

struct vcxl_task_robust_bucket *
cxl_task_roburst_hashbucket(struct task_struct *curr)
{
	unsigned long hash;
	hash = hash_ptr((void *)curr, TASK_ROBURST_BITS) %
	       TASK_ROBURST_TAB_SIZE;
	return &task_robust_table[hash];
}

long do_set_robust_list(struct file *filp,
			struct robust_mutex_list __user *uhead)
{
	long rval = 0;
	struct vcxl_task_robust_bucket *hb;
	struct vcxl_task_robust_node *cursor;
	struct hlist_node *n;
	bool found = false;

	hb = cxl_task_roburst_hashbucket(current);

	spin_lock(&hb->lock);
	hlist_for_each_entry_safe(cursor, n, (struct hlist_head *)hb, hlink) {
		if (cursor->task == current) {
			cursor->rh = uhead;
			found = true;
		}
	}
	spin_unlock(&hb->lock);

	if (!found) {
		pr_err("need to register the process first via ioctl\n");
		return -EINVAL;
	}
	return rval;
}

static int handle_futex_death(uint64_t offset,
			      struct vcxl_futex_hash_bucket *futex_tab,
			      struct task_struct *curr)
{
	int ret = 0;
	struct vcxl_futex_q *this, *next;
	unsigned long flags;
	struct vcxl_futex_hash_bucket *hb;

	/* Futex address must be 64bit aligned */
	if ((offset % sizeof(uint64_t)) != 0)
		return -1;
	hb = cxl_proc_hashbucket(futex_tab, offset);

	if (!vcxl_futex_hb_waiters_pending(hb)) {
		return ret;
	}
	/* The task is about to die, no need to wake up. Just removing the entry from the table */
	spin_lock_irqsave(&hb->lock, flags);
	plist_for_each_entry_safe(this, next, &hb->chain, list) {
		if (this->key == offset && this->task == curr) {
			__vcxl_futex_unqueue(this);
			smp_store_release(&this->lock_ptr, NULL);
			break;
		}
	}
	spin_unlock_irqrestore(&hb->lock, flags);
	return ret;
}

static force_inline int
fetch_vcxl_robust_entry(struct robust_mutex_base __user **entry,
			struct robust_mutex_base __user *__user *head)
{
	unsigned long uentry;

	if (get_user(uentry, (unsigned long __user *)head)) {
		return -EFAULT;
	}
	*entry = (void __user *)uentry;
	return 0;
}

/* unlike the kernel robust list, this robust list is used to clean up the vcxl_futex_table */
void exit_robust_list(struct task_struct *curr,
		      struct vcxl_task_robust_node *node)
{
	struct robust_mutex_base __user *entry, *next_entry, *pending;
	int rc;
	unsigned int limit = ROBUST_LIST_LIMIT;
	struct robust_mutex_list __user *head = node->rh;
	uint64_t futex_offset;

	if (!head) {
		return;
	}

	/**
     * Fetch the list head
    */
	if (fetch_vcxl_robust_entry(&entry, (struct robust_mutex_base __user *
					     __user *)&head->list.next))
		return;

	/* Fetch any possible pending lock-add first, and handle it if it exists*/
	if (fetch_vcxl_robust_entry(&pending, (struct robust_mutex_base __user *
					       __user *)&head->list_op_pending))
		return;
	next_entry = (struct robust_mutex_base __user *)NULL;
	while (entry != &head->list) {
		/* Fetch the next entry in the list before calling handle_futex_death */
		rc = fetch_vcxl_robust_entry(&next_entry,
					     (struct robust_mutex_base __user *
					      __user *)&entry->next);
		/* A pending lock might already be on the list, so don't process it twice */
		if (entry != pending) {
			if (copy_from_user(&futex_offset, &entry->futex_offset,
					   sizeof(uint64_t))) {
				pr_err("fail to get futex_offset from userspace\n");
				return;
			}
			handle_futex_death(futex_offset, node->futex_tab, curr);
		}
		if (rc)
			return;
		entry = next_entry;
		if (!--limit)
			break;
		cond_resched();
	}
	if (pending) {
		if (copy_from_user(&futex_offset, &entry->futex_offset,
				   sizeof(uint64_t))) {
			pr_err("fail to get futex_offset from userspace\n");
			return;
		}
		handle_futex_death(futex_offset, node->futex_tab, curr);
	}
}

void __vcxl_futex_unqueue(struct vcxl_futex_q *q)
{
	struct vcxl_futex_hash_bucket *hb;
	if (WARN_ON(!q->lock_ptr) || WARN_ON(plist_node_empty(&q->list)))
		return;
	lockdep_assert_held(q->lock_ptr);

	hb = container_of(q->lock_ptr, struct vcxl_futex_hash_bucket, lock);
	plist_del(&q->list, &hb->chain);
	vcxl_futex_hb_waiters_dec(hb);
}

/**
 * @brief Remove the vcxl_futex_q from its vcxl_futex_hash_bucket
 *
 * @param q The vcxl_futex_q to unqueue
 * @return int 1 - if the vcxl_futex_q was still queued (and we removed unqueued it)
 *             0 - if the vcxl_futex_q was already unqueued by the waking thread
 */
int vcxl_futex_unqueue(struct vcxl_futex_q *q)
{
	spinlock_t *lock_ptr;
	unsigned long flags;
	int ret = 0;
	/* In the common case we don't take the spinlock, which is nice */
retry:
	/**
     * q->lock_ptr can change between this read and the following spin_lock.
     * Use READ_ONCE() to forbid the compiler from reloading q->lock_ptr and
     * optimizing lock_ptr out of the logic below.
     */
	lock_ptr = READ_ONCE(q->lock_ptr);
	if (lock_ptr != NULL) {
		spin_lock_irqsave(lock_ptr, flags);
		/*
		 * q->lock_ptr can change between reading it and
		 * spin_lock(), causing us to take the wrong lock.  This
		 * corrects the race condition.
		 *
		 * Reasoning goes like this: if we have the wrong lock,
		 * q->lock_ptr must have changed (maybe several times)
		 * between reading it and the spin_lock().  It can
		 * change again after the spin_lock() but only if it was
		 * already changed before the spin_lock().  It cannot,
		 * however, change back to the original value.  Therefore
		 * we can detect whether we acquired the correct lock.
		 */
		if (unlikely(q->lock_ptr != lock_ptr)) {
			spin_unlock_irqrestore(lock_ptr, flags);
			goto retry;
		}
		__vcxl_futex_unqueue(q);

		spin_unlock_irqrestore(lock_ptr, flags);
		ret = 1;
	}

	return ret;
}

/**
 * @brief vcxl_futex_queue() and wait for wakeup, timeout, or signal.
 *
 * @param hb the futex hash bucket, must be locked by the caller
 * @param q the futex queue to queue up on
 * @param timeout the prepared hrtimer_sleeper, or null for no timeout
 */
void vcxl_futex_wait_queue(struct vcxl_futex_hash_bucket *hb,
			   struct vcxl_futex_q *q,
			   struct hrtimer_sleeper *timeout)
{
	/**
     * The task state is guaranteed to be set before another task can
	 * wake it. set_current_state() is implemented using smp_store_mb() and
	 * vcxl_futex_queue() calls spin_unlock() upon completion, both serializing
	 * access to the hash list and forcing another memory barrier.
     */
#ifdef TASK_FREEZABLE
	set_current_state(TASK_INTERRUPTIBLE | TASK_FREEZABLE);
#else
	set_current_state(TASK_INTERRUPTIBLE);
#endif
	vcxl_futex_queue(q, hb);

	if (timeout) {
		hrtimer_sleeper_start_expires(timeout, HRTIMER_MODE_ABS);
	}
	/**
     * @brief If we have been removed from the hash list, then another task
     * has tried to wake us up, and we can skip the call to schedule().
     */
	if (likely(!plist_node_empty(&q->list))) {
		/**
         * If the timer has already expired, current will already be flagged
         * for rescheduling. Only call schedule if there's no timeout, or if it
         * has yet to expire.
         */
		if (!timeout || timeout->task) {
#ifdef TASK_FREEZABLE
			schedule();
#else
			freezable_schedule();
#endif
		}
	}
	__set_current_state(TASK_RUNNING);
}

struct vcxl_futex_hash_bucket *
vcxl_futex_q_lock(struct vcxl_ivpci_device *ivpci_dev, struct vcxl_futex_q *q)
	__acquires(&hb->lock)
{
	struct vcxl_futex_hash_bucket *hb;

	hb = cxl_proc_hashbucket(ivpci_dev->futex_hash_table, q->key);
	/**
     * Increment the counter before taking the lock so that
     * a potential waker won't miss a to-be-slept task that is
     * waiting for the spinlock. This is safe as all vcxl_futex_q_lock()
     * users end up calling vcxl_futex_queue(). Similarly, for housekeeping,
     * decrement the counter at vcxl_q_unlock() when some error has occurred
     * and we don't end up adding the task to the list.
     */
	vcxl_futex_hb_waiters_inc(hb); /* implies smp_mb(); (A) */
	q->lock_ptr = &hb->lock;
	q->irqsave_flags = &hb->irqsave_flags;

	/* the wake part is invoke in interrupt handler */
	spin_lock_irqsave(&hb->lock, *q->irqsave_flags);

	return hb;
}

void vcxl_futex_q_unlock(struct vcxl_futex_hash_bucket *hb)
	__releases(&hb->lock)
{
	spin_unlock_irqrestore(&hb->lock, hb->irqsave_flags);
	vcxl_futex_hb_waiters_dec(hb);
}

void __vcxl_futex_queue(struct vcxl_futex_q *q,
			struct vcxl_futex_hash_bucket *hb)
{
	int prio;

	/**
     * The priority used to register this element is
     * - either the real thread-priority for the real-time threads
     * (i.e. threads with a priority lower than MAX_RT_PRIO)
	 * - or MAX_RT_PRIO for non-RT threads.
	 * Thus, all RT-threads are woken first in priority order, and
	 * the others are woken last, in FIFO order.
     *
     */
	prio = min(current->normal_prio, MAX_RT_PRIO);

	plist_node_init(&q->list, prio);
	plist_add(&q->list, &hb->chain);
	q->task = current;
}

/**
 * @brief vcxl_futex_wait_setup() - prepare to wait on a futex
 *
 * @param ivpci_dev the cxl device
 * @param cond the struct contains wait parameters
 * @param address the address of the futex
 * @param the associated futex_q
 * @param storage for hash_bucket pointer to be returned to caller
 *
 * Setup the vcxl_futex_q and locate the vcxl_futex_hash_bucket. Get the futex
 * value and compare it with the expected value. Return with the hb lock held
 * on success, and unlocked on failure.
 *
 * Return:
 * - 0 - user addr contains val and hb has been locked;
 * - <1 - -EWOULDBLOCK (uaddr doesn't contain val) and hb is unlocked
 */
int vcxl_futex_wait_setup(struct vcxl_ivpci_device *ivpci_dev,
			  struct vcxl_cond_wait *cond, void *address,
			  struct vcxl_futex_q *q,
			  struct vcxl_futex_hash_bucket **hb)
{
	int ret = 0;
	u64 uval;

	q->key = cond->offset;
	/**
     * @brief Access the user value AFTER the hash-bucket is locked.
     * Order is important: see https://elixir.bootlin.com/linux/v6.3.8/source/kernel/futex/waitwake.c#L583
     *
     */
	*hb = vcxl_futex_q_lock(ivpci_dev, q);

	uval = load_uint64_acquire((_Atomic uint64_t *)address);

	if (uval != cond->value) {
		vcxl_futex_q_unlock(*hb);
		ret = -EAGAIN;
	}
	return ret;
}

static long handle_vcxl_cond_wait(struct vcxl_ivpci_device *ivpci,
				  struct vcxl_cond_wait *cond)
{
	struct vcxl_ivpci_device *ivpci_dev;

	long ret = 0;
	void *address = NULL;
	uint32_t *waiter_arr = NULL;
	ktime_t wake_time;
	struct hrtimer_sleeper timeout, *to = NULL;
	enum wait_types waiting_type;
	struct vcxl_futex_hash_bucket *hb;
	struct vcxl_futex_q q;

	/* Ensure that the offset is aligned to 8bytes */
	if (cond->offset & (sizeof(uint64_t) - 1)) {
		return -EADDRNOTAVAIL;
	}
	/* Ensure that the offset is within shared memory */
	if (cond->offset + sizeof(uint64_t) > ivpci_dev->meta_dram.mem_length) {
		return -E2BIG;
	}
	address = shm_off_to_virtual_addr(&ivpci_dev->meta_dram, cond->offset);
	waiter_arr = (uint32_t *)shm_off_to_virtual_addr(
		&ivpci_dev->meta_dram, cond->waiter_arr_offset);
	waiting_type = cond->wait_type & 0xffff;

	/* Ensure that the type of wait is valid */
	switch (waiting_type) {
	case VCXL_WAIT_IF_EQUAL:
		break;
	case VCXL_WAIT_IF_EQUAL_TIMEOUT:
		to = &timeout;
		break;
	default:
		return -EINVAL;
	}
	if (to) {
		/* Copy the user-supplied timesec into the kernel structure.
		 * We do things this way to flatten differences between 32 bit
		 * and 64 bit timespecs.
		 */
		if (cond->wake_time_nsec >= NSEC_PER_SEC)
			return -EINVAL;
		wake_time =
			ktime_set(cond->wake_time_sec, cond->wake_time_nsec);

		hrtimer_init_sleeper_on_stack(to, CLOCK_MONOTONIC,
					      HRTIMER_MODE_ABS);
		hrtimer_set_expires_range_ns(&to->timer, wake_time,
					     current->timer_slack_ns);
	}
	while (1) {
		ret = vcxl_futex_wait_setup(ivpci_dev, cond, address, &q, &hb);
		if (ret) {
			break;
		}
		atomic_fetch_add_uint32(
			(_Atomic uint32_t
				 *)&waiter_arr[ivpci_dev->meta_dram.machine_id -
					       1],
			1);
		vcxl_futex_wait_queue(hb, &q, to);

		ret = 0;

		if (!vcxl_futex_unqueue(&q)) {
			break;
		}
		ret = -ETIMEDOUT;
		if (to && !to->task) {
			break;
		}
		/* Count the number of times that we woke up. */
		cond->wakes++;
		if (!signal_pending(current)) {
			continue;
		}
	}
	if (to) {
		hrtimer_cancel(&to->timer);
		destroy_hrtimer_on_stack(&to->timer);
	}
	return ret;
}

long do_vcxl_cond_wait(struct vcxl_ivpci_device *ivpci,
		       struct vcxl_cond_wait __user *ucond)
{
	struct vcxl_cond_wait cond;
	long rval = 0;
	if (copy_from_user(&cond, ucond, sizeof(cond))) {
		return -EFAULT;
	}
	cond.wakes = 0;
	rval = handle_vcxl_cond_wait(ivpci, &cond);
	if (copy_to_user(ucond, &cond, sizeof(cond))) {
		return -EFAULT;
	}
	return rval;
}

irqreturn_t vcxl_ivpci_interrupt_process_wake_table_next(int irq, void *dev_id)
{
	struct vcxl_ivpci_device *ivpci_dev = dev_id;

	if (unlikely(ivpci_dev == NULL)) {
		return IRQ_NONE;
	}

	dev_info(&ivpci_dev->dev->dev, PFX "interrupt: %d\n", irq);
	vcxl_futex_wake(ivpci_dev, WAKE_NEXT);

	return IRQ_HANDLED;
}

irqreturn_t vcxl_ivpci_interrupt_process_wake_table_all(int irq, void *dev_id)
{
	struct vcxl_ivpci_device *ivpci_dev = dev_id;

	if (unlikely(ivpci_dev == NULL)) {
		return IRQ_NONE;
	}

	dev_info(&ivpci_dev->dev->dev, PFX "interrupt: %d\n", irq);
	vcxl_futex_wake(ivpci_dev, WAKE_ALL);

	return IRQ_HANDLED;
}

/**
 * From: /kernel/sched/core.c
 */
static bool __wake_q_add(struct wake_q_head *head, struct task_struct *task)
{
	struct wake_q_node *node = &task->wake_q;

	/*
	 * Atomically grab the task, if ->wake_q is !nil already it means
	 * it's already queued (either by us or someone else) and will get the
	 * wakeup due to that.
	 *
	 * In order to ensure that a pending wakeup will observe our pending
	 * state, even in the failed case, an explicit smp_mb() must be used.
	 */
	smp_mb__before_atomic();
	if (unlikely(cmpxchg_relaxed(&node->next, NULL, WAKE_Q_TAIL)))
		return false;

	/*
	 * The head is context local, there can be no concurrency.
	 */
	*head->lastp = node;
	head->lastp = &node->next;
	return true;
}

/**
 * wake_q_add_safe() - safely queue a wakeup for 'later' waking.
 * @head: the wake_q_head to add @task to
 * @task: the task to queue for 'later' wakeup
 *
 * Queue a task for later wakeup, most likely by the wake_up_q() call in the
 * same context, _HOWEVER_ this is not guaranteed, the wakeup can come
 * instantly.
 *
 * This function must be used as-if it were wake_up_process(); IOW the task
 * must be ready to be woken at this location.
 *
 * This function is essentially a task-safe equivalent to wake_q_add(). Callers
 * that already hold reference to @task can call the 'safe' version and trust
 * wake_q to do the right thing depending whether or not the @task is already
 * queued for wakeup.
 */
static void vcxl_wake_q_add_safe(struct wake_q_head *head,
				 struct task_struct *task)
{
	if (!__wake_q_add(head, task))
		put_task_struct(task);
}

/*
 * The hash bucket lock must be held when this is called.
 * Afterwards, the vcxl_futex_q must not be accessed. Callers
 * must ensure to later call vcxl_wake_up_q() for the actual
 * wakeups to occur.
 */
void vcxl_futex_wake_mark(struct wake_q_head *wake_q, struct vcxl_futex_q *q)
{
	struct task_struct *task = q->task;

	get_task_struct(task);
	__vcxl_futex_unqueue(q);

	/*
	 * The waiting task can free the vcxl_futex_q as soon as q->lock_ptr = NULL
	 * is written, without taking any locks. This is possible in the event
	 * of a spurious wakeup, for example. A memory barrier is required here
	 * to prevent the following store to lock_ptr from getting ahead of the
	 * plist_del in __vcxl_futex_unqueue().
	 */
	smp_store_release(&q->lock_ptr, NULL);

	/*
	 * Queue the task for later wakeup for after we've released
	 * the hb->lock.
	 */
	vcxl_wake_q_add_safe(wake_q, task);
}

void vcxl_wake_up_q(struct wake_q_head *head)
{
	struct wake_q_node *node = head->first;

	while (node != WAKE_Q_TAIL) {
		struct task_struct *task;

		task = container_of(node, struct task_struct, wake_q);
		/* Task can safely be re-inserted now: */
		node = node->next;
		task->wake_q.next = NULL;

		/*
		 * wake_up_process() executes a full barrier, which pairs with
		 * the queueing in wake_q_add() so as not to miss wakeups.
		 */
		wake_up_process(task);
		put_task_struct(task);
	}
}

static void wake_proc(struct vcxl_ivpci_device *ivpci_dev, u64 offset,
		      enum wake_types wt)
{
	struct vcxl_futex_hash_bucket *hb;
	unsigned long flags;
	struct vcxl_futex_q *this, *next;
	DEFINE_WAKE_Q(wake_q);
	int wakes = 0;

	hb = cxl_proc_hashbucket(&ivpci_dev->futex_hash_table[0], offset);

	if (!vcxl_futex_hb_waiters_pending(hb)) {
		dev_info(&ivpci_dev->dev->dev, "no waiters pending\n");
		return;
	}

	spin_lock_irqsave(&hb->lock, flags);

	plist_for_each_entry_safe(this, next, &hb->chain, list) {
		if (this->key == offset) {
			vcxl_futex_wake_mark(&wake_q, this);
			wakes += 1;
			if (wt == WAKE_NEXT) {
				break;
			}
		}
	}

	spin_unlock_irqrestore(&hb->lock, flags);
	vcxl_wake_up_q(&wake_q);
}

// called in interrupt context
void vcxl_futex_wake(struct vcxl_ivpci_device *ivpci_dev, enum wake_types wt)
{
	struct circular_queue *cq = self_vcxl_addr_queue(&ivpci_dev->meta_dram);
	u64 offset;
	int deq_ret = 0;

	if (cq == NULL) {
		dev_err(&ivpci_dev->dev->dev, "invalid vmid: %d\n",
			ivpci_dev->meta_dram.machine_id);
		return;
	}
	deq_ret = circular_queue_dequeue(cq, &offset,
					 ivpci_dev->meta_dram.machine_id);
	/* No entry */
	if (deq_ret == -1) {
		dev_info(&ivpci_dev->dev->dev, "no entry in address queue\n");
		return;
	}
	wake_proc(ivpci_dev, offset, wt);
}

long do_vcxl_wake(struct file *filp, struct vcxl_cond_wake __user *uwake)
{
	struct vcxl_cond_wake wake;
	long rval = 0;
	int enq_ret = 0;
	struct vcxl_ivpci_device *ivpci_dev;
	u16 vector;
	u16 ivposition;
	struct circular_queue *cq;
	enum wake_types wt;

	if (copy_from_user(&wake, uwake, sizeof(wake))) {
		return -EFAULT;
	}
	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	vector = wake.arg & 0xffff;
	ivposition = (u16)((wake.arg & 0xffff0000) >> 16);
	dev_info(&ivpci_dev->dev->dev, "wake vector: %u, peer id: %u\n", vector,
		 ivposition);
	// 0 ivposition is expected to be taken. The vm id starts from 1.
	if (vector == CXL_IVPCI_VECTOR_PROCESS_WAKE_TABLE_ALL) {
		wt = WAKE_ALL;
	} else if (vector == CXL_IVPCI_VECTOR_PROCESS_WAKE_TABLE_NEXT) {
		wt = WAKE_NEXT;
	} else {
		dev_err(&ivpci_dev->dev->dev, "Invalid wake vector: %u\n",
			vector);
		return -EINVAL;
	}

	if (ivposition == ivpci_dev->meta_dram.machine_id) {
		wake_proc(ivpci_dev, wake.offset, wt);
	} else {
		cq = peer_vcxl_addr_queue(&ivpci_dev->meta_dram, ivposition);
		if (cq == NULL) {
			dev_err(&ivpci_dev->dev->dev, "invalid vmid: %u\n",
				ivposition);
			return -EVM_ID;
		}
		// blocking to try to insert the offset into the circular queue.
		while (1) {
			enq_ret = circular_queue_enqueue(cq, wake.offset,
							 ivposition);
			if (enq_ret == 0) {
				break;
			}
		}
		iowrite32(wake.arg & 0xffffffff,
			  ivpci_dev->regs_addr + DOORBELL_OFF);
	}
	return rval;
}
