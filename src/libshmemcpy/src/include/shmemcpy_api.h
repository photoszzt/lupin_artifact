#ifndef SHMEMCPY_API_H
#define SHMEMCPY_API_H 1

#if defined (__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void shmemcpy_api_init(void);

/*
 * shmem_drain -- wait for any shared memory stores to drain from HW buffers
 */
void shmem_drain(void);

/*
 * shmem_flush_cpu_cache -- flush processor cache for the given range
 */
void shmem_flush_cpu_cache(const void *addr, size_t len);

/*
 * shmem_output_cpu_cache -- make all changes to a range of shmem visible to other machines
 */
void shmem_output_cpu_cache(const void *addr, size_t len);

/*
 * shmem_memmove -- mem[move|cpy] to shmem
 */
void * shmem_memmove(void *shmemdest, const void *src, size_t len,
		unsigned flags);

/*
 * shmem_memset -- memset to shmem
 */
void * shmem_memset(void *shmemdest, int c, size_t len, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif // SHMEMCPY_API_H