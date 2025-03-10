#ifndef ATOMIC_INT_OP_H_
#define ATOMIC_INT_OP_H_

#if defined(__KERNEL__) || defined(MODULE)
#include <asm/barrier.h>
#include <asm/cmpxchg.h>
#include <linux/types.h>
#else
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "crash_here.hpp"

#endif

#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

/* stackoverflow:
 * https://stackoverflow.com/questions/55008994/most-efficient-popcount-on-uint128-t
 */
__extension__ typedef unsigned __int128 uint128_t;
union Uint128 {
  __uint128_t uu128;
  uint64_t uu64[2];
};

#if defined(__KERNEL__) || defined(MODULE)
#define atomic_load_acquire_op(name, type)                                     \
  static force_inline type load_##name##_acquire(const _Atomic type *addr) {   \
    return smp_load_acquire(addr);                                             \
  }

#define atomic_load_relaxed_op(name, type)                                     \
  static force_inline type load_##name##_relaxed(const _Atomic type *addr) {   \
    return READ_ONCE(*addr);                                                   \
  }

#define atomic_store_release_op(name, type)                                    \
  static force_inline void store_##name##_release(_Atomic type *addr,          \
                                                  type val) {                  \
    smp_store_release(addr, val);                                              \
  }

#define atomic_store_relaxed_op(name, type)                                    \
  static force_inline void store_##name##_relaxed(_Atomic type *addr,          \
                                                  type val) {                  \
    WRITE_ONCE(*addr, val);                                                    \
  }

#define atomic_cmpxchg_op(name, type)                                          \
  static force_inline bool atomic_cmpxchg_##name(_Atomic type *addr,           \
                                                 type *exp, type desire) {     \
    return try_cmpxchg(addr, exp, desire);                                     \
  }

#define atomic_faa_op(name, type)                                              \
  static force_inline type atomic_fetch_add_##name(_Atomic type *addr,         \
                                                   type val) {                 \
    return xadd(addr, val);                                                    \
  }

#define atomic_cmpxchg_op_acqrel_rlx(name, type)                               \
  static force_inline bool atomic_cmpxchg_acqrel_rlx_##name(                   \
      _Atomic type *addr, type *exp, type desire) {                            \
    return try_cmpxchg(addr, exp, desire);                                     \
  }

#define atomic_cmpxchg_op_acq_rlx(name, type)                                  \
  static force_inline bool atomic_cmpxchg_acq_rlx_##name(                      \
      _Atomic type *addr, type *exp, type desire) {                            \
    return try_cmpxchg_acquire(addr, exp, desire);                             \
  }

#define atomic_cmpxchg_op_rel_rlx(name, type)                                  \
  static force_inline bool atomic_cmpxchg_rel_rlx_##name(                      \
      _Atomic type *addr, type *exp, type desire) {                            \
    return try_cmpxchg_release(addr, exp, desire);                             \
  }

#define atomic_cmpxchg_op_rlx_rlx(name, type)                                  \
  static force_inline bool atomic_cmpxchg_rlx_rlx_##name(                      \
      _Atomic type *addr, type *exp, type desire) {                            \
    return try_cmpxchg_relaxed(addr, exp, desire);                             \
  }

/* linux kernel uses gnu11 */
#define _Atomic_uint64_t _Atomic uint64_t
#define _Atomic_int64_t _Atomic int64_t
#define _Atomic_uint32_t _Atomic uint32_t
#define _Atomic_uint16_t _Atomic uint16_t
#define _Atomic_uint8_t _Atomic uint8_t

#else
#define atomic_load_acquire_op(name, type)                                     \
  static force_inline type load_##name##_acquire(const _Atomic(type) *addr) {  \
    crash_here_rand(-1);                                                       \
    return atomic_load_explicit(addr, memory_order_acquire);                   \
  }

#define atomic_load_relaxed_op(name, type)                                     \
  static force_inline type load_##name##_relaxed(const _Atomic(type) *addr) {  \
    crash_here_rand(-1);                                                       \
    return atomic_load_explicit(addr, memory_order_relaxed);                   \
  }

#define atomic_store_relaxed_op(name, type)                                    \
  static force_inline void store_##name##_relaxed(_Atomic(type) *addr,         \
                                                  type val) {                  \
    crash_here_rand(-1);                                                       \
    atomic_store_explicit(addr, val, memory_order_relaxed);                    \
  }

#define atomic_store_release_op(name, type)                                    \
  static force_inline void store_##name##_release(_Atomic(type) *addr,         \
                                                  type val) {                  \
    crash_here_rand(-1);                                                       \
    atomic_store_explicit(addr, val, memory_order_release);                    \
  }

#define atomic_cmpxchg_op(name, type)                                          \
  static force_inline bool atomic_cmpxchg_##name(_Atomic(type) *addr,          \
                                                 type *exp, type desire) {     \
    crash_here_rand(-1);                                                       \
    return atomic_compare_exchange_strong_explicit(                            \
        addr, exp, desire, memory_order_acq_rel, memory_order_acquire);        \
  }

#define atomic_cmpxchg_op_acqrel_rlx(name, type)                               \
  static force_inline bool atomic_cmpxchg_acqrel_rlx_##name(                   \
      _Atomic(type) *addr, type *exp, type desire) {                           \
    crash_here_rand(-1);                                                       \
    return atomic_compare_exchange_strong_explicit(                            \
        addr, exp, desire, memory_order_acq_rel, memory_order_relaxed);        \
  }

