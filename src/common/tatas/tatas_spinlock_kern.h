#ifndef TATAS_SPINLOCK_KERN_H_
#define TATAS_SPINLOCK_KERN_H_

#include "process_id.h"
#include "spinlock_kern_macro.h"
#include "tatas_spin.h"
#include <linux/types.h>

#include "spinlock_kern_name.h"
// the recover routine assumes that the same cpu is calling the recover
static inline void ttas_lock_kern(struct ttas *lock, uint16_t machine_id) {
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

static inline void ttas_lock_with_gen_kern(struct ttas *lock,
                                           uint16_t machine_id,
                                           uint32_t gen_id) {
  uint16_t process_id;
  bool ret;
  BUG_ON(machine_id < 1);
  ret = get_process_identifier(machine_id, &process_id);
  BUG_ON(!ret);
  if (ttas_recover_with_gen(lock, process_id, gen_id)) {
    return;
  }
  ret = ttas_enter_with_gen(lock, process_id, gen_id, spin_no_abort);
  BUG_ON(!ret);
}

static inline bool ttas_trylock_with_gen_kern(struct ttas *lock,
                                              uint16_t machine_id,
                                              uint32_t gen_id) {
  uint16_t process_id;
  bool ret;
  BUG_ON(machine_id < 1);
  ret = get_process_identifier(machine_id, &process_id);
  BUG_ON(!ret);
  if (ttas_recover_with_gen(lock, process_id, gen_id)) {
    return true;
  }
  return ttas_try_enter_with_gen(lock, process_id, gen_id);
}

static inline void ttas_unlock_kern(struct ttas *lock, uint16_t machine_id) {
  uint16_t process_id;
  bool ret;
  BUG_ON(machine_id < 1);
  ret = get_process_identifier(machine_id, &process_id);
  BUG_ON(!ret);
  ttas_exit(lock, process_id);
}

static inline void do_ttas_lock(struct ttas *lock, uint16_t machine_id)
    __acquires(lock) {
  __acquire(lock);
  ttas_lock_kern(lock, machine_id);
  mmiowb_spin_lock();
}

static inline void do_ttas_lock_with_gen(struct ttas *lock, uint16_t machine_id,
                                         uint32_t gen_id) __acquires(lock) {
  __acquire(lock);
  ttas_lock_with_gen_kern(lock, machine_id, gen_id);
  mmiowb_spin_lock();
}

static inline void do_ttas_unlock(struct ttas *lock, uint16_t machine_id)
    __releases(lock) {
  mmiowb_spin_unlock();
  ttas_unlock_kern(lock, machine_id);
  __release(lock);
}

def_slock_irqsave(ttas, ttas);

def_slock_irqsave_with_gen(ttas, ttas);

#define ttas_lock_irqsave(lock, machine_id, flags)                             \
  do {                                                                         \
    typecheck(unsigned long, flags);                                           \
    flags = slock_irqsave(ttas)(lock, machine_id);                             \
  } while (0)

#define ttas_lock_with_gen_irqsave(lock, machine_id, gen_id, flags)            \
  do {                                                                         \
    typecheck(unsigned long, flags);                                           \
    flags = slock_irqsave(ttas)(lock, machine_id, gen_id);                     \
  } while (0)

def_slock_irq(ttas, ttas);

def_slock_irq_with_gen(ttas, ttas);

def_slock_bh(ttas, ttas);

def_slock_bh_with_gen(ttas, ttas);

/**
 * In linux kernel, the process id is the CPU id in the CXL memory pool.
 * The cpu id is calculated inside the lock
 */
def_slock(ttas, ttas);
def_slock_with_gen(ttas, ttas);

def_sunlock(ttas, ttas);
def_sunlock_irqrestore(ttas, ttas);
def_sunlock_irq(ttas, ttas);
def_sunlock_bh(ttas, ttas);

static inline enum transfer_st ttas_lock_recover_kern(struct ttas *lock,
                                                      u16 old_machine_id,
                                                      u16 new_machine_id,
                                                      u32 newm_gen_id) {
  u16 newm_process_id, old_process_id, old_machine_id_on_lock;
  bool ret;
  u64 owner_gen, new_lock_val, lock_state;
  struct cs_status st;

  BUG_ON(new_machine_id < 1);
  ret = get_process_identifier(new_machine_id, &newm_process_id);
  BUG_ON(!ret);
  owner_gen = compose_owner_gen(newm_process_id, newm_gen_id);
  new_lock_val = compose_cs_status(true, owner_gen);

  lock_state = pcas_read_uint64(&lock->csst);
  decompose_cs_status(lock_state, &st);
  old_process_id = GET_OWNER_ID(st.s);
  old_machine_id_on_lock = old_process_id / nr_cpu_ids + 1;
  /*
  PRINT_INFO("old process id: %u, old_machine_id_on_lock: %u, new_machine_id: "
             "%u, newm_process_id: %u, newm_gen_id: %u, nr_cpu_ids: %u\n",
             old_process_id, old_machine_id_on_lock, new_machine_id,
             newm_process_id, newm_gen_id, nr_cpu_ids);
  */

  // 1. it's the same cpu from the same machine running recover
  // try to go back to critical section by updating the lock state
  // 2. it's unlocked
  // 3. the owner is old_machine_id but a different cpu
  if (old_process_id == newm_process_id || lock_state == UNLOCKED ||
      old_machine_id_on_lock == old_machine_id) {
    ret = persist_cas_uint64(&lock->csst, &lock_state, new_lock_val);
    if (ret) {
      return TRANSFERED;
    } else {
      // some os might already help
      return FAILED;
    }
  } else {
    // some os might already help
    return FAILED;
  }
}

static inline enum transfer_st do_ttas_lock_recover(struct ttas *lock,
                                                    u16 old_machine_id,
                                                    u16 new_machine_id,
                                                    u32 newm_gen_id) {
  enum transfer_st st =
      ttas_lock_recover_kern(lock, old_machine_id, new_machine_id, newm_gen_id);
  if (st == TRANSFERED) {
    mmiowb_spin_lock();
  }
  return st;
}

static inline enum transfer_st ttas_lock_recover(struct ttas *lock,
                                                 u16 old_machine_id,
                                                 u16 new_machine_id,
                                                 u32 newm_gen_id) {
  preempt_disable();
  return do_ttas_lock_recover(lock, old_machine_id, new_machine_id,
                              newm_gen_id);
}

static inline unsigned long _ttas_lock_recover_irqsave(struct ttas *lock,
                                                       u16 old_machine_id,
                                                       u16 new_machine_id,
                                                       u32 newm_gen_id,
                                                       enum transfer_st *st) {
  unsigned long flags;
  local_irq_save(flags);
  preempt_disable();
  *st = do_ttas_lock_recover(lock, old_machine_id, new_machine_id, newm_gen_id);
  return flags;
}

#define ttas_lock_recover_irqsave(lock, old_machine_id, new_machine_id,        \
                                  newm_gen_id, st, flags)                      \
  do {                                                                         \
    typecheck(unsigned long, flags);                                           \
    flags = _ttas_lock_recover_irqsave(lock, old_machine_id, new_machine_id,   \
                                       newm_gen_id, st);                       \
  } while (0)

