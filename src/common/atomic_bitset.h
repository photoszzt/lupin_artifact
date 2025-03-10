#ifndef ATOMIC_BITSET_H_
#define ATOMIC_BITSET_H_

#include "atomic_int_op.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void unset_bit_uint8(_Atomic_uint8_t* active, uint8_t k)
{
    uint8_t mask = (uint8_t)((uint8_t)1 << k);
    if (load_uint8_acquire(active) & mask) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint8_t));
#endif

#if defined(__KERNEL__) || defined(MODULE)
        __atomic_fetch_sub((uint8_t *)active, mask, __ATOMIC_ACQ_REL);
#else
        atomic_fetch_sub_explicit(active, mask, memory_order_acq_rel);
#endif

#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint8_t));
#endif
    }
}

static inline void set_bit_uint8(_Atomic_uint8_t* active, uint8_t k)
{
    uint8_t mask = (uint8_t)((uint8_t)1 << k);
    if (!(load_uint8_acquire(active) & mask)) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint8_t));
#endif
        atomic_fetch_add_uint8(active, mask);
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint8_t));
#endif
    }
}

static inline void unset_bit_uint16(_Atomic_uint16_t* active, uint8_t k)
{
    uint16_t mask = (uint16_t)((uint16_t)1 << k);
    if (load_uint16_acquire(active) & mask) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint16_t));
#endif

#if defined(__KERNEL__) || defined(MODULE)
        __atomic_fetch_sub((uint16_t *)active, mask, __ATOMIC_ACQ_REL);
#else
        atomic_fetch_sub_explicit(active, mask, memory_order_acq_rel);
#endif

#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint16_t));
#endif
    }
}

static inline void set_bit_uint16(_Atomic_uint16_t* active, uint8_t k)
{
    uint16_t mask = (uint16_t)((uint16_t)1 << k);
    if (!(load_uint16_acquire(active) & mask)) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint16_t));
#endif
        atomic_fetch_add_uint16(active, mask);
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint16_t));
#endif
    }
}

static inline void unset_bit_uint32(_Atomic_uint32_t* active, uint8_t k)
{
    uint32_t mask = (uint32_t)((uint32_t)1 << k);
    if (load_uint32_acquire(active) & mask) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint32_t));
#endif

#if defined(__KERNEL__) || defined(MODULE)
        __atomic_fetch_sub((uint32_t *)active, mask, __ATOMIC_ACQ_REL);
#else
        atomic_fetch_sub_explicit(active, mask, memory_order_acq_rel);
#endif

#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint32_t));
#endif
    }
}

static inline void set_bit_uint32(_Atomic_uint32_t* active, uint8_t k)
{
    uint32_t mask = (uint32_t)((uint32_t)1 << k);
    if (!(load_uint32_acquire(active) & mask)) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint32_t));
#endif
        atomic_fetch_add_uint32(active, mask);
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint32_t));
#endif
    }
}

static inline void unset_bit_uint64(_Atomic_uint64_t* active, uint8_t k)
{
    uint64_t mask = (uint64_t)((uint64_t)1 << k);
    if (load_uint64_acquire(active) & mask) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint64_t));
#endif

#if defined(__KERNEL__) || defined(MODULE)
        __atomic_fetch_sub((uint64_t *)active, mask, __ATOMIC_ACQ_REL);
#else
        atomic_fetch_sub_explicit(active, mask, memory_order_acq_rel);
#endif

#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint64_t));
#endif
    }
}

static inline void set_bit_uint64(_Atomic_uint64_t* active, uint8_t k)
{
    uint64_t mask = (uint64_t)((uint64_t)1 << k);
    if (!(load_uint64_acquire(active) & mask)) {
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint64_t));
#endif
        atomic_fetch_add_uint64(active, mask);
#ifdef CACHELINE_GRANULAR
        shmem_output_cacheline(active, sizeof(uint64_t));
#endif
    }
}

#define next_bit_f(wname) JOIN(next_bit, wname)
#define def_next_bit(wtype, wname, wsize) \
static inline uint8_t next_bit_f(wname)(wtype active, uint8_t k) \
{ \
    uint8_t pos = (uint8_t)((k + (uint8_t)1) % (uint8_t)wsize); \
    while (true) { \
        if (active & (wtype)(((wtype)1) << pos)) { \
            break; \
        } \
        pos = (uint8_t)((pos + (uint8_t)1) % (uint8_t)wsize); \
    } \
    return pos; \
}
def_next_bit(uint64_t, uint64, 64)
def_next_bit(uint32_t, uint32, 32)
def_next_bit(uint16_t, uint16, 16)
def_next_bit(uint8_t, uint8, 8)



#define set_bit(active, k) _Generic((active), \
    _Atomic_uint8_t*: set_bit_uint8, \
    _Atomic_uint16_t*: set_bit_uint16, \
    _Atomic_uint32_t*: set_bit_uint32, \
    _Atomic_uint64_t*: set_bit_uint64)(active, k)
#define unset_bit(active, k) _Generic((active), \
    _Atomic_uint8_t*: unset_bit_uint8, \
    _Atomic_uint16_t*: unset_bit_uint16, \
    _Atomic_uint32_t*: unset_bit_uint32, \
    _Atomic_uint64_t*: unset_bit_uint64)(active, k)
#define next_bit(active, k) _Generic((active), \
    uint8_t: next_bit_uint8, \
    uint16_t: next_bit_uint16, \
    uint32_t: next_bit_uint32, \
    uint64_t: next_bit_uint64)(active, k)


#ifdef __cplusplus
}
#endif

#endif // ATOMIC_BITSET_H_