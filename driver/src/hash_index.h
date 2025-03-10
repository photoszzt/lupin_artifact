#ifndef VCXL_HASH_INDEX_H_
#define VCXL_HASH_INDEX_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wconversion"

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#include <asm/processor.h>
#include <linux/list_bl.h>
#else
#include <stdint.h>
#include <stdbool.h>
#include <linux/hashtable.h>
#include "spinlock.h"
#endif

#include "obj_header.h"

#pragma GCC diagnostic pop

/* This table stores index from the program id -> objheader virtual address */
struct shmem_index {
#if defined(__KERNEL__) || defined(MODULE)
	struct hlist_bl_head *table;
#else
	struct hlist_head *table;
	spinlock_t lock;
#endif
	uint32_t size;
	uint32_t hash_seed;
};

#if defined(__KERNEL__) || defined(MODULE)
#define DEFINE_SHMEM_INDEX(name) \
	struct shmem_index name = { .table = NULL, .size = 0, .hash_seed = 0 }
#else
#define DEFINE_SHMEM_INDEX(name) \
	struct shmem_index name = { .table = NULL, .size = 0, .hash_seed = 0 }
#endif

int init_shmem_index(struct shmem_index *idx);
void shmem_index_exit(struct shmem_index *idx);
void shmem_index_insert(struct shmem_index *idx, char *key,
			struct obj_header *obj_hdr);
bool shmem_index_del(struct shmem_index *idx, char *key);
bool lookup_shmem_alloc_record(struct shmem_index *idx, char *key,
			       struct obj_header **obj_hdr_ret);

#endif // VCXL_HASH_INDEX_H_
