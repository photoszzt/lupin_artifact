#ifndef SPINLOCK_KERN_NAME_H_
#define SPINLOCK_KERN_NAME_H_

#include "template.h"

#define slock_irqsave(name) JOIN(JOIN(_, name), lock_irqsave)
#define slock_irqsave_gen(name) JOIN(JOIN(_, name), lock_with_gen_irqsave)
#define slock_irq(name) JOIN(name, lock_irq)
#define slock_irq_gen(name) JOIN(name, lock_with_gen_irq)
#define slock_bh(name) JOIN(name, lock_bh)
#define slock_bh_gen(name) JOIN(name, lock_with_gen_bh)
#define slock(name) JOIN(name, lock)
#define slock_gen(name) JOIN(name, lock_with_gen)
#define sunlock(name) JOIN(name, unlock)
#define sunlock_irqrestore(name) JOIN(name, unlock_irqrestore)
#define sunlock_irq(name) JOIN(name, unlock_irq)
#define sunlock_bh(name) JOIN(name, unlock_bh)
#define do_slock(name) JOIN(PASTE(do_, name), lock)
#define do_slock_gen(name) JOIN(PASTE(do_, name), lock_with_gen)
#define do_sunlock(name) JOIN(PASTE(do_, name), unlock)
#define slock_kern(name) JOIN(name, lock_kern)
#define sunlock_kern(name) JOIN(name, unlock_kern)

#endif // SPINLOCK_KERN_NAME_H_
