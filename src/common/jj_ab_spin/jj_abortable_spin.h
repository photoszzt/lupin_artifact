#ifndef JJ_ABORTABLE_SPIN_H_
#define JJ_ABORTABLE_SPIN_H_ 1

#ifndef JJ_AB_MAX_PROC_SHIFT
#error "need to define JJ_AB_MAX_PROC_SHIFT"
#endif

#define MINARR_MAX_PROCESS_SHIFT JJ_AB_MAX_PROC_SHIFT

#if defined(__KERNEL__) || defined(MODULE)
#include <asm/bug.h>
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stdbool.h>
#include <string.h>
#endif

#include "atomic_int_op.h"
#include "common_macro.h"
#include "jj_ab_cs_status.h"
#include "min_array/min_array.h"
#include "persist_cas.h"
#include "template.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The uint64_t variable is using single word persistent cas
 * from "Easy Lock-Free Indexing in Non-Volatile Memory"
 * But the max value is reduced to 63 bit max.
 *
 * For the go variable, the top bit is occuppied as negative. Need to use
 * flush-on-read for adr no flush on read for eadr:
 * https://www.intel.com/content/www/us/en/developer/articles/technical/eadr-new-opportunities-for-persistent-memory-applications.html.
 *
 * original jj lock contains the go array mainly for the DSM model. On CC model,
 * the go array can be eliminated and spin on the cs_status.
 */
#define jj_spinlock(max_proc_shift) struct JOIN(jj_spinlock, max_proc_shift)
/**
 * For mutex convention, the futex word should appear on the first of the data
 * structure
 */
jj_spinlock(JJ_AB_MAX_PROC_SHIFT) {
  _Atomic_uint64_t cs_status;
  _Atomic_uint64_t token;
  _Atomic_uint64_t seq;
  min_array(JJ_AB_MAX_PROC_SHIFT) min_arr;
#ifdef ORIG_JJ_LOCK
  _Atomic_int64_t go[MINARR_MAX_NUM_PROCESSES];
#endif
}
__attribute__((aligned(CACHELINE_SIZE)));

#ifdef ORIG_JJ_LOCK
#define INVALID_GO -1
static force_inline int64_t read_go(_Atomic_int64_t *go) {
  int64_t ret = load_int64_acquire(go);
/* flush-on-read for adr */
#if defined(CACHELINE_GRANULAR)
  shmem_output_cacheline(go, sizeof(int64_t));
#endif
  return ret;
}

static force_inline void write_go(_Atomic_int64_t *go, int64_t val) {
  store_int64_release(go, val);
#if defined(CACHELINE_GRANULAR)
  shmem_output_cacheline(go, sizeof(int64_t));
#endif
}

static force_inline bool atomic_cmpxchg_go(_Atomic_int64_t *go, int64_t *exp,
                                           int64_t desire) {
  bool ret = atomic_cmpxchg_int64(go, exp, desire);
#if defined(CACHELINE_GRANULAR)
  if (ret)
    shmem_output_cacheline(go, sizeof(int64_t));
#endif
  return ret;
}
#endif

#define jj_spinlock_abort_f JOIN(jj_spinlock_abort, JJ_AB_MAX_PROC_SHIFT)
#define jj_spinlock_promote_f JOIN(jj_spinlock_promote, JJ_AB_MAX_PROC_SHIFT)
#define jj_spinlock_tryenter_f JOIN(jj_spinlock_tryenter, JJ_AB_MAX_PROC_SHIFT)
#define jj_spinlock_enter_f JOIN(jj_spinlock_enter, JJ_AB_MAX_PROC_SHIFT)
#define jj_spinlock_exit_f JOIN(jj_spinlock_exit, JJ_AB_MAX_PROC_SHIFT)
#define jj_spinlock_init_f JOIN(jj_spinlock_init, JJ_AB_MAX_PROC_SHIFT)

bool jj_spinlock_abort_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                         uint16_t process_id) WARN_UNUSED_RESULT;
bool jj_spinlock_promote_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                           uint16_t process_id, bool flag,
                           uint64_t *peer_out) WARN_UNUSED_RESULT;
bool jj_spinlock_tryenter_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                            uint16_t process_id) WARN_UNUSED_RESULT;
bool jj_spinlock_enter_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                         uint16_t process_id,
                         bool (*abort_signal)(uint32_t)) WARN_UNUSED_RESULT;
bool jj_spinlock_exit_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                        uint16_t process_id,
                        uint64_t *promoted_peer) WARN_UNUSED_RESULT;
void jj_spinlock_init_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock);

