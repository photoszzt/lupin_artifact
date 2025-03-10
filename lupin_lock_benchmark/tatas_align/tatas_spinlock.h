#ifndef TATAS_SPINLOCK_H_
#define TATAS_SPINLOCK_H_

#include "persist_cas.h"
#include "tatas_spin.h"

static force_inline void ttas_init(struct ttas* lock) {
  persist_atomic_write_uint64(&lock->csst, UNLOCKED);
}

#if defined(__KERNEL__) || defined(MODULE)
#include "tatas_spinlock_kern.h"
#else  // user level

#include "spinlock_kern_name.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "common_macro.h"

static bool spin_no_abort(uint32_t counter) {
  (void)counter;
  return false;
}

static WARN_UNUSED_RESULT force_inline uint64_t get_lock_st(struct ttas* lock) {
  return pcas_read_uint64(&lock->csst);
}

static force_inline void ttas_lock(struct ttas* lock, uint16_t proc_id) {
  bool ret;
  if (ttas_recover(lock, proc_id)) {
    return;
  }
  ret = ttas_enter(lock, proc_id, spin_no_abort);
  BUG_ON(!ret);
}

static force_inline void ttas_unlock(struct ttas* lock, uint16_t proc_id) {
  ttas_exit(lock, proc_id);
}

static force_inline bool ttas_trylock(struct ttas* lock, uint16_t proc_id) {
  if (ttas_recover(lock, proc_id)) {
    return true;
  }
  return ttas_try_enter(lock, proc_id);
}

#define ttas_lock_irqsave(lock, machine_id, flags) \
  (void)flags;                                     \
  slock(ttas)(lock, machine_id)

static force_inline void ttas_lock_bh(struct ttas* lock, uint16_t proc_id) {
  ttas_lock(lock, proc_id);
}

static force_inline void ttas_lock_irq(struct ttas* lock, uint16_t proc_id) {
  ttas_lock(lock, proc_id);
}

static force_inline void ttas_unlock_irq(struct ttas* lock, uint16_t proc_id) {
  ttas_unlock(lock, proc_id);
}

static force_inline void ttas_unlock_irqrestore(struct ttas* lock,
                                                uint16_t proc_id,
                                                unsigned long flags) {
  (void)flags;
  ttas_unlock(lock, proc_id);
}

static force_inline void ttas_unlock_bh(struct ttas* lock, uint16_t proc_id) {
  ttas_unlock(lock, proc_id);
}

#ifdef __cplusplus
}
#endif

#endif

#endif  // TATAS_SPINLOCK_H_
