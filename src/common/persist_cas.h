#ifndef PERSIST_CAS_H_
#define PERSIST_CAS_H_

#include "atomic_int_op.h"
#include "output_cacheline.h"

// Persistent single word CAS from Easy Lock-Free Indexing in Non-Volatile
// Memory https://www2.cs.sfu.ca/~tzwang/pmwcas.pdf

/* 8 bit pcas */
static const uint8_t DIRTY_8 = ((uint8_t)1 << 7);
static const uint8_t UINT7_MAX = (((uint8_t)1 << 7) - 1);
#define CLEAR_DIRTY_8 ((uint8_t)(0xff & ~DIRTY_8))

static force_inline uint8_t pcas_read_uint8(_Atomic_uint8_t *addr) {
  uint8_t val = load_uint8_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  uint8_t exp;
  if (val & DIRTY_8) {
    shmem_output_cacheline(addr, sizeof(uint8_t));
    exp = val;
    atomic_cmpxchg_uint8(addr, &exp, exp & CLEAR_DIRTY_8);
  }
  return val & CLEAR_DIRTY_8;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline uint8_t pcas_read_noflush_uint8(_Atomic_uint8_t *addr) {
  uint8_t val = load_uint8_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  return val & CLEAR_DIRTY_8;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline void persist_atomic_write_uint8(_Atomic_uint8_t *addr,
                                                    uint8_t val) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  store_uint8_release(addr, val);
#elif defined(CACHELINE_GRANULAR)
  store_uint8_release(addr, val | DIRTY_8);
  shmem_output_cacheline(addr, sizeof(uint8_t));
#else
  (void)addr;
  (void)val;
#error unrecognize persist granularity
#endif
}

static force_inline bool persist_cas_uint8(_Atomic_uint8_t *addr, uint8_t *exp,
                                           uint8_t desire) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  return atomic_cmpxchg_uint8(addr, exp, desire);
#elif defined(CACHELINE_GRANULAR)
  pcas_read_uint8(addr);
  return atomic_cmpxchg_uint8(addr, exp, desire | DIRTY_8);
#else
  (void)addr;
  (void)exp;
#error unrecognize persist granularity
#endif
}

/* 16 bit pcas */
static const uint16_t DIRTY_16 = ((uint16_t)1 << 15);
static const uint16_t UINT15_MAX = (((uint16_t)1 << 15) - 1);
#define CLEAR_DIRTY_16 ((uint16_t)(0xffff & ~DIRTY_16))

static force_inline uint16_t pcas_read_uint16(_Atomic_uint16_t *addr) {
  uint16_t val = load_uint16_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  uint16_t exp;
  if (val & DIRTY_16) {
    shmem_output_cacheline(addr, sizeof(uint16_t));
    exp = val;
    atomic_cmpxchg_uint16(addr, &exp, exp & CLEAR_DIRTY_16);
  }
  return val & CLEAR_DIRTY_16;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline uint16_t pcas_read_noflush_uint16(_Atomic_uint16_t *addr) {
  uint16_t val = load_uint16_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  return val & CLEAR_DIRTY_16;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline void persist_atomic_write_uint16(_Atomic_uint16_t *addr,
                                                     uint16_t val) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  store_uint16_release(addr, val);
#elif defined(CACHELINE_GRANULAR)
  store_uint16_release(addr, val | DIRTY_16);
  shmem_output_cacheline(addr, sizeof(uint16_t));
#else
  (void)addr;
  (void)val;
#error unrecognize persist granularity
#endif
}

static force_inline bool persist_cas_uint16(_Atomic_uint16_t *addr,
                                            uint16_t *exp, uint16_t desire) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  return atomic_cmpxchg_uint16(addr, exp, desire);
#elif defined(CACHELINE_GRANULAR)
  pcas_read_uint16(addr);
  return atomic_cmpxchg_uint16(addr, exp, desire | DIRTY_16);
#else
  (void)addr;
  (void)exp;
  (void)desire;
#error unrecognize persist granularity
#endif
}

/* 32 bit pcas */
static const uint32_t DIRTY_32 = ((uint32_t)1 << 31);
static const uint32_t CLEAR_DIRTY_32 = (uint32_t)(~DIRTY_32);
static const uint32_t UINT31_MAX = (((uint32_t)1 << 31) - 1);

