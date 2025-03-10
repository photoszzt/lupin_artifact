#ifndef COMMON_SPINLOCK_H_
#define COMMON_SPINLOCK_H_ 1

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/spinlock.h>
#else
#include <pthread.h>
#define spinlock_t		pthread_spinlock_t
#define spin_lock_init_shared(x) pthread_spin_init(x, PTHREAD_PROCESS_SHARED)
#define spin_lock_init_private(x) pthread_spin_init(x, PTHREAD_PROCESS_PRIVATE)

#define spin_lock(x)			pthread_spin_lock(x)
#define spin_unlock(x)			pthread_spin_unlock(x)
#define spin_lock_bh(x)			pthread_spin_lock(x)
#define spin_unlock_bh(x)		pthread_spin_unlock(x)
#define spin_lock_irq(x)		pthread_spin_lock(x)
#define spin_unlock_irq(x)		pthread_spin_unlock(x)
#define spin_lock_irqsave(x, f)		(void)f, pthread_spin_lock(x)
#define spin_unlock_irqrestore(x, f)	(void)f, pthread_spin_unlock(x)
#endif

#endif // COMMON_SPINLOCK_H_