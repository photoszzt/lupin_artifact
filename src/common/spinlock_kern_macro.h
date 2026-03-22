#ifndef SPINLOCK_KERN_MACRO_H_
#define SPINLOCK_KERN_MACRO_H_ 1

#include "spinlock_kern_name.h"
#include <linux/bottom_half.h>
#include <linux/compiler_attributes.h>
#include <linux/compiler_types.h>
#include <linux/irqflags.h>
#include <linux/preempt.h>
#include <linux/typecheck.h>

static inline bool spin_no_abort(uint32_t counter) {
  (void)counter;
  return false;
}

#define def_slock_irqsave(name, lock_type)                                     \
  static inline unsigned long slock_irqsave(name)(struct lock_type * lock,     \
                                                  uint16_t machine_id) {       \
    unsigned long flags;                                                       \
    local_irq_save(flags);                                                     \
    preempt_disable();                                                         \
    do_slock(name)(lock, machine_id);                                          \
    return flags;                                                              \
  }

#define def_slock_irqsave_with_gen(name, lock_type)                            \
  static inline unsigned long slock_irqsave_gen(name)(                         \
      struct lock_type * lock, uint16_t machine_id, uint32_t gen_id) {         \
    unsigned long flags;                                                       \
    local_irq_save(flags);                                                     \
    preempt_disable();                                                         \
    do_slock_gen(name)(lock, machine_id, gen_id);                              \
    return flags;                                                              \
  }

#define def_slock_irq(name, lock_type)                                         \
  static inline void slock_irq(name)(struct lock_type * lock,                  \
                                     uint16_t machine_id) {                    \
    local_irq_disable();                                                       \
    preempt_disable();                                                         \
    do_slock(name)(lock, machine_id);                                          \
  }

#define def_slock_irq_with_gen(name, lock_type)                                \
  static inline void slock_irq_gen(name)(                                      \
      struct lock_type * lock, uint16_t machine_id, uint32_t gen_id) {         \
    local_irq_disable();                                                       \
    preempt_disable();                                                         \
    do_slock_gen(name)(lock, machine_id, gen_id);                              \
  }

#define def_slock_bh(name, lock_type)                                          \
  static inline void slock_bh(name)(struct lock_type * lock,                   \
                                    uint16_t machine_id) {                     \
    __local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);                      \
    do_slock(name)(lock, machine_id);                                          \
  }

#define def_slock_bh_with_gen(name, lock_type)                                 \
  static inline void slock_bh_gen(name)(                                       \
      struct lock_type * lock, uint16_t machine_id, uint32_t gen_id) {         \
    __local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);                      \
    do_slock_gen(name)(lock, machine_id, gen_id);                              \
  }

#define def_slock(name, lock_type)                                             \
  static inline void slock(name)(struct lock_type * lock,                      \
                                 uint16_t machine_id) {                        \
    preempt_disable();                                                         \
    do_slock(name)(lock, machine_id);                                          \
  }

#define def_slock_with_gen(name, lock_type)                                    \
  static inline void slock_gen(name)(struct lock_type * lock,                  \
                                     uint16_t machine_id, uint32_t gen_id) {   \
    preempt_disable();                                                         \
    do_slock_gen(name)(lock, machine_id, gen_id);                              \
  }

#define def_sunlock(name, lock_type)                                           \
  static inline void sunlock(name)(struct lock_type * lock,                    \
                                   uint16_t machine_id) {                      \
    do_sunlock(name)(lock, machine_id);                                        \
    preempt_enable();                                                          \
  }

#define def_sunlock_irqrestore(name, lock_type)                                \
  static inline void sunlock_irqrestore(name)(                                 \
      struct lock_type * lock, uint16_t machine_id, unsigned long flags) {     \
    do_sunlock(name)(lock, machine_id);                                        \
    local_irq_restore(flags);                                                  \
    preempt_enable();                                                          \
  }

#define def_sunlock_irq(name, lock_type)                                       \
  static inline void sunlock_irq(name)(struct lock_type * lock,                \
                                       uint16_t machine_id) {                  \
    do_sunlock(name)(lock, machine_id);                                        \
    local_irq_enable();                                                        \
    preempt_enable();                                                          \
  }

#define def_sunlock_bh(name, lock_type)                                        \
  static inline void sunlock_bh(name)(struct lock_type * lock,                 \
                                      uint16_t machine_id) {                   \
    do_sunlock(name)(lock, machine_id);                                        \
    __local_bh_enable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);                       \
  }

#endif // SPINLOCK_KERN_MACRO_H_