static force_inline uint32_t pcas_read_uint32(_Atomic_uint32_t *addr) {
  uint32_t val = load_uint32_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  uint32_t exp;
  if (val & DIRTY_32) {
    shmem_output_cacheline(addr, sizeof(uint32_t));
    exp = val;
    atomic_cmpxchg_uint32(addr, &exp, exp & CLEAR_DIRTY_32);
  }
  return val & CLEAR_DIRTY_32;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline uint32_t pcas_read_noflush_uint32(_Atomic_uint32_t *addr) {
  uint32_t val = load_uint32_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  return val & CLEAR_DIRTY_32;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline void persist_atomic_write_uint32(_Atomic_uint32_t *addr,
                                                     uint32_t val) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  store_uint32_release(addr, val);
#elif defined(CACHELINE_GRANULAR)
  store_uint32_release(addr, val | DIRTY_32);
  shmem_output_cacheline(addr, sizeof(uint32_t));
#else
  (void)addr;
  (void)val;
#error unrecognize persist granularity
#endif
}

static force_inline bool persist_cas_uint32(_Atomic_uint32_t *addr,
                                            uint32_t *exp, uint32_t desire) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  return atomic_cmpxchg_uint32(addr, exp, desire);
#elif defined(CACHELINE_GRANULAR)
  pcas_read_uint32(addr);
  return atomic_cmpxchg_uint32(addr, exp, desire | DIRTY_32);
#else
  (void)addr;
  (void)exp;
  (void)desire;
#error unrecognize persist granularity
#endif
}

/* 64 bit pcas */
static const uint64_t DIRTY_64 = ((uint64_t)1 << 63);
static const uint64_t CLEAR_DIRTY_64 = (uint64_t)(~DIRTY_64);
static const uint64_t UINT63_MAX = (((uint64_t)1 << 63) - 1);

static force_inline uint64_t pcas_read_uint64(_Atomic_uint64_t *addr) {
  uint64_t val = load_uint64_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  uint64_t exp;
  if (val & DIRTY_64) {
    shmem_output_cacheline(addr, sizeof(uint64_t));
    exp = val;
    atomic_cmpxchg_uint64(addr, &exp, exp & CLEAR_DIRTY_64);
  }
  return val & CLEAR_DIRTY_64;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline uint64_t
pcas_read_noflush_uint64(const _Atomic_uint64_t *addr) {
  uint64_t val = load_uint64_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  return val & CLEAR_DIRTY_64;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline void persist_atomic_write_uint64(_Atomic_uint64_t *addr,
                                                     uint64_t val) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  store_uint64_release(addr, val);
#elif defined(CACHELINE_GRANULAR)
  store_uint64_release(addr, val | DIRTY_64);
  shmem_output_cacheline(addr, sizeof(uint64_t));
#else
  (void)addr;
  (void)val;
#error unrecognize persist granularity
#endif
}

static force_inline bool persist_cas_uint64(_Atomic_uint64_t *addr,
                                            uint64_t *exp, uint64_t desire) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  return atomic_cmpxchg_uint64(addr, exp, desire);
#elif defined(CACHELINE_GRANULAR)
  pcas_read_uint64(addr);
  return atomic_cmpxchg_uint64(addr, exp, desire | DIRTY_64);
#else
  (void)addr;
  (void)exp;
  (void)desire;
#error unrecognize persist granularity
#endif
}

#if !defined(__KERNEL__) && !defined(MODULE)
/* 128 bit pcas */
static const uint128_t DIRTY_128 = ((uint128_t)1 << 127);
static const uint128_t CLEAR_DIRTY_128 = (uint128_t)(~DIRTY_128);
static const uint128_t UINT127_MAX = (((uint128_t)1 << 127) - (uint128_t)1);

static force_inline uint128_t pcas_read_uint128(uint128_t *addr) {
  uint128_t val = load_uint128_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  uint128_t exp;
  if (val & DIRTY_128) {
    shmem_output_cacheline(addr, sizeof(uint128_t));
    exp = val;
    atomic_cmpxchg_uint128(addr, &exp, exp & CLEAR_DIRTY_128);
  }
  return val & CLEAR_DIRTY_128;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline uint128_t pcas_read_noflush_uint128(uint128_t *addr) {
  uint128_t val = load_uint128_acquire(addr);
#if defined(BYTE_GRANULAR) || defined(DRAM)
  return val;
#elif defined(CACHELINE_GRANULAR)
  return val & CLEAR_DIRTY_128;
#else
  (void)addr;
#error unrecognize persist granularity
#endif
}

static force_inline void persist_atomic_write_uint128(uint128_t *addr,
                                                      uint128_t val) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  store_uint128_release(addr, val);
#elif defined(CACHELINE_GRANULAR)
  store_uint128_release(addr, val | DIRTY_128);
  shmem_output_cacheline(addr, sizeof(uint128_t));
#else
  (void)addr;
  (void)val;
#error unrecognize persist granularity
#endif
}