#define jj_spinlock_recover_f JOIN(jj_spinlock_recover, JJ_AB_MAX_PROC_SHIFT)
static force_inline void
jj_spinlock_recover_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                      uint16_t process_id) {
  bool ret;
#ifdef ORIG_JJ_LOCK
  if (read_go(&lock->go[process_id]) != INVALID_GO) {
    ret = jj_spinlock_abort_f(lock, process_id);
    (void)ret;
  }
#else
  struct cs_status cs_status;
  read_decompose_cs_status(&lock->cs_status, &cs_status);
  if (cs_status.b && cs_status.s == process_id) {
    ret = jj_spinlock_abort_f(lock, process_id);
    (void)ret;
  }
#endif
}

#ifdef __cplusplus
}
#endif

#endif // JJ_ABORTABLE_SPIN_H_

#ifdef JJ_SPINLOCK_IMPL
#undef JJ_SPINLOCK_IMPL

#define MIN_ARR_IMPL
#include "min_array/min_array.h"

#ifdef __cplusplus
extern "C" {
#endif

bool jj_spinlock_promote_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                           uint16_t process_id, bool flag, uint64_t *peer_out) {
  uint64_t cur_css_state = read_cs_status(&lock->cs_status);
  struct cs_status cs_status;
  uint64_t peer;
  uint64_t exp_cs_status, desire_cs_status, m, token;
  bool promote_another = false;
#ifdef ORIG_JJ_LOCK
  int64_t g;
#endif

  decompose_cs_status(cur_css_state, &cs_status);
  peer = cs_status.s;
  // bool ret;
  // PRINT_INFO("[promote] cur css: 0x%lx, b: %lu, s: 0x%lx\n", cur_css_state,
  // cs_status.b, cs_status.s);

  if (!cs_status.b) {
    m = find_min_f(JJ_AB_MAX_PROC_SHIFT)(&lock->min_arr);
    token = get_counter(m);
    peer = get_process_id(m);
    // PRINT_INFO("find min got: %lx, token: %lx, peer: %lx\n", m, token, peer);
    if (token == COUNTER_INF && !flag) {
      if (peer_out) {
        *peer_out = 0;
      }
      return false;
    }
    if (token == COUNTER_INF) {
      peer = process_id;
    }
    exp_cs_status = compose_cs_status(false, cs_status.s);
    desire_cs_status = compose_cs_status(true, peer);

    // PRINT_INFO("[promote] exp css 0x%lx, desire css 0x%lx\n", exp_cs_status,
    // desire_cs_status);
    if (!atomic_cmpxchg_cs_status(&lock->cs_status, &exp_cs_status,
                                  desire_cs_status)) {
      // PRINT_INFO("[promote] css status is not expected, got 0x%lx\n",
      // exp_cs_status);
      if (peer_out) {
        *peer_out = 0;
      }
      promote_another = false;
    } else {
      if (peer_out) {
        *peer_out = peer;
      }
      promote_another = true;
    }
  }
#ifdef ORIG_JJ_LOCK
  /* s owns the critical section; peer is set to s */
  g = read_go(&lock->go[(uint16_t)peer]);
  // PRINT_INFO("g is %ld\n", g);
  if ((g == INVALID_GO) || (g == 0)) {
    return;
  }
  /* It's possible that peer is still busywaiting because it doesn't know it
   * owns the critical section */
  cs_status = read_decompose_cs_status(&lock->cs_status);
  // PRINT_INFO("recheck css state b: %lu, s: 0x%lx, g: %ld\n", cs_status.b,
  // cs_status.s, g);
  if (cs_status.b && cs_status.s == peer) {
    atomic_cmpxchg_go(&lock->go[(uint16_t)peer], &g, 0);
    // ret = atomic_cmpxchg_int64(&lock->go[peer], &g, 0);
    // if (ret) {
    //     PRINT_INFO("change peer %lu go to 0", peer);
    // } else {
    //     PRINT_INFO("unexpected g 0x%lx\n", g);
    // }
  }
#endif
  return promote_another;
}

bool jj_spinlock_abort_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                         uint16_t process_id) {
  struct cs_status cs;
  bool promote_another;
  min_arr_update_f(JJ_AB_MAX_PROC_SHIFT)(&lock->min_arr, process_id,
                                         default_process_state(process_id));
  promote_another = jj_spinlock_promote_f(lock, process_id, true, NULL);
  (void)promote_another;
  read_decompose_cs_status(&lock->cs_status, &cs);
  if (cs.b && cs.s == process_id) {
    /* already in CS */
    return false;
  }
#ifdef ORIG_JJ_LOCK
  write_go(&lock->go[process_id], INVALID_GO);
#endif
  return true;
}

