#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"

#if defined(__KERNEL__) || defined(MODULE)
// header for buddy alloc
#define BUDDY_HEADER 1
#include <linux/types.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/stddef.h>
#include <asm/bug.h>
#include "output_cacheline.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8 /* Normally in <limits.h> */
#endif

// buddy alloc use SIZE_MAX in #define and can't find size_t for some reason
#ifdef SIZE_MAX
#undef SIZE_MAX
#endif

#if BITS_PER_LONG != 64
#define SIZE_MAX (~0u)
#else
#define SIZE_MAX (~0ul)
#endif

#define BUDDY_PRINTF pr_info
#define BUDDY_FRAG_OPTIONAL 1

#else // defined(__KERNEL__) || defined(MODULE)
#include <errno.h>
#include <inttypes.h>
#include "output_cacheline.h"
#endif

#define BUDDY_ALLOC_IMPLEMENTATION
#include "buddy_alloc.h"
#undef BUDDY_ALLOC_IMPLEMENTATION

#if defined(__KERNEL__) || defined(MODULE)
#undef SIZE_MAX
#endif

#include "log.h"
#include "uapi/vcxl_shm.h"
#include "alloc_impl.h"
#include "circular_queue.h"
#include "vcxl_def.h"
// #include "hash_index.h"
#include "obj_header.h"
#include "fault_inject.h"
// #include "mpsc_queue.h"
#include "cxl_cgroup.h"

#include "tatas/tatas_spinlock.h"

#pragma GCC diagnostic pop

// static DEFINE_SHMEM_INDEX(name_to_obj);

struct meta_embed_check {
	unsigned int can_fit;
	size_t offset;
	size_t length;
};

/* Metadata stored in shared memory */
struct meta_shmem {
	struct log_entry redolog[REDOLOG_ENTRIES];
	struct list_queue obj_headers;
	struct list_queue obj_allocs;
	struct ttas meta_lock;
	struct buddy
		buddy_alloc; /* buddy allocator; this struct needs to be at the end as it has a data[] */
};

void assign_meta_info(struct meta_info *info, struct meta_shmem *meta_shmem,
		      struct meta_dram *meta_dram)
{
	info->alignment = meta_shmem->buddy_alloc.alignment;
	info->buddy_flags = meta_shmem->buddy_alloc.buddy_flags;
	info->buddy_struct_offset =
		(__u64)((unsigned char *)meta_shmem - meta_dram->mem_start);
	info->mem_size = meta_shmem->buddy_alloc.memory_size;
}

static inline void record_append_obj(struct meta_shmem *meta_shmem,
				     int *redolog_pos_ptr,
				     struct obj_header *obj_hdr,
				     uint8_t *mem_start)
{
	record_list_queue_append(meta_shmem->redolog, redolog_pos_ptr,
				 &meta_shmem->obj_headers, &obj_hdr->le,
				 mem_start);
}

static inline void record_remove_obj(struct meta_shmem *meta_shmem,
				     int *redolog_pos_ptr,
				     struct obj_header *obj_hdr,
				     uint8_t *mem_start)
{
	record_list_queue_remove(meta_shmem->redolog, redolog_pos_ptr,
				 &meta_shmem->obj_headers, &obj_hdr->le,
				 mem_start);
}

static force_inline void meta_lock_init(struct meta_shmem *meta_shmem)
{
	ttas_init(&meta_shmem->meta_lock);
}

static force_inline void meta_lock(struct meta_shmem *meta_shmem,
				   struct meta_dram *meta_dram)
{
	int gen_id = get_os_gen_id(meta_dram->machine_id - 1);
	ttas_lock_with_gen(&meta_shmem->meta_lock, meta_dram->machine_id,
			   gen_id);
}

WARN_UNUSED_RESULT enum transfer_st
meta_lock_recover(struct meta_shmem *meta_shmem, struct meta_dram *meta_dram,
		  u16 old_machine_id)
{
	int gen_id = get_os_gen_id(meta_dram->machine_id - 1);
	return ttas_lock_recover(&meta_shmem->meta_lock, old_machine_id,
				 meta_dram->machine_id, gen_id);
}

void meta_lock_done_recover(struct meta_shmem *meta_shmem,
			    struct meta_dram *meta_dram, enum transfer_st st)
{
	done_ttas_lock_recover(&meta_shmem->meta_lock, meta_dram->machine_id,
			       st);
}

static force_inline void meta_unlock(struct meta_shmem *meta_shmem,
				     struct meta_dram *meta_dram)
{
	ttas_unlock(&meta_shmem->meta_lock, meta_dram->machine_id);
}

static force_inline struct buddy_tree_pos
buddy_malloc_find_pos_lockheld(struct meta_shmem *meta_shmem,
			       size_t requested_size)
	__must_hold(meta_shmem->meta_lock)
{
	return buddy_malloc_find_pos(&meta_shmem->buddy_alloc, requested_size);
}