static force_inline bool persist_cas_uint128(uint128_t *addr, uint128_t *exp,
                                             uint128_t desire) {
#if defined(DRAM) || defined(BYTE_GRANULAR)
  return atomic_cmpxchg_uint128(addr, exp, desire);
#elif defined(CACHELINE_GRANULAR)
  pcas_read_uint128(addr);
  return atomic_cmpxchg_uint128(addr, exp, desire | DIRTY_128);
#else
  (void)addr;
  (void)exp;
  (void)desire;
#error unrecognize persist granularity
#endif
}
#endif

/**
 * #if defined(__KERNEL__) || defined(MODULE)
 * #define persist_atomic_write(addr, val)                \
 *   _Generic((addr),                                     \
 *       _Atomic_uint8_t *: persist_atomic_write_uint8,   \
 *       _Atomic_uint16_t *: persist_atomic_write_uint16, \
 *       _Atomic_uint32_t *: persist_atomic_write_uint32, \
 *       _Atomic_uint64_t *: persist_atomic_write_uint64)(addr, val)
 * #define persist_cas(addr, exp, desire)        \
 *   _Generic((addr),                            \
 *       _Atomic_uint8_t *: persist_cas_uint8,   \
 *       _Atomic_uint16_t *: persist_cas_uint16, \
 *       _Atomic_uint32_t *: persist_cas_uint32, \
 *       _Atomic_uint64_t *: persist_cas_uint64)(addr, exp, desire)
 * #define pcas_read(addr)                     \
 *   _Generic((addr),                          \
 *       _Atomic_uint8_t *: pcas_read_uint8,   \
 *       _Atomic_uint16_t *: pcas_read_uint16, \
 *       _Atomic_uint32_t *: pcas_read_uint32, \
 *       _Atomic_uint64_t *: pcas_read_uint64)(addr)
 * #define pcas_read_noflush(addr)                     \
 *   _Generic((addr),                                  \
 *       _Atomic_uint8_t *: pcas_read_noflush_uint8,   \
 *       _Atomic_uint16_t *: pcas_read_noflush_uint16, \
 *       _Atomic_uint32_t *: pcas_read_noflush_uint32, \
 *       _Atomic_uint64_t *: pcas_read_noflush_uint64)(addr)
 *
 * #else
 *
 * #define persist_atomic_write(addr, val)                \
 *   _Generic((addr),                                     \
 *       _Atomic_uint8_t *: persist_atomic_write_uint8,   \
 *       _Atomic_uint16_t *: persist_atomic_write_uint16, \
 *       _Atomic_uint32_t *: persist_atomic_write_uint32, \
 *       _Atomic_uint64_t *: persist_atomic_write_uint64, \
 *       uint128_t *: persist_atomic_write_uint128)(addr, val)
 * #define persist_cas(addr, exp, desire)        \
 *   _Generic((addr),                            \
 *       _Atomic_uint8_t *: persist_cas_uint8,   \
 *       _Atomic_uint16_t *: persist_cas_uint16, \
 *       _Atomic_uint32_t *: persist_cas_uint32, \
 *       _Atomic_uint64_t *: persist_cas_uint64, \
 *       uint128_t *: persist_cas_uint128)(addr, exp, desire)
 * #define pcas_read(addr)                     \
 *   _Generic((addr),                          \
 *       _Atomic_uint8_t *: pcas_read_uint8,   \
 *       _Atomic_uint16_t *: pcas_read_uint16, \
 *       _Atomic_uint32_t *: pcas_read_uint32, \
 *       _Atomic_uint64_t *: pcas_read_uint64, \
 *       uint128_t *: pcas_read_uint128)(addr)
 * #define pcas_read_noflush(addr)                     \
 *   _Generic((addr),                                  \
 *       _Atomic_uint8_t *: pcas_read_noflush_uint8,   \
 *       _Atomic_uint16_t *: pcas_read_noflush_uint16, \
 *       _Atomic_uint32_t *: pcas_read_noflush_uint32, \
 *       _Atomic_uint64_t *: pcas_read_noflush_uint64, \
 *       uint128_t *: pcas_read_noflush_uint128)(addr)
 *
 * #endif
 */

#endif  // PERSIST_CAS_H_
