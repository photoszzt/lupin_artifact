#ifndef TATAS_LOCK_H_
#define TATAS_LOCK_H_

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "atomic_int_op.h"
#include "common_macro.h"
#include "jj_ab_spin/jj_ab_cs_status.h"
#include "persist_cas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * if it's locked, csst is (1, owner_id)
 * if it's not locked, csst is 0
 *
 * With gen, if it's locked, csst is (1, owner_id, gen)
 * owner id 16 bit, gen 32 bits
 */
#define UNLOCKED ((uint64_t)0)
struct ttas {
  _Atomic_uint64_t csst;
};
#define OWNER_GEN_MASK ((uint64_t)0xffffffff)
#define CLEAR_OWNER_GEN ((uint64_t)~GEN_MASK)
#define GET_OWNER_GEN(x) ((uint64_t)(x & OWNER_GEN_MASK))
#define OWNER_ID_MASK ((uint64_t)(0xffff) << 32)
#define GET_OWNER_ID(x) (uint64_t)(((uint64_t)x & OWNER_ID_MASK) >> 32UL)

static inline uint64_t compose_owner_gen(uint16_t proc_id, uint32_t gen) {
  uint64_t s = 0;
  s = (((uint64_t)proc_id) << 32) | gen;
  return s;
}

static inline bool ttas_enter_with_gen(struct ttas *lock, uint16_t proc_id,
                                       uint32_t gen,
                                       bool (*abort_signal)(uint32_t)) {
  uint64_t owner_gen = compose_owner_gen(proc_id, gen);
  uint64_t lock_val = compose_cs_status(true, owner_gen);
  uint32_t counter = 0;
  uint64_t exp = UNLOCKED;
  while (1) {
    exp = UNLOCKED;
    while (pcas_read_uint64(&lock->csst) != UNLOCKED) {
      if (abort_signal(counter++)) {
        return false;
      }
      CPU_PAUSE();
    }
    if (persist_cas_uint64(&lock->csst, &exp, lock_val)) {
      return true;
    }
  }
}

static inline bool ttas_enter(struct ttas *lock, uint16_t proc_id,
                              bool (*abort_signal)(uint32_t)) {
  uint64_t lock_val = compose_cs_status(true, proc_id);
  uint32_t counter = 0;
  uint64_t exp = UNLOCKED;
  while (1) {
    exp = UNLOCKED;
    while (pcas_read_uint64(&lock->csst) != UNLOCKED) {
      if (abort_signal(counter++)) {
        return false;
      }
      CPU_PAUSE();
    }
    if (persist_cas_uint64(&lock->csst, &exp, lock_val)) {
      return true;
    }
  }
}

static inline void ttas_exit(struct ttas *lock, uint16_t proc_id) {
  (void)proc_id;
  persist_atomic_write_uint64(&lock->csst, UNLOCKED);
}

static inline bool ttas_try_enter(struct ttas *lock, uint16_t proc_id) {
  uint64_t lock_val = compose_cs_status(true, proc_id);
  uint64_t exp = UNLOCKED;

  return persist_cas_uint64(&lock->csst, &exp, lock_val);
}

static inline bool ttas_try_enter_with_gen(struct ttas *lock, uint16_t proc_id,
                                           uint32_t gen) {
  uint64_t owner_gen = compose_owner_gen(proc_id, gen);
  uint64_t lock_val = compose_cs_status(true, owner_gen);
  uint64_t exp = UNLOCKED;

  return persist_cas_uint64(&lock->csst, &exp, lock_val);
}

/**
 * returns true if the proc_id is already in critical section
 * returns false otherwise
 */
static inline bool ttas_recover(struct ttas *lock, uint16_t proc_id) {
  uint64_t lock_val = compose_cs_status(true, proc_id);
  return pcas_read_uint64(&lock->csst) == lock_val;
}

static inline bool ttas_recover_with_gen(struct ttas *lock, uint16_t proc_id,
                                         uint32_t gen) {
  uint64_t owner_gen = compose_owner_gen(proc_id, gen);
  uint64_t lock_val = compose_cs_status(true, owner_gen);
  return pcas_read_uint64(&lock->csst) == lock_val;
}

static inline void ttas_free_if_locked(struct ttas *lock, uint16_t proc_id) {
  bool locked = ttas_recover(lock, proc_id);
  if (locked) {
    ttas_exit(lock, proc_id);
  }
}

static inline void ttas_free_wth_gen_if_locked(struct ttas *lock,
                                               uint16_t proc_id, uint32_t gen) {
  bool locked = ttas_recover_with_gen(lock, proc_id, gen);
  if (locked) {
    ttas_exit(lock, proc_id);
  }
}

enum transfer_st {
  UNLOCK,
  TRANSFERED,
  FAILED,
};

static inline enum transfer_st
ttas_lock_transfer(struct ttas *lock, uint16_t exp_id, uint16_t change_id) {
  uint64_t lock_st, change_lock_val;
  struct cs_status st;
  bool ret;
  lock_st = pcas_read_uint64(&lock->csst);
  if (lock_st == UNLOCKED) {
    return UNLOCK;
  }
  decompose_cs_status(lock_st, &st);
  if (st.s != exp_id) {
    return FAILED;
  }
  change_lock_val = compose_cs_status(true, change_id);
  ret = persist_cas_uint64(&lock->csst, &lock_st, change_lock_val);
  if (ret) {
    return TRANSFERED;
  } else {
    return FAILED;
  }
}

#ifdef __cplusplus
}
#endif

#endif // TATAS_LOCK_H_
