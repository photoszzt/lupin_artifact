#ifndef LOAD_AND_FLUSH_H_
#define LOAD_AND_FLUSH_H_

#include "output_cacheline.h"
#include "atomic_int_op.h"
#include "common_macro.h"

static WARN_UNUSED_RESULT force_inline uint8_t load_and_flush_uint8(_Atomic_uint8_t* addr)
{
    uint8_t ret = load_uint8_acquire(addr);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(addr, sizeof(uint8_t));
#endif
    return ret;
}

static WARN_UNUSED_RESULT force_inline uint16_t load_and_flush_uint16(_Atomic_uint16_t* addr)
{
    uint16_t ret = load_uint16_acquire(addr);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(addr, sizeof(uint16_t));
#endif
    return ret;
}

static WARN_UNUSED_RESULT force_inline uint32_t load_and_flush_uint32(_Atomic_uint32_t* addr)
{
    uint32_t ret = load_uint32_acquire(addr);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(addr, sizeof(uint32_t));
#endif
    return ret;
}

static WARN_UNUSED_RESULT force_inline uint64_t load_and_flush_uint64(_Atomic_uint64_t* addr)
{
    uint64_t ret = load_uint64_acquire(addr);
#ifdef CACHELINE_GRANULAR
    shmem_output_cacheline(addr, sizeof(uint64_t));
#endif
    return ret;
}

#define load_and_flush(addr) _Generic((addr), \
    _Atomic_uint8_t*: load_and_flush_uint8, \
    _Atomic_uint16_t*: load_and_flush_uint16, \
    _Atomic_uint32_t*: load_and_flush_uint32, \
    _Atomic_uint64_t*: load_and_flush_uint64)(addr)

#endif // LOAD_AND_FLUSH_H_
