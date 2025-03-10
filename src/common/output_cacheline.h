#ifndef VCXL_PERSIST_H_
#define VCXL_PERSIST_H_

#include "common_macro.h"
#if defined (__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#include <linux/libnvdimm.h>
#include <asm-generic/barrier.h>

static force_inline void shmem_output_cacheline(const void *addr, size_t len)
{
#ifdef BYTE_GRANULAR
// #pragma message "using byte granular"
	SUPPRESS_UNUSED(addr, len);
	wmb();	
#elif defined(CACHELINE_GRANULAR)
// #pragma message "using cacheline granular"
	arch_wb_cache_pmem((void *)addr, len);
	wmb();	/* ensure CLWB or CLFLUSHOPT completes */
#elif defined(DRAM)
	SUPPRESS_UNUSED(addr, len);
#else
	(void)addr;
	(void)len;
#error unrecognize persist granularity
#endif
}

static force_inline void shmem_flush_cpu_cache(const void *addr, size_t len)
{
#if defined(BYTE_GRANULAR) || defined(DRAM)
	SUPPRESS_UNUSED(addr, len);
#elif defined(CACHELINE_GRANULAR)
	arch_wb_cache_pmem((void *)addr, len);
#else
	(void)addr;
	(void)len;
#error unrecognize persist granularity
#endif
}

static force_inline void shmem_drain(void)
{
	wmb();
}

#else
#include <stdint.h>

#define FLUSH_ALIGN ((uintptr_t)64)

static force_inline void shmem_drain(void) {
    __asm__ volatile("sfence":::"memory");
}

static force_inline void
pmem_clwb(const void *addr)
{
    __asm__ volatile(".byte 0x66; xsaveopt %0" : "+m" \
            (*(volatile char *)(uintptr_t)(addr)));
}

static force_inline void shmem_flush_cpu_cache(const void *addr, size_t len)
{
#if defined(BYTE_GRANULAR) || defined(DRAM)
	SUPPRESS_UNUSED(addr, len);
#elif defined(CACHELINE_GRANULAR)
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		pmem_clwb((char *)uptr);
	}
#else
	(void)addr;
	(void)len;
#error unrecognize protection granularity
#endif
}

static force_inline void shmem_output_cacheline(const void *addr, size_t len)
{
#ifdef BYTE_GRANULAR
// #pragma message "using byte granular"
	SUPPRESS_UNUSED(addr, len);
	shmem_drain(); /* ensure write ordering */
#elif defined(CACHELINE_GRANULAR)
// #pragma message "using cacheline granular"
	shmem_flush_cpu_cache(addr, len);
	shmem_drain();	/* ensure CLWB or CLFLUSHOPT completes */
#elif defined(DRAM)
	SUPPRESS_UNUSED(addr, len);
#else
	(void)addr;
	(void)len;
#error unrecognize protection granularity
#endif
}
#endif // user space

#endif // VCXL_PERSIST_H_ 
