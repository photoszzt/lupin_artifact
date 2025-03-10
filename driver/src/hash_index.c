#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wswitch-default"

#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/string.h>

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/random.h>
#include <asm/processor.h>
#include <linux/list_bl.h>
#else
#include <assert.h>
#include <errno.h>
#include <sys/random.h>
#include <linux/hashtable.h>
#include "spinlock.h"
#endif
#pragma GCC diagnostic pop

#include "common_macro.h"
#include "vcxl_def.h"
#include "uapi/vcxl_udef.h"
#include "hash_index.h"
#include "random_bytes.h"

#if defined(__KERNEL__) || defined(MODULE)
static struct kmem_cache *alloc_record_cache;
#define HASHLINK_TYPE struct hlist_bl_node
#else
#define HASHLINK_TYPE struct hlist_node
#endif

struct shmem_alloc_record {
	HASHLINK_TYPE hashlink; /* link in hash table */
	char key[PROG_ID_SIZE];
	struct obj_header *obj_hdr;
};

int init_shmem_index(struct shmem_index *idx)
{
	/* 16KiB for hash buckets */
	uint64_t mem = 16 * 1024;
	uint32_t i;
	int ret;
	uint32_t num_buckets;

#if defined(__KERNEL__) || defined(MODULE)
	num_buckets = (uint32_t)(mem / sizeof(struct hlist_bl_head));
	idx->table = Vmalloc_array(num_buckets, sizeof(struct hlist_bl_head));
#else
	num_buckets = (uint32_t)(mem / sizeof(struct hlist_head));
	idx->table = Vmalloc_array(num_buckets, sizeof(struct hlist_head));
	spin_lock_init_private(&idx->lock);
#endif
	idx->size = num_buckets;
	if (!idx->table) {
		return -ENOMEM;
	}
	for (i = 0; i < idx->size; i++) {
#if defined(__KERNEL__) || defined(MODULE)
		INIT_HLIST_BL_HEAD(idx->table + i);
#else
		INIT_HLIST_HEAD(idx->table + i);
#endif
	}

	ret = wait_and_get_random_bytes(&idx->hash_seed,
					sizeof(idx->hash_seed));
	if (ret < 0) {
		return ret;
	}
#if defined(__KERNEL__) || defined(MODULE)
	alloc_record_cache = kmem_cache_create(
		"alloc_record_cache", sizeof(struct shmem_alloc_record), 0, 0,
		NULL);
#endif
	return 0;
}

void shmem_index_exit(struct shmem_index *idx)
{
#if defined(__KERNEL__) || defined(MODULE)
	struct hlist_bl_head *slot;
	struct hlist_bl_node *pos, *n;
#else
	struct hlist_head *slot;
	struct hlist_node *n;
#endif
	struct shmem_alloc_record *rec;
	uint32_t i;

	for (i = 0; i < idx->size; i++) {
		slot = &idx->table[i];
#if defined(__KERNEL__) || defined(MODULE)
		hlist_bl_for_each_entry_safe(rec, pos, n, slot, hashlink) {
			kmem_cache_free(alloc_record_cache, rec);
		}
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
		hlist_for_each_entry_safe(rec, n, slot, hashlink) {
			free(rec);
		}
#pragma GCC diagnostic pop
#endif
	}

	Vfree(idx->table);
#if defined(__KERNEL__) || defined(MODULE)
	kmem_cache_destroy(alloc_record_cache);
#endif
}

void shmem_index_insert(struct shmem_index *idx, char *key,
			struct obj_header *obj_hdr)
{
	__u32 hash =
		jhash(key, (uint32_t)strlen(key), idx->hash_seed) % idx->size;
	struct shmem_alloc_record *rec;

#if defined(__KERNEL__) || defined(MODULE)
	struct hlist_bl_head *h = &idx->table[hash];
	rec = kmem_cache_zalloc(alloc_record_cache, GFP_KERNEL);
#else
	struct hlist_head *h = &idx->table[hash];
	rec = calloc(1, sizeof(struct shmem_alloc_record));
#endif
	rec->obj_hdr = obj_hdr;
	memcpy(rec->key, key, strlen(key) + 1);
#if defined(__KERNEL__) || defined(MODULE)
	hlist_bl_lock(h);
	hlist_bl_add_head(&rec->hashlink, h);
	hlist_bl_unlock(h);
#else
	spin_lock(&idx->lock);
	hlist_add_head(&rec->hashlink, h);
	spin_unlock(&idx->lock);
#endif
}

/**
 * @brief delete the record cooresponding to the key
 *
 * @param idx pointer to shmem index
 * @param key
 * @return true if found the record cooresponding to the key and deleted it
 * @return false otherwise
 */
bool shmem_index_del(struct shmem_index *idx, char *key)
{
	__u32 hash =
		jhash(key, (uint32_t)strlen(key), idx->hash_seed) % idx->size;
	struct shmem_alloc_record *cursor;
#if defined(__KERNEL__) || defined(MODULE)
	struct hlist_bl_head *h = &idx->table[hash];
	struct hlist_bl_node *pos, *n;

	hlist_bl_lock(h);
	hlist_bl_for_each_entry_safe(cursor, pos, n, h, hashlink) {
		if (strcmp(cursor->key, key) == 0) {
			hlist_bl_del(&cursor->hashlink);
			hlist_bl_unlock(h);
			return true;
		}
	}
	hlist_bl_unlock(h);
#else
	struct hlist_head *h = &idx->table[hash];
	struct hlist_node *n;

	spin_lock(&idx->lock);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
	hlist_for_each_entry_safe(cursor, n, h, hashlink) {
		if (strcmp(cursor->key, key) == 0) {
			hlist_del(&cursor->hashlink);
			spin_unlock(&idx->lock);
			return true;
		}
	}
#pragma GCC diagnostic pop
	spin_unlock(&idx->lock);
#endif
	return false;
}

/**
 * @brief find the address cooresponding to the key
 *
 * @param idx pointer to shmem index
 * @param key
 * @param addr_ret
 * @return true if found the address cooresponding to the key
 * @return false otherwise
 */
bool lookup_shmem_alloc_record(struct shmem_index *idx, char *key,
			       struct obj_header **obj_hdr_ret)
{
	__u32 hash =
		jhash(key, (uint32_t)strlen(key), idx->hash_seed) % idx->size;
	struct shmem_alloc_record *cursor;

#if defined(__KERNEL__) || defined(MODULE)
	struct hlist_bl_head *h = &idx->table[hash];
	struct hlist_bl_node *pos, *n;
	hlist_bl_lock(h);
	hlist_bl_for_each_entry_safe(cursor, pos, n, h, hashlink) {
		if (strcmp(cursor->key, key) == 0) {
			*obj_hdr_ret = cursor->obj_hdr;
			hlist_bl_unlock(h);
			return true;
		}
	}
	hlist_bl_unlock(h);
#else
	struct hlist_head *h = &idx->table[hash];
	struct hlist_node *n;
	spin_lock(&idx->lock);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
	hlist_for_each_entry_safe(cursor, n, h, hashlink) {
		if (strcmp(cursor->key, key) == 0) {
			*obj_hdr_ret = cursor->obj_hdr;
			spin_unlock(&idx->lock);
			return true;
		}
	}
#pragma GCC diagnostic pop
	spin_unlock(&idx->lock);
#endif

	return false;
}