static force_inline struct buddy_tree_pos
buddy_free_find_pos_lockheld(struct meta_shmem *meta_shmem, __u8 *addr,
			     size_t requested_size)
	__must_hold(meta_shmem->meta_lock)
{
	return buddy_free_find_pos(&meta_shmem->buddy_alloc, addr,
				   requested_size);
}

size_t metashmem_sizeof_alignment(size_t memory_size, size_t alignment)
{
	size_t buddy_tree_order;
	if (!is_valid_alignment(alignment)) {
		return 0; /* invalid */
	}
	if (memory_size < alignment) {
		return 0; /* invalid */
	}
	buddy_tree_order = buddy_tree_order_for_memory(memory_size, alignment);
	return sizeof(struct meta_shmem) +
	       buddy_tree_sizeof((uint8_t)buddy_tree_order);
}

struct buddy_embed_check metashmem_embed_offset(size_t memory_size,
						size_t alignment)
{
	struct buddy_embed_check result = { 0 };
	size_t meta_size, offset;
	result.can_fit = 1;

	meta_size = metashmem_sizeof_alignment(memory_size, alignment);
	if (meta_size >= memory_size || meta_size == 0) {
		result.can_fit = 0;
		return result;
	}

	offset = memory_size - meta_size;
	if (offset % BUDDY_ALIGNOF(struct meta_shmem) != 0) {
		meta_size += offset % BUDDY_ALIGNOF(struct meta_shmem);
		if (meta_size >= memory_size) {
			result.can_fit = 0;
		}
		offset = memory_size - meta_size;
	}

	if (result.can_fit) {
		result.offset = offset;
		result.buddy_size = meta_size;
	}
	return result;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
struct meta_shmem *
metashmem_init_alignment(unsigned char *at, unsigned char *main,
			 size_t memory_size, size_t alignment,
			 size_t *buddy_tree_size, bool output_cache_line)
{
	size_t at_alignment, main_alignment, buddy_tree_order;
	struct meta_shmem *meta_shmem;

	if (at == NULL) {
		PRINT_ERR("[ERR] metashmem_init_alignment: at is NULL\n");
		return NULL;
	}
	if (main == NULL) {
		PRINT_ERR("[ERR] metashmem_init_alignment: main is NULL\n");
		return NULL;
	}
	if (at == main) {
		PRINT_ERR(
			"[ERR] metashmem_init_alignment: main and at should not be the same\n");
		return NULL;
	}
	if (!is_valid_alignment(alignment)) {
		PRINT_ERR(
			"[ERR] metashmem_init_alignment: alignment is not valid\n");
		return NULL; /* invalid */
	}
	at_alignment = ((uintptr_t)at) % BUDDY_ALIGNOF(struct meta_shmem);
	if (at_alignment != 0) {
		PRINT_ERR(
			"[ERR] metashmem_init_alignment: meta data is not aligned\n");
		return NULL;
	}
	main_alignment = ((uintptr_t)main) % BUDDY_ALIGNOF(size_t);
	if (main_alignment != 0) {
		PRINT_ERR(
			"[ERR] metashmem_init_alignment: main region is not aligned: main %lx, alignment %lu\n",
			((uintptr_t)main), main_alignment);
		return NULL;
	}
	/* Trim down memory to alignment */
	if (memory_size % alignment) {
		memory_size -= (memory_size % alignment);
	}
	if (!is_valid_alignment(alignment)) {
		PRINT_ERR("invalid alignment");
		return NULL; /* invalid */
	}
	if (memory_size < alignment) {
		PRINT_ERR("memory size: 0x%lx smaller than alignment: 0x%lx\n",
			  memory_size, alignment);
		return NULL; /* invalid */
	}
	buddy_tree_order = buddy_tree_order_for_memory(memory_size, alignment);
	PRINT_INFO(
		"metadata at 0x%lx, main at 0x%lx, memory_size: 0x%lx, buddy_tree_order: 0x%lx\n",
		(uintptr_t)at, (uintptr_t)main, memory_size, buddy_tree_order);

	/* TODO check for overlap between buddy metadata and main block */
	meta_shmem = (struct meta_shmem *)at;
	meta_shmem->buddy_alloc.arena.main = main;
	meta_shmem->buddy_alloc.memory_size = memory_size;
	meta_shmem->buddy_alloc.buddy_flags = 0;
	meta_shmem->buddy_alloc.alignment = alignment;
	meta_lock_init(meta_shmem);
	BUG_ON(meta_shmem->buddy_alloc.memory_size != memory_size);
	BUG_ON(meta_shmem->buddy_alloc.arena.main != main);
	BUG_ON(meta_shmem->buddy_alloc.buddy_flags != 0);
	BUG_ON(meta_shmem->buddy_alloc.alignment != alignment);
	buddy_tree_init(meta_shmem->buddy_alloc.buddy_tree,
			(uint8_t)buddy_tree_order, buddy_tree_size);
	buddy_toggle_virtual_slots(&meta_shmem->buddy_alloc, 1);
	if (output_cache_line) {
		shmem_flush_cpu_cache(meta_shmem, sizeof(struct meta_shmem));
		shmem_flush_cpu_cache(meta_shmem->buddy_alloc.buddy_tree,
				      *buddy_tree_size);
		shmem_drain();
	}
	return meta_shmem;
}

struct meta_shmem *metashmem_embed_alignment(unsigned char *main,
					     size_t memory_size,
					     size_t alignment)
{
	struct buddy_embed_check result;
	struct meta_shmem *meta_shmem;
	size_t buddy_tree_size;

	if (!main) {
		PRINT_ERR("metashmem_embed_alignment: mainptr is null\n");
		return NULL;
	}
	if (!is_valid_alignment(alignment)) {
		PRINT_ERR(
			"metashmem_embed_alignment: alignment is not valid\n");
		return NULL; /* invalid */
	}
	result = metashmem_embed_offset(memory_size, alignment);
	if (!result.can_fit) {
		PRINT_ERR(
			"metashmem_embed_alignment: metadata can't fit into memory region\n");
		return NULL;
	}
	PRINT_INFO(
		"buddy meta offset: 0x%lx, meta data size: 0x%lx, metadata addr range: 0x%lx-0x%lx\n",
		result.offset, result.buddy_size,
		(uintptr_t)main + result.offset,
		(uintptr_t)main + result.offset + result.buddy_size);
	memset(main + result.offset, 0, result.buddy_size);

	meta_shmem = metashmem_init_alignment(main + result.offset, main,
					      result.offset, alignment,
					      &buddy_tree_size, false);
	if (!meta_shmem) { /* regular initialization failed */
		PRINT_ERR(
			"[ERR] metashmem_embed_alignment: metashmem_init_alignment returns NULL\n");
		return NULL;
	}
	PRINT_INFO("buddy_tree_size: 0x%lx\n", buddy_tree_size);

	meta_shmem->buddy_alloc.buddy_flags |= BUDDY_RELATIVE_MODE;
	meta_shmem->buddy_alloc.arena.main_offset =
		(unsigned char *)&meta_shmem->buddy_alloc - main;
	meta_shmem->obj_headers.pe_first = SHMEM_OPTR_NULL;
	meta_shmem->obj_headers.pe_tail = SHMEM_OPTR_NULL;
	memset((unsigned char *)&meta_shmem->redolog, 0,
	       sizeof(struct log_entry) * REDOLOG_ENTRIES);
	shmem_flush_cpu_cache(meta_shmem->buddy_alloc.buddy_tree,
			      buddy_tree_size);
	return meta_shmem;
}

static inline void flush_meta(struct meta_shmem *meta_shmem)
{
	/* flush the modification of obj list */
	shmem_flush_cpu_cache(&meta_shmem->obj_headers,
			      sizeof(meta_shmem->obj_headers));
	shmem_drain();
}

static void execute_log_entries(struct meta_shmem *meta_shmem, int redolog_pos,
				struct meta_dram *meta_dram)
{
	execute_log(meta_shmem->redolog, redolog_pos, &meta_shmem->buddy_alloc,
		    meta_dram);
	flush_meta(meta_shmem);
}

/**
 * @brief initialize metadata stored at the head of the shared memory
 */
int vcxl_init_meta_impl(struct vcxl_ivpci_device *ivpci_dev,
			struct meta_dram *meta_dram, void *dev,
			struct meta_shmem **meta_shmem_ret)
{
	uint64_t total_circular_queue_size = 0;
	uint64_t i = 0;
	struct obj_allocation_records *obj_alloc_recs;
	uint64_t total_reserved = FUTEX_ADDR_QUEUE_SIZE + OBJ_ALLOC_RECS_SIZE +
				  PROC_DEATH_NOTIFICATION_SIZE +
				  CXLCG_META_SIZE;
	struct meta_shmem *meta_shmem_ptr;
	// init_shmem_index(&name_to_obj);

	total_circular_queue_size =
		((size_t)VCXL_MAX_SUPPORTED_MACHINES) * circular_queue_size;
	if (total_circular_queue_size > FUTEX_ADDR_QUEUE_SIZE) {
		DEV_ERR(dev,
			PFX
			"The total futex addr queue size exceeds the area reserved for it, reserved: %d, expected: %" PRIu64
			"\n",
			FUTEX_ADDR_QUEUE_SIZE, total_circular_queue_size);
		return -EINVAL;
	}
	if (total_circular_queue_size > PROC_DEATH_NOTIFICATION_SIZE) {
		DEV_ERR(dev,
			PFX
			"The total proc death notification queue size exceeds the area reserved for it, reserved: %d, expected: %" PRIu64
			"\n",
			PROC_DEATH_NOTIFICATION_SIZE,
			total_circular_queue_size);
		return -EINVAL;
	}
	for (i = 0; i < VCXL_MAX_SUPPORTED_MACHINES; i++) {
		init_circular_queue(
			(struct circular_queue *)(meta_dram->mem_start +
						  i * circular_queue_size));
		init_circular_queue(
			(struct circular_queue *)(meta_dram->mem_start +
						  FUTEX_ADDR_QUEUE_SIZE +
						  OBJ_ALLOC_RECS_SIZE +
						  i * circular_queue_size));
	}

	init_cxlcg_meta(ivpci_dev, meta_dram->mem_start +
					   FUTEX_ADDR_QUEUE_SIZE +
					   OBJ_ALLOC_RECS_SIZE +
					   PROC_DEATH_NOTIFICATION_SIZE);

	obj_alloc_recs =
		(struct obj_allocation_records *)(meta_dram->mem_start +
						  FUTEX_ADDR_QUEUE_SIZE);
	init_obj_allocation_records(obj_alloc_recs);
	meta_dram->cur_obj_recs = obj_alloc_recs;
	// shmem_flush_cpu_cache(meta_dram->mem_start, total_reserved);

	/* metashmem_embed_alignment sets obj_headers to NULL */
	meta_shmem_ptr = (struct meta_shmem *)metashmem_embed_alignment(
		meta_dram->mem_start + total_reserved,
		meta_dram->mem_length - total_reserved, BUDDY_ALLOC_ALIGNMENT);
	meta_shmem_ptr->obj_allocs.pe_first =
		shmem_optr_from_ptr((uintptr_t)meta_dram->mem_start,
				    (uint8_t *)&obj_alloc_recs->le);
	meta_shmem_ptr->obj_allocs.pe_tail =
		meta_shmem_ptr->obj_allocs.pe_first;
	*meta_shmem_ret = meta_shmem_ptr;
	DEV_INFO(dev,
		 PFX "obj allocs pe_first off is %llu, pe_tail off is %llu\n",
		 (*meta_shmem_ret)->obj_allocs.pe_first.off,
		 (*meta_shmem_ret)->obj_allocs.pe_tail.off);
	shmem_flush_cpu_cache((uint8_t *)(*meta_shmem_ret),
			      sizeof(struct meta_shmem));
	shmem_drain();

	return 0;
}
#pragma GCC diagnostic pop

void vcxl_hash_index_exit(void)
{
	// shmem_index_exit(&name_to_obj);
}

int recover_with_redolog(struct meta_shmem *meta_shmem,
			 struct meta_dram *meta_dram)
{
	int i = 0;
	bool has_commit = false;
	int redolog_pos = 0;
	for (i = 0; i < REDOLOG_ENTRIES; i++) {
		struct log_entry *entry = &meta_shmem->redolog[i];
		if (entry->tp == COMMIT) {
			int ret = util_checksum(
				meta_shmem->redolog,
				sizeof(struct log_entry) * (size_t)i,
				&entry->entry.commit_loge.check_sum, 0, 0);
			if (ret) {
				has_commit = true;
				redolog_pos = i;
				break;
			} else {
				/**
          * The checksum record is malformed. Possible to get here by:
          * 1. The flush and fence is not successful. The checksum is not written completely.
          * 2. Some field of the log are not written to the cxl memory and lost in crash.
          */
				PRINT_INFO("Redolog checksum failed\n");
				return ELOG_ERR;
			}
		}
	}
	// if the commit record is not found, none of the work is performed.
	if (has_commit) {
		execute_log_entries(meta_shmem, redolog_pos, meta_dram);
	}
	return 0;
}

inline WARN_UNUSED_RESULT struct obj_header *
obj_header_head(uint8_t *mem_start, struct meta_shmem *meta_shmem)
{
	return (struct obj_header *)shmem_optr_direct_ptr(
		(uintptr_t)mem_start, meta_shmem->obj_headers.pe_first);
}

inline WARN_UNUSED_RESULT struct obj_allocation_records *
obj_alloc_recs_head(uint8_t *mem_start, struct meta_shmem *meta_shmem)
{
	return (struct obj_allocation_records *)shmem_optr_direct_ptr(
		(uintptr_t)mem_start, meta_shmem->obj_allocs.pe_first);
}

void print_allocation(uint8_t *mem_start, struct meta_shmem *meta_shmem)
{
	struct obj_header *obj_hdr;
	obj_hdr = obj_header_head(mem_start, meta_shmem);
	while (obj_hdr != NULL) {
		PRINT_INFO(
			"machine_id: %u, slot_id: %u, prog_id: %s, length: %" PRIu64
			"\n",
			obj_hdr->machine_id, obj_hdr->slot_id, obj_hdr->prog_id,
			obj_hdr->length);
		if (shmem_optr_is_null(obj_hdr->le.pe_next)) {
			break;
		}
		obj_hdr = next_obj_header(mem_start, obj_hdr);
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"

int recover_meta(struct vcxl_ivpci_device *ivpci_dev,
		 struct meta_dram *meta_dram, void *dev,
		 struct meta_shmem **meta_shmem_ret)
{
	size_t memory_size;
	struct buddy_embed_check result;
	struct obj_header *obj_hdr;
	struct obj_allocation_records *obj_alloc_recs;
	int ret;
	enum transfer_st st;
	size_t total_reserved = FUTEX_ADDR_QUEUE_SIZE + OBJ_ALLOC_RECS_SIZE +
				PROC_DEATH_NOTIFICATION_SIZE;
	// struct circular_queue *futex_addrq = NULL;

	/* increment the generation ID at "early" stage */
	recover_cxlcg_meta(ivpci_dev, meta_dram->mem_start +
					      FUTEX_ADDR_QUEUE_SIZE +
					      OBJ_ALLOC_RECS_SIZE +
					      PROC_DEATH_NOTIFICATION_SIZE);

	memory_size = meta_dram->mem_length - total_reserved;
	result = metashmem_embed_offset(memory_size, BUDDY_ALLOC_ALIGNMENT);
	if (!result.can_fit) {
		DEV_ERR(dev,
			PFX
			"fail to recover buddy allocator, memory size is: %lu\n",
			memory_size);
		return -EFAULT;
	}
	*meta_shmem_ret = (struct meta_shmem *)(meta_dram->mem_start +
						total_reserved + result.offset);
	// init_shmem_index(&name_to_obj);

	// futex_addrq = self_vcxl_addr_queue(meta_dram);
	// drain_circular_queue(futex_addrq, meta_dram->machine_id);

	// assume we fail while holding the lock, so the old machine is us
	st = meta_lock_recover(*meta_shmem_ret, meta_dram,
			       meta_dram->machine_id);
	if (st != TRANSFERED) {
		// some other machine might be recovering; just grab the lock
		// PRINT_INFO("lock transfer failed, need to grab the lock\n");
		meta_lock(*meta_shmem_ret, meta_dram);
		st = TRANSFERED;
	}
	ret = recover_with_redolog(*meta_shmem_ret, meta_dram);
	if (ret != 0) {
		meta_unlock(*meta_shmem_ret, meta_dram);
		return -ret;
	}
	crash_here(1, "crash in recover with lock");
	// PRINT_INFO("done recover shmem, now recovering index\n");

	// done with updating the shmem data structures; reconstruct the dram data structure;
	obj_hdr = obj_header_head(meta_dram->mem_start, *meta_shmem_ret);
	while (obj_hdr != NULL) {
		PRINT_INFO(
			"machine_id: %u, slot_id: %u, prog_id: %s, length: %" PRIu64
			"\n",
			obj_hdr->machine_id, obj_hdr->slot_id, obj_hdr->prog_id,
			obj_hdr->length);
		// if (obj_hdr->machine_id == meta_dram->machine_id) {
		// 	// PRINT_INFO("insert obj_hdr: %p with prog_id %s\n", obj_hdr, obj_hdr->prog_id);
		// 	shmem_index_insert(&name_to_obj, obj_hdr->prog_id,
		// 			   obj_hdr);
		// }
		if (shmem_optr_is_null(obj_hdr->le.pe_next)) {
			break;
		}
		obj_hdr = next_obj_header(meta_dram->mem_start, obj_hdr);
	}

	meta_dram->cur_obj_recs = NULL;
	obj_alloc_recs =
		obj_alloc_recs_head(meta_dram->mem_start, *meta_shmem_ret);
	// DEV_INFO(dev, "first obj_alloc_recs offset: %" PRIu64 "\n",
	// 	 (*meta_shmem_ret)->obj_allocs.pe_first.off);
	while (obj_alloc_recs != NULL) {
		if (obj_alloc_recs->slot_map.available_slots != 0) {
			meta_dram->cur_obj_recs = obj_alloc_recs;
			break;
		}
		if (shmem_optr_is_null(obj_alloc_recs->le.pe_next)) {
			break;
		}
		obj_alloc_recs = next_obj_allocation_records(
			meta_dram->mem_start, obj_alloc_recs);
	}
	meta_unlock(*meta_shmem_ret, meta_dram);
	if (meta_dram->cur_obj_recs == NULL) {
		PRINT_WARN(
			"Can't find empty obj header allocations pool; need to allocate one\n");
	}
	return 0;
}

int check_allocation(struct region_desc *check, struct meta_dram *meta_dram,
		     struct meta_shmem *meta_shmem)
{
	// check dram index is recovered correctly
	struct obj_header *obj_hdr;
	struct buddy_tree_pos pos;
	struct internal_position pos_internal;
	size_t pos_status;
	struct buddy_tree *t;
	struct obj_allocation_records *obj_allocs;
	uint64_t obj_allocs_offset;
	uint16_t calc_slot_id, alloced_slot, total_slots;
	bool found = false;
	size_t src_prog_id_len;

	meta_lock(meta_shmem, meta_dram);
	// found = lookup_shmem_alloc_record(&name_to_obj, check->prog_id,
	// 				  &obj_hdr);
	// if (!found) {
	src_prog_id_len = strlen(check->prog_id);

	obj_hdr = obj_header_head(meta_dram->mem_start, meta_shmem);
	while (obj_hdr != NULL) {
		if (strlen(obj_hdr->prog_id) == src_prog_id_len) {
			if (strncmp(obj_hdr->prog_id, check->prog_id,
				    src_prog_id_len) == 0) {
				found = true;
				break;
			}
		}
		if (shmem_optr_is_null(obj_hdr->le.pe_next)) {
			break;
		}
		obj_hdr = next_obj_header(meta_dram->mem_start, obj_hdr);
	}
	// }
	if (!found) {
		meta_unlock(meta_shmem, meta_dram);
		PRINT_ERR("allocation not found: id %s\n", check->prog_id);
		return EIDX_NOT_FOUND;
	}

	obj_allocs_offset = get_obj_allocation_records_offset(
		obj_hdr, meta_dram->mem_start);
	obj_allocs = (struct obj_allocation_records *)(meta_dram->mem_start +
						       obj_allocs_offset);
	calc_slot_id = (uint16_t)((uint8_t *)obj_hdr -
				  (uint8_t *)&obj_allocs->obj_headers[0]) /
		       sizeof(struct obj_header);

	total_slots = SIZEOF_OBJ_BITSET * sizeof(uint8_t);
	/* slot should be seted */
	BUG_ON(bitset_test(obj_allocs->slot_map.obj_alloc_bitset,
			   calc_slot_id) == 0);
	alloced_slot = (uint16_t)bitset_count_range(
		obj_allocs->slot_map.obj_alloc_bitset, 0, total_slots);
	BUG_ON(NUM_OBJ_SLOTS - alloced_slot !=
	       obj_allocs->slot_map.available_slots);

	/* Check the metadata in obj header */
	BUG_ON((uintptr_t)(uint8_t *)&obj_allocs->obj_headers[obj_hdr->slot_id] !=
	       (uintptr_t)(uint8_t *)obj_hdr);
	BUG_ON(calc_slot_id != obj_hdr->slot_id);

	BUG_ON(meta_dram->machine_id != obj_hdr->machine_id);
	BUG_ON(obj_hdr->length != check->length);
	BUG_ON(obj_hdr->obj_optr.off != check->offset);
	BUG_ON(strlen(obj_hdr->prog_id) != strlen(check->prog_id));
	BUG_ON(obj_hdr->prog_id == NULL);
	BUG_ON(strncmp(obj_hdr->prog_id, check->prog_id,
		       strlen(check->prog_id)) != 0);

	/* Check the buddy bitmap should be allocated */
	pos = buddy_free_find_pos_lockheld(meta_shmem,
					   meta_dram->mem_start + check->offset,
					   check->length);
	t = (struct buddy_tree *)meta_shmem->buddy_alloc.buddy_tree;
	pos_internal = buddy_tree_internal_position_tree(t, pos);
	pos_status =
		read_from_internal_position(buddy_tree_bits(t), pos_internal);
	/* Should be allocated */
	BUG_ON(pos_status != pos_internal.local_offset);

	meta_unlock(meta_shmem, meta_dram);
	return 0;
}
#pragma GCC diagnostic pop

int alloc_and_log_lockheld(struct log_entry entries[],
			   struct buddy *buddy_alloc,
			   struct meta_dram *meta_dram, uint64_t length,
			   int *redolog_pos_ptr, __u8 **obj_out)
{
	struct buddy_tree_pos pos;
	__u8 *obj;
	pos = buddy_malloc_find_pos(buddy_alloc, length);
	if (!buddy_tree_valid(buddy_tree(buddy_alloc), pos)) {
		PRINT_ERR(
			PFX
			"buddy_malloc buddy tree not valid, requested length: %" PRIu64
			", buddy tree depth %lu, index %lu, index upper bound %lu\n",
			length, pos.depth, pos.index,
			buddy_tree(buddy_alloc)->upper_pos_bound);
		return -ENOMEM;
	}
	set_buddyop_log_entry(entries, redolog_pos_ptr, BUDDY_ALLOC, pos);
	obj = address_for_position(buddy_alloc, pos);
	if (obj == NULL) {
		PRINT_ERR(PFX
			  "buddy_malloc no memory, requested length: %" PRIu64
			  ", buddy tree depth %lu, index %lu\n",
			  length, pos.depth, pos.index);
		return -ENOMEM;
	}
	if (obj < meta_dram->mem_start ||
	    obj > meta_dram->mem_start + meta_dram->mem_length) {
		PRINT_ERR(PFX "buddy_malloc returned invalid address: %p\n",
			  obj);
		return -EFAULT;
	}
	*obj_out = obj;
	return 0;
}

/**
 * @brief allocate memory from the shared memory, assuming meta_lock is held
 *
 * @param meta_shmem metadata structure
 * @param alloc_size allocation size
 * @param dev pointer to pci_dev; used for logging. Userspace uses NULL
 * @param allocated return pointer to the allocated memory; pointer should point to the user data not header
 * @return int 0 on success, negative on failure
 */
static int atomic_allocate_memory_lockheld(struct meta_shmem *meta_shmem,
					   struct meta_dram *meta_dram,
					   struct region_desc *alloc,
					   __u8 **allocated)
{
	__u8 *obj;
	struct shmem_optr obj_optr;
	int redolog_pos = 0;
	struct obj_header *obj_header;
	int ret;

	ret = alloc_and_log_lockheld(&meta_shmem->redolog[0],
				     &meta_shmem->buddy_alloc, meta_dram,
				     alloc->length, &redolog_pos, &obj);
	if (ret) {
		return ret;
	}

	obj_optr = shmem_optr_from_ptr((uintptr_t)meta_dram->mem_start, obj);
	/* The obj should be aligned to alignment of struct obj_header */
	// PRINT_INFO("atomic_allocate_memory prog_id: %s\n", alloc->prog_id);
	obj_header = record_obj_hdr_alloc(meta_shmem->redolog,
					  &meta_shmem->buddy_alloc,
					  &meta_shmem->obj_allocs, &redolog_pos,
					  meta_dram, obj_optr, alloc->length,
					  alloc->prog_id);
	record_append_obj(meta_shmem, &redolog_pos, obj_header,
			  meta_dram->mem_start);
	set_commit_log_entry(meta_shmem->redolog, &redolog_pos);
	crash_here(2, "crash before flush and sfence\n");
	shmem_output_cacheline(meta_shmem->redolog,
			       ((size_t)redolog_pos) *
				       sizeof(struct log_entry));
	crash_here(3, "crash after flush and sfence\n");
	execute_log(&meta_shmem->redolog[0], redolog_pos,
		    &meta_shmem->buddy_alloc, meta_dram);
	flush_meta(meta_shmem);

	/* add allocation to in memory index */
	// shmem_index_insert(&name_to_obj, alloc->prog_id, obj_header);
	*allocated = obj;
	return 0;
}

/**
 * @brief allocate memory from the shared memory
 *
 * @param meta_shmem metadata structure
 * @param alloc_size allocation size
 * @param dev pointer to pci_dev; used for logging. Userspace uses NULL
 * @param allocated return pointer to the allocated memory; pointer should point to the user data not header
 * @return int 0 on success, negative on failure
 */
int atomic_allocate_memory(struct meta_shmem *meta_shmem,
			   struct meta_dram *meta_dram,
			   struct region_desc *alloc, __u8 **allocated)
{
	int ret;
	meta_lock(meta_shmem, meta_dram);
	crash_here(11, "crash in allocation");
	barrier();
	ret = atomic_allocate_memory_lockheld(meta_shmem, meta_dram, alloc,
					      allocated);
	meta_unlock(meta_shmem, meta_dram);
	return ret;
}

/**
 * @brief free memory from the shared memory
 *
 * @param meta_shmem metadata structure
 * @param dev pointer to pci_dev; used for logging. Userspace uses NULL
 * @return int 0 on success, negative on failure
 */
int atomic_free_memory(struct meta_shmem *meta_shmem,
		       struct meta_dram *meta_dram,
		       struct region_desc *free_arg, void *dev)
{
	uint64_t obj_offset = free_arg->offset;
	__u8 *addr_to_free = meta_dram->mem_start + obj_offset;
	struct buddy_tree_pos pos;
	int redolog_pos = 0;
	bool found = false;
	struct obj_header *obj_hdr = NULL;
	size_t src_prog_id_len;
	DEV_INFO(dev, "offset to free: 0x%" PRIx64 ", prog_id: %s\n",
		 obj_offset, free_arg->prog_id);

	// found_in_index = lookup_shmem_alloc_record(
	// 	&name_to_obj, free_arg->prog_id, &obj_header);
	// if (!found_in_index) {
	// 	/* all allocation should be in index; assuming the allocator should deallocate its allocation */
	// 	DEV_INFO(dev,
	// 		 PFX "allocation with id %s not found in dram index\n",
	// 		 free_arg->prog_id);
	// 	return 0;
	// }
	src_prog_id_len = strlen(free_arg->prog_id);

	meta_lock(meta_shmem, meta_dram);
	obj_hdr = obj_header_head(meta_dram->mem_start, meta_shmem);
	while (obj_hdr != NULL) {
		PRINT_INFO(
			"machine_id: %u, slot_id: %u, prog_id: %s, length: %" PRIu64
			", prog_id len: %lu, src len: %lu\n",
			obj_hdr->machine_id, obj_hdr->slot_id, obj_hdr->prog_id,
			obj_hdr->length, strlen(obj_hdr->prog_id),
			src_prog_id_len);
		if (strlen(obj_hdr->prog_id) == src_prog_id_len) {
			int ret = strncmp(obj_hdr->prog_id, free_arg->prog_id,
					  src_prog_id_len);
			PRINT_INFO(
				"found hdr with match size, compare content: %d\n",
				ret);
			if (ret == 0) {
				found = true;
				break;
			}
		}
		if (shmem_optr_is_null(obj_hdr->le.pe_next)) {
			break;
		}
		obj_hdr = next_obj_header(meta_dram->mem_start, obj_hdr);
	}
	if (!found) {
		meta_unlock(meta_shmem, meta_dram);
		/* all allocation should be in index; assuming the allocator should deallocate its allocation */
		DEV_INFO(dev, PFX "allocation with id %s not found in cxl\n",
			 free_arg->prog_id);
		return 0;
	}
	crash_here(12, "crash in free");
	pos = buddy_free_find_pos_lockheld(meta_shmem, addr_to_free,
					   free_arg->length);
	if (!buddy_tree_valid(buddy_tree(&meta_shmem->buddy_alloc), pos)) {
		meta_unlock(meta_shmem, meta_dram);
		DEV_ERR(dev, PFX "buddy_free invalid ptr\n");
		return -EFAULT;
	}
	meta_shmem->redolog[0] = EMPTY_LOG_ENTRY;
	set_buddyop_log_entry(meta_shmem->redolog, &redolog_pos, BUDDY_FREE,
			      pos);
	record_remove_obj(meta_shmem, &redolog_pos, obj_hdr,
			  meta_dram->mem_start);
	record_obj_hdr_free(meta_shmem->redolog, &redolog_pos, meta_dram,
			    obj_hdr);
	set_commit_log_entry(meta_shmem->redolog, &redolog_pos);
	shmem_output_cacheline(meta_shmem->redolog,
			       ((size_t)redolog_pos) *
				       sizeof(struct log_entry));

	execute_log_entries(meta_shmem, redolog_pos, meta_dram);
	// found_in_index = shmem_index_del(&name_to_obj, free_arg->prog_id);
	// if (!found_in_index) {
	// 	DEV_ERR(dev,
	// 		PFX "obj with offset: 0x%" PRIx64
	// 		    "not found but it should\n",
	// 		obj_offset);
	// }

	// buddy_free(&ivpci_dev->meta_shmem->buddy_alloc,
	//     (void *)(ivpci_dev->kernel_mapped_shm + arg.offset));
	meta_unlock(meta_shmem, meta_dram);
	return 0;
}

void find_allocation(struct vcxl_find_alloc *find_alloc,
		     struct meta_shmem *meta_shmem, struct meta_dram *meta_dram)
{
	__u8 *obj = NULL;
	struct obj_header *obj_hdr = NULL;
	// bool ret;
	size_t src_prog_id_len;
	bool found = false;
	find_alloc->existing = 0;

	// ret = lookup_shmem_alloc_record(&name_to_obj, find_alloc->desc.prog_id,
	// 				&obj_hdr);
	// if (ret && likely(obj_hdr != NULL)) {
	// 	find_alloc->existing = 1;
	// 	BUG_ON_ASSERT(strlen(find_alloc->desc.prog_id) !=
	// 		      strlen(obj_hdr->prog_id));
	// 	BUG_ON_ASSERT(strncmp(find_alloc->desc.prog_id,
	// 			      obj_hdr->prog_id,
	// 			      strlen(obj_hdr->prog_id)));
	// 	find_alloc->desc.offset = obj_hdr->obj_optr.off;
	// 	find_alloc->desc.length = obj_hdr->length;
	// } else {
	src_prog_id_len = strlen(find_alloc->desc.prog_id);

	meta_lock(meta_shmem, meta_dram);
	obj_hdr = obj_header_head(meta_dram->mem_start, meta_shmem);
	while (obj_hdr != NULL) {
		if (strlen(obj_hdr->prog_id) == src_prog_id_len) {
			if (strncmp(obj_hdr->prog_id, find_alloc->desc.prog_id,
				    src_prog_id_len) == 0) {
				found = true;
				find_alloc->existing = 1;
				find_alloc->desc.offset = obj_hdr->obj_optr.off;
				find_alloc->desc.length = obj_hdr->length;
				break;
			}
		}
		if (shmem_optr_is_null(obj_hdr->le.pe_next)) {
			break;
		}
		obj_hdr = next_obj_header(meta_dram->mem_start, obj_hdr);
	}
	if (!found) {
		find_alloc->existing = 0;

		// If the length is zero, assume the caller is only interested in
		// checking for the existence of the allocation.
		if (find_alloc->desc.length > 0) {
			atomic_allocate_memory_lockheld(
				meta_shmem, meta_dram, &find_alloc->desc, &obj);
			find_alloc->desc.offset =
				(uint64_t)(obj - meta_dram->mem_start);
		}
	}
	meta_unlock(meta_shmem, meta_dram);
	// }
}
