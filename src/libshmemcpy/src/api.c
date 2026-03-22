#include "shmemcpy_arch_api.h"
#include "common_macro.h"
#if defined(__KERNEL__) || defined(MODULE)
#include <linux/string.h>
#else
#include <string.h>
#include "out.h"
#endif

static struct shmemcpy_arch_api arch_api;

/*
 * memmove_nodrain_libc -- (internal) memmove to pmem using libc
 */
static void *
memmove_nodrain_libc(void *pmemdest, const void *src, size_t len,
		unsigned flags, flush_func flush,
		const struct memmove_nodrain *memmove_funcs)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x", pmemdest, src, len,
			flags);

	SUPPRESS_UNUSED(memmove_funcs);

	memmove(pmemdest, src, len);

	if (!(flags & SHMEM_F_MEM_NOFLUSH))
		flush(pmemdest, len);

	return pmemdest;
}

/*
 * memset_nodrain_libc -- (internal) memset to pmem using libc
 */
static void *
memset_nodrain_libc(void *pmemdest, int c, size_t len, unsigned flags,
		flush_func flush, const struct memset_nodrain *memset_funcs)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x", pmemdest, c, len,
			flags);

	SUPPRESS_UNUSED(memset_funcs);

	memset(pmemdest, c, len);

	if (!(flags & SHMEM_F_MEM_NOFLUSH))
		flush(pmemdest, len);

	return pmemdest;
}

void shmemcpy_api_init(void) {
    arch_api.memmove_nodrain = NULL;
    arch_api.memset_nodrain = NULL;
    arch_api.memmove_nodrain_eadr = NULL;
    arch_api.memset_nodrain_eadr = NULL;
    arch_api.flush = NULL;
    arch_api.fence = NULL;
    arch_api.flush_has_builtin_fence = 0;

    shmemcpy_arch_api_init(&arch_api);
    if (arch_api.memmove_nodrain == NULL) {
#if defined(__KERNEL__) || defined(MODULE)
        arch_api.memmove_nodrain = memmove_nodrain_libc;
        arch_api.memmove_nodrain_eadr = memmove_nodrain_libc;
#else
		(void)memmove_nodrain_libc;
        arch_api.memmove_nodrain = memmove_nodrain_generic;
        arch_api.memmove_nodrain_eadr = memmove_nodrain_generic;
#endif
    }

    if (arch_api.memset_nodrain == NULL) {
#if defined(__KERNEL__) || defined(MODULE)
        arch_api.memset_nodrain = memset_nodrain_libc;
        arch_api.memset_nodrain_eadr = memset_nodrain_libc;
#else
		(void)memset_nodrain_libc;
        arch_api.memset_nodrain = memset_nodrain_generic;
        arch_api.memset_nodrain_eadr = memset_nodrain_generic;
#endif
    }
}

/*
 * shmem_drain -- wait for any shared memory stores to drain from HW buffers
 */
void
shmem_drain(void)
{
	arch_api.fence();
}

/*
 * shmem_flush_cpu_cache -- flush processor cache for the given range
 */
void
shmem_flush_cpu_cache(const void *addr, size_t len)
{
	arch_api.flush(addr, len);
}

/*
 * shmem_output_cpu_cache -- make all changes to a range of shmem visible to other machines
 */
void
shmem_output_cpu_cache(const void *addr, size_t len)
{
	shmem_flush_cpu_cache(addr, len);
	shmem_drain();
}

/*
 * shmem_memmove -- mem[move|cpy] to shmem
 */
void *
shmem_memmove(void *shmemdest, const void *src, size_t len,
		unsigned flags)
{
#ifdef DEBUG
	if (flags & ~SHMEM_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	arch_api.memmove_nodrain(shmemdest, src, len, flags, arch_api.flush,
			&arch_api.memmove_funcs);
	if ((flags & (SHMEM_F_MEM_NODRAIN | SHMEM_F_MEM_NOFLUSH)) == 0)
		shmem_drain();

	return shmemdest;
}

/*
 * shmem_memset -- memset to shmem
 */
void * 
shmem_memset(void *shmemdest, int c, size_t len, unsigned flags)
{
#ifdef DEBUG
	if (flags & ~SHMEM_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	arch_api.memset_nodrain(shmemdest, c, len, flags, arch_api.flush,
			&arch_api.memset_funcs);
	if ((flags & (SHMEM_F_MEM_NODRAIN | SHMEM_F_MEM_NOFLUSH)) == 0)
		shmem_drain();

	return shmemdest;
}
