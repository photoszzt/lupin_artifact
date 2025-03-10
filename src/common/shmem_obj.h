#ifndef SHMEM_OBJ_H
#define SHMEM_OBJ_H

#include <linux/types.h>
#if !defined(__KERNEL__) && !defined(MODULE)
#include <stdbool.h>
#endif
#include "common_macro.h"

// shmem_oid is a relative pointer that points to an object in the shared memory.
struct shmem_optr {
	uint64_t off;
};

static const struct shmem_optr SHMEM_OPTR_NULL = { 0 };

static WARN_UNUSED_RESULT force_inline bool
shmem_optr_is_null(struct shmem_optr oid)
{
	return oid.off == 0;
}

static WARN_UNUSED_RESULT force_inline bool
shmem_optr_equals(struct shmem_optr optr1, struct shmem_optr optr2)
{
	return optr1.off == optr2.off;
}

static WARN_UNUSED_RESULT force_inline void *
shmem_optr_direct_ptr(uintptr_t base, struct shmem_optr oid)
{
	if (oid.off == 0) {
		return NULL;
	}
	return (void *)(base + (uintptr_t)oid.off);
}

static WARN_UNUSED_RESULT force_inline struct shmem_optr
shmem_optr_from_ptr(uintptr_t base, void *ptr)
{
	if (ptr == NULL) {
		return (struct shmem_optr){
			.off = 0,
		};
	} else {
		return (struct shmem_optr){
			.off = (uint64_t)((uintptr_t)ptr - base),
		};
	}
}

static WARN_UNUSED_RESULT force_inline uintptr_t optr_to_offset(uintptr_t base,
								void *ptr)
{
	return (uintptr_t)ptr - base;
}

#endif // SHMEM_OBJ_H
