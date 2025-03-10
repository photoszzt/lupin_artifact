#ifndef VCXL_SHMEM_OPS_H_
#define VCXL_SHMEM_OPS_H_ 1

#include <linux/string.h>
#include "output_cacheline.h"
#include "shmemcpy_arch_api.h"

/*
 * shmem_memmove -- mem[move|cpy] to shmem
 */
void * shmem_memmove(void *shmemdest, const void *src, size_t len,
		unsigned flags);

/*
 * shmem_memset -- memset to shmem
 */
void * shmem_memset(void *shmemdest, int c, size_t len, unsigned flags);

#endif // VCXL_SHMEM_OPS_H_