static inline enum transfer_st ttas_lock_recover_irq(struct ttas *lock,
                                                     u16 old_machine_id,
                                                     u16 new_machine_id,
                                                     u32 newm_gen_id) {
  local_irq_disable();
  preempt_disable();
  return do_ttas_lock_recover(lock, old_machine_id, new_machine_id,
                              newm_gen_id);
}

static inline enum transfer_st ttas_lock_recover_bh(struct ttas *lock,
                                                    u16 old_machine_id,
                                                    u16 new_machine_id,
                                                    u32 newm_gen_id) {
  __local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
  return do_ttas_lock_recover(lock, old_machine_id, new_machine_id,
                              newm_gen_id);
}

static inline void done_ttas_lock_recover(struct ttas *lock, u16 machine_id,
                                          enum transfer_st st) {
  if (st == TRANSFERED) {
    do_ttas_unlock(lock, machine_id);
  }
  preempt_enable();
}

static inline void done_ttas_lock_recover_irqrestore(struct ttas *lock,
                                                     u16 machine_id,
                                                     enum transfer_st st,
                                                     unsigned long flags) {
  if (st == TRANSFERED) {
    do_ttas_unlock(lock, machine_id);
  }
  local_irq_restore(flags);
  preempt_enable();
}

static inline void done_ttas_lock_recover_irq(struct ttas *lock, u16 machine_id,
                                              enum transfer_st st) {
  if (st == TRANSFERED) {
    do_ttas_unlock(lock, machine_id);
  }
  local_irq_enable();
  preempt_enable();
}

static inline void done_ttas_lock_recover_bh(struct ttas *lock, u16 machine_id,
                                             enum transfer_st st) {
  if (st == TRANSFERED) {
    do_ttas_unlock(lock, machine_id);
  }
  __local_bh_enable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);
}

#endif // TATAS_SPINLOCK_KERN_H_