#define atomic_cmpxchg_op_acq_rlx(name, type)                                  \
  static force_inline bool atomic_cmpxchg_acq_rlx_##name(                      \
      _Atomic(type) *addr, type *exp, type desire) {                           \
    crash_here_rand(-1);                                                       \
    return atomic_compare_exchange_strong_explicit(                            \
        addr, exp, desire, memory_order_acquire, memory_order_relaxed);        \
  }

#define atomic_cmpxchg_op_rel_rlx(name, type)                                  \
  static force_inline bool atomic_cmpxchg_rel_rlx_##name(                      \
      _Atomic(type) *addr, type *exp, type desire) {                           \
    crash_here_rand(-1);                                                       \
    return atomic_compare_exchange_strong_explicit(                            \
        addr, exp, desire, memory_order_release, memory_order_relaxed);        \
  }

#define atomic_cmpxchg_op_rlx_rlx(name, type)                                  \
  static force_inline bool atomic_cmpxchg_rlx_rlx_##name(                      \
      _Atomic(type) *addr, type *exp, type desire) {                           \
    crash_here_rand(-1);                                                       \
    return atomic_compare_exchange_strong_explicit(                            \
        addr, exp, desire, memory_order_relaxed, memory_order_relaxed);        \
  }

#define atomic_faa_op(name, type)                                              \
  static force_inline type atomic_fetch_add_##name(_Atomic(type) *addr,        \
                                                   type val) {                 \
    crash_here_rand(-1);                                                       \
    return atomic_fetch_add_explicit(addr, val, memory_order_acq_rel);         \
  }

static force_inline uint128_t load_uint128_acquire(volatile uint128_t *addr) {
  crash_here_rand(-1);
  return __atomic_load_n(addr, __ATOMIC_ACQUIRE);
}

static force_inline void store_uint128_release(volatile uint128_t *addr,
                                               uint128_t val) {
  crash_here_rand(-1);
  __atomic_store_n(addr, val, __ATOMIC_RELEASE);
}

static force_inline bool atomic_cmpxchg_uint128(volatile uint128_t *dest,
                                                uint128_t *exp,
                                                uint128_t desire) {
  crash_here_rand(-1);
  return __atomic_compare_exchange_n(dest, exp, desire, false, __ATOMIC_ACQ_REL,
                                     __ATOMIC_ACQUIRE);
}

/* Only c23/c++23 recognize such declaration */
#define _Atomic_uint64_t _Atomic(uint64_t)
#define _Atomic_int64_t _Atomic(int64_t)
#define _Atomic_uint32_t _Atomic(uint32_t)
#define _Atomic_uint16_t _Atomic(uint16_t)
#define _Atomic_uint8_t _Atomic(uint8_t)

#endif

static inline uint8_t popcnt_u128(uint128_t n) {
  const union Uint128 n_u = {.uu128 = n};
  const uint8_t cnt_a = (uint8_t)__builtin_popcountll(n_u.uu64[0]);
  const uint8_t cnt_b = (uint8_t)__builtin_popcountll(n_u.uu64[1]);
  return cnt_a + cnt_b;
}

// clang-format off
atomic_load_relaxed_op(uint64, uint64_t)
atomic_load_relaxed_op(int64, int64_t)
atomic_load_relaxed_op(uint32, uint32_t)

atomic_load_acquire_op(uint64, uint64_t)
atomic_load_acquire_op(int64, int64_t)
atomic_load_acquire_op(uint32, uint32_t)
atomic_load_acquire_op(uint16, uint16_t)
atomic_load_acquire_op(uint8, uint8_t)
atomic_store_release_op(uint64, uint64_t)
atomic_store_release_op(int64, int64_t)
atomic_store_release_op(uint32, uint32_t)
atomic_store_release_op(uint16, uint16_t)
atomic_store_release_op(uint8, uint8_t)
atomic_store_relaxed_op(uint64, uint64_t)
atomic_store_relaxed_op(int64, int64_t)
atomic_store_relaxed_op(uint32, uint32_t)

atomic_cmpxchg_op(uint64, uint64_t)
atomic_cmpxchg_op(int64, int64_t)
atomic_cmpxchg_op(uint32, uint32_t)
atomic_cmpxchg_op(uint16, uint16_t)
atomic_cmpxchg_op(uint8, uint8_t)
atomic_cmpxchg_op_acq_rlx(uint64, uint64_t)
atomic_cmpxchg_op_acq_rlx(uint32, uint32_t)
atomic_cmpxchg_op_rel_rlx(uint64, uint64_t)
atomic_cmpxchg_op_rel_rlx(uint32, uint32_t)
atomic_cmpxchg_op_acqrel_rlx(uint64, uint64_t)
atomic_cmpxchg_op_acqrel_rlx(uint32, uint32_t)
atomic_cmpxchg_op_rlx_rlx(uint64, uint64_t)
atomic_cmpxchg_op_rlx_rlx(uint32, uint32_t)
atomic_faa_op(uint64, uint64_t)
atomic_faa_op(uint32, uint32_t)
atomic_faa_op(uint16, uint16_t)
atomic_faa_op(uint8, uint8_t)
// clang-format on

#ifdef __cplusplus
}
#endif

#endif // ATOMIC_INT_OP_H_
