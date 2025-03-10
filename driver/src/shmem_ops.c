#include "shmem_ops.h"

/*
 * shmem_memmove -- mem[move|cpy] to shmem
 */
void * shmem_memmove(void *shmemdest, const void *src, size_t len,
		unsigned flags)
{
	if (!(flags & SHMEM_F_MEM_NOFLUSH)) {
#if defined(__KERNEL__) || defined(MODULE)
        memcpy_flushcache(shmemdest, src, len);
#else
		memmove(shmemdest, src, len);
		shmem_flush_cpu_cache(shmemdest, len);
#endif
	} else {
        memmove(shmemdest, src, len);
	}
	if ((flags & (SHMEM_F_MEM_NODRAIN | SHMEM_F_MEM_NOFLUSH)) == 0) {
		shmem_drain();
	}
    return shmemdest;
}

/*
 * shmem_memset -- memset to shmem
 */
void * shmem_memset(void *shmemdest, int c, size_t len, unsigned flags)
{
    memset(shmemdest, c, len);

	if (!(flags & SHMEM_F_MEM_NOFLUSH))
		shmem_flush_cpu_cache(shmemdest, len);

	if ((flags & (SHMEM_F_MEM_NODRAIN | SHMEM_F_MEM_NOFLUSH)) == 0)
		shmem_drain();
    return shmemdest;
}