bool jj_spinlock_tryenter_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                            uint16_t process_id) {
  uint64_t token, exp_token;
  uint64_t cur_cs_status = read_cs_status(&lock->cs_status);
  uint64_t process_state;
  bool promote_another;

  struct cs_status cs_status;
  decompose_cs_status(cur_cs_status, &cs_status);
  // PRINT_INFO("cur css status: %lx b: %lu, s: %lx\n", cur_cs_status,
  // cs_status.b, cs_status.s);
  if (cs_status.b && cs_status.s == process_id) {
    return true;
  }
  token = pcas_read_uint64(&lock->token);
  // PRINT_INFO("cur token: %lx, counter max: %lx\n", token, COUNTER_INF);
  /* token is cap by int64_t and 47 bit max */
  if (unlikely(token == COUNTER_INF - 1)) {
    PRINT_ERR("token counter overflow\n");
  }
  exp_token = token;
  persist_cas_uint64(&lock->token, &exp_token, token + 1);
#ifdef ORIG_JJ_LOCK
  write_go(&lock->[process_id], (int64_t)token);
#endif

  process_state = compose_process_state(token, process_id);
  // PRINT_INFO("cur process_id: %x, process_state: %lx\n", process_id,
  // process_state);
  min_arr_update_f(JJ_AB_MAX_PROC_SHIFT)(&lock->min_arr, process_id,
                                         process_state);
  promote_another = jj_spinlock_promote_f(lock, process_id, false, NULL);
  (void)promote_another;
#ifdef ORIG_JJ_LOCK
  /* await Go[process_id] == 0 || AbortSignal[process_id] */
  return load_int64_acquire(&lock->go[process_id]) == 0;
#else
  read_decompose_cs_status(&lock->cs_status, &cs_status);
  return cs_status.b && cs_status.s == process_id;
#endif
}

bool jj_spinlock_enter_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                         uint16_t process_id, bool (*abort_signal)(uint32_t)) {
  bool got_lock = jj_spinlock_tryenter_f(lock, process_id);
  uint32_t counter = 0;
  struct cs_status cs_st;
#ifdef ORIG_JJ_LOCK
  if (!got_lock) {
    while (load_int64_acquire(&lock->go[process_id]) != 0 &&
           !abort_signal(counter++)) {
      CPU_PAUSE();
    }
  }
  if (read_go(&lock->go[process_id]) == 0) {
    /* break out the while loop due to acquired the lock */
    return true;
  } else {
    /* break out the while loop due to abort */
    return !jj_spinlock_abort_f(lock, process_id);
  }
#else
  if (!got_lock) {
    while (true) {
      read_decompose_cs_status(&lock->cs_status, &cs_st);
      if ((cs_st.b && cs_st.s == process_id) || (abort_signal(counter++))) {
        break;
      }
      CPU_PAUSE();
    }
  }
  read_decompose_cs_status(&lock->cs_status, &cs_st);
  if (cs_st.b && cs_st.s == process_id) {
    /* break out the while loop due to acquired the lock */
    return true;
  } else {
    /* break out the while loop due to abort */
    return !jj_spinlock_abort_f(lock, process_id);
  }
#endif
}

/* unlock route; */
bool jj_spinlock_exit_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock,
                        uint16_t process_id, uint64_t *promoted_peer) {
  uint64_t s;
  bool promoted_another;
  min_arr_update_f(JJ_AB_MAX_PROC_SHIFT)(&lock->min_arr, process_id,
                                         default_process_state(process_id));
  s = pcas_read_uint64(&lock->seq);
  if (unlikely(s == COUNTER_INF - 1)) {
    PRINT_ERR("seq counter overflow\n");
  }
  persist_atomic_write_uint64(&lock->seq, s + 1);
  write_cs_status(&lock->cs_status, compose_cs_status(false, s + 1));
  promoted_another =
      jj_spinlock_promote_f(lock, process_id, false, promoted_peer);
#ifdef ORIG_JJ_LOCK
  /* Before setting go back to INVALID_GO, it's in critical section */
  write_go(&lock->[process_id], INVALID_GO);
#endif
  return promoted_another;
}

void jj_spinlock_init_f(jj_spinlock(JJ_AB_MAX_PROC_SHIFT) * lock) {
#ifdef ORIG_JJ_LOCK
  int i = 0;
#endif
  lock->cs_status = compose_cs_status(false, 1);
  lock->seq = 1;
  lock->token = 1;
#ifdef ORIG_JJ_LOCK
  for (i = 0; i < MAX_NUM_PROCESSES; i++) {
    write_go(&lock->go[(uint16_t)i], INVALID_GO);
  }
#endif
  init_min_array_f(JJ_AB_MAX_PROC_SHIFT)(&lock->min_arr);
  shmem_output_cacheline((uint8_t *)lock,
                         sizeof(jj_spinlock(JJ_AB_MAX_PROC_SHIFT)));
}

#ifdef __cplusplus
}
#endif

#endif // JJ_SPINLOCK_IMPL
