#ifndef LIST_QUEUE_H_
#define LIST_QUEUE_H_

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/stddef.h>
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "shmem_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

struct list_queue {
	struct shmem_optr pe_first;
	struct shmem_optr pe_tail;
};
_Static_assert(sizeof(struct list_queue) == 16,
	       "list_queue size is not expected");

// size: 16 bytes
struct list_entry {
	struct shmem_optr pe_next;
	struct shmem_optr pe_prev;
};
_Static_assert(sizeof(struct list_entry) == 16,
	       "list_entry size is not expected");
#define LE_PREV_OFF \
	offsetof(struct list_entry, pe_prev) + offsetof(struct shmem_optr, off)
#define LE_NEXT_OFF \
	offsetof(struct list_entry, pe_next) + offsetof(struct shmem_optr, off)

static force_inline WARN_UNUSED_RESULT struct shmem_optr
le_prev_optr(struct shmem_optr le_optr)
{
	return (struct shmem_optr){
		.off = le_optr.off + LE_PREV_OFF,
	};
}

static force_inline WARN_UNUSED_RESULT struct shmem_optr
le_next_optr(struct shmem_optr le_optr)
{
	return (struct shmem_optr){
		.off = le_optr.off + LE_NEXT_OFF,
	};
}

#ifdef __cplusplus
}
#endif

#endif // LIST_QUEUE_H_
