/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

#ifndef SHMEMCPY_ARCH_API_H
#define SHMEMCPY_ARCH_API_H 1

#if defined (__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stddef.h>
#include <sys/types.h>
#endif

#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHMEM_F_MEM_NODRAIN     (1U << 0)

#define SHMEM_F_MEM_NONTEMPORAL (1U << 1)
#define SHMEM_F_MEM_TEMPORAL    (1U << 2)

#define SHMEM_F_MEM_WC          (1U << 3)
#define SHMEM_F_MEM_WB          (1U << 4)

#define SHMEM_F_MEM_NOFLUSH     (1U << 5)

#define SHMEM_F_MEM_VALID_FLAGS (SHMEM_F_MEM_NODRAIN | \
                SHMEM_F_MEM_NONTEMPORAL | \
                SHMEM_F_MEM_TEMPORAL | \
                SHMEM_F_MEM_WC | \
                SHMEM_F_MEM_WB | \
                SHMEM_F_MEM_NOFLUSH)


struct shmemcpy_api;
struct memmove_nodrain;
struct memset_nodrain;

typedef void (*fence_func)(void);
typedef void (*flush_func)(const void *, size_t);
typedef void *(*memmove_nodrain_func)(void *shmemdest, const void *src,
		size_t len, unsigned flags, flush_func flush,
		const struct memmove_nodrain *memmove_funcs);
typedef void *(*memset_nodrain_func)(void *shmemdest, int c, size_t len,
		unsigned flags, flush_func flush,
		const struct memset_nodrain *memset_funcs);
typedef void (*memmove_func)(char *shmemdest, const char *src, size_t len);
typedef void (*memset_func)(char *shmemdest, int c, size_t len);

struct memmove_nodrain {
	struct {
		memmove_func noflush;
		memmove_func flush;
		memmove_func empty;
	} t; /* temporal */
	struct {
		memmove_func noflush;
		memmove_func flush;
		memmove_func empty;
	} nt; /* nontemporal */
};

struct memset_nodrain {
	struct {
		memset_func noflush;
		memset_func flush;
		memset_func empty;
	} t; /* temporal */
	struct {
		memset_func noflush;
		memset_func flush;
		memset_func empty;
	} nt; /* nontemporal */
};

struct shmemcpy_arch_api {
	struct memmove_nodrain memmove_funcs;
	struct memset_nodrain memset_funcs;
	memmove_nodrain_func memmove_nodrain;
	memmove_nodrain_func memmove_nodrain_eadr;
	memset_nodrain_func memset_nodrain;
	memset_nodrain_func memset_nodrain_eadr;
	flush_func flush;
	fence_func fence;
	int flush_has_builtin_fence;
};

void
shmemcpy_arch_api_init(struct shmemcpy_arch_api *info);

void *
memset_nodrain_generic(void *dst, int c, size_t len, unsigned flags,
		flush_func flush, const struct memset_nodrain *memset_funcs);
void *
memmove_nodrain_generic(void *dst, const void *src, size_t len, unsigned flags,
		flush_func flush, const struct memmove_nodrain *memmove_funcs);

#ifdef __cplusplus
}
#endif

#endif // SHMEMCPY_ARCH_API_H
