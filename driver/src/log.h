#ifndef ALLOC_LOG_H_
#define ALLOC_LOG_H_ 1

#ifndef BUDDY_FRAG_OPTIONAL
#define BUDDY_FRAG_OPTIONAL 1
#endif

#ifndef BUDDY_HEADER
#define BUDDY_HEADER 1
#endif

#include <linux/types.h>
#if defined(__KERNEL__) || defined(MODULE)
#include <linux/build_bug.h>
#define BUDDY_HEADER 1
#else
#include <assert.h>
#endif

#include "buddy_alloc.h"
#include "shmem_obj.h"
#include "spinlock.h"
#include "vcxl_def.h"
#include "uapi/vcxl_udef.h"
#include "list_queue.h"
#include "obj_header.h"
#include "util_checksum.h"
#include "common_macro.h"
#include "circular_queue.h"
#include "mpsc_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// buddy op log entry
// 16 bytes
struct buddyop_log_entry {
	struct buddy_tree_pos pos; /* 16 bytes */
};
static_assert(sizeof(struct buddyop_log_entry) == 16, "size is not 16 bytes");

// 16 bytes
struct val_log_entry {
	struct shmem_optr off;
	uint64_t value;
};
static_assert(sizeof(struct val_log_entry) == 16, "size is not 16 bytes");

struct two_val_log_entry {
	struct shmem_optr off1;
	uint64_t value1;
	struct shmem_optr off2;
	uint64_t value2;
};
static_assert(sizeof(struct two_val_log_entry) == 32, "size is not 32 bytes");

struct str_log_entry {
	struct shmem_optr off;
	/* Nul terminated str */
	char val[PROG_ID_SIZE];
};
static_assert(sizeof(struct str_log_entry) == 24, "size is not 24 bytes");

struct obj_hdr_alloc {
	/* offset ptr to obj_allocation_records */
	struct shmem_optr alloc_rec_ptr;
	struct shmem_optr obj_ptr;
	uint64_t length;
	uint16_t slot;
	uint16_t available_slots;
	unsigned char prog_id[PROG_ID_SIZE];
	uint16_t machine_id;
	uint8_t prog_id_size;
	unsigned char unused[5];
};
static_assert(sizeof(struct obj_hdr_alloc) == 48, "size is not 48 bytes");
static_assert(_Alignof(struct obj_hdr_alloc) == 8, "");

struct obj_hdr_free {
	/* offset ptr to obj_allocation_records */
	struct shmem_optr alloc_rec_ptr;
	uint16_t slot;
	uint16_t available_slots;
};
static_assert(sizeof(struct obj_hdr_free) == 16, "size is not 16 bytes");

struct init_obj_alloc_recs_entry {
	struct shmem_optr new_obj_alloc_entry;
};
static_assert(sizeof(struct init_obj_alloc_recs_entry) == 8,
	      "size is not 8 bytes");

// 8 bytes
struct commit_log_entry {
	uint64_t check_sum;
};
static_assert(sizeof(struct commit_log_entry) == 8, "size is not 8 bytes");

enum entry_type {
	INVALID = 0,
	BUDDY_ALLOC = 1,
	BUDDY_FREE = 2,
	VAL = 3,
	STR = 4,
	OBJ_HDR_ALLOC = 5,
	OBJ_HDR_FREE = 6,
	TWO_VAL_LOG = 7,
	INIT_OBJ_ALLOC_RECS = 8,
	COMMIT = 9,
};

// size: 56 bytes
struct log_entry {
	enum entry_type tp;
	union {
		struct buddyop_log_entry buddyop_loge;
		struct val_log_entry val_loge;
		struct two_val_log_entry two_val_loge;
		struct str_log_entry str_loge;
		struct obj_hdr_alloc obj_hdr_alloc_loge;
		struct obj_hdr_free obj_hdr_free_loge;
		struct init_obj_alloc_recs_entry init_obj_alloc_loge;
		struct commit_log_entry commit_loge;
	} entry;
};
_Static_assert(sizeof(struct log_entry) == 56, "");

static const struct log_entry EMPTY_LOG_ENTRY = { 0 };

/* The log entry operations erase the next op to zero */
/**
 * Set the log entry to be a buddy op log entry
*/
void set_buddyop_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
			   enum entry_type tp, struct buddy_tree_pos pos);

void set_value_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
			 struct shmem_optr off, uint64_t value);

void set_two_value_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
			     struct shmem_optr off1, uint64_t value1,
			     struct shmem_optr off2, uint64_t value2);

void set_str_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
		       struct shmem_optr off, char prog_id[]);

void set_obj_hdr_alloc_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
				 struct shmem_optr alloc_rec_ptr,
				 struct shmem_optr obj_ptr, uint64_t length,
				 const unsigned char *prog_id, uint16_t slot,
				 uint16_t machine_id, uint16_t available_slots);

void set_obj_hdr_free_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
				struct shmem_optr alloc_rec_ptr, uint16_t slot,
				uint16_t available_slots);

void set_init_obj_allocs_log_entry(struct log_entry entry[],
				   int *redolog_pos_ptr,
				   struct shmem_optr init_alloc_ptr);

void set_commit_log_entry(struct log_entry entries[], int *redolog_pos_ptr);

struct obj_alloc_recs_list {
	struct obj_allocation_records *head;
	struct obj_allocation_records *tail;
};

struct obj_alloc_recs_entry {
	struct obj_allocation_records *next;
	struct obj_allocation_records *prev;
};

/* Metadata stored in dram */
struct meta_dram {
	uint8_t *mem_start;
	uint64_t mem_length;
	struct obj_allocation_records *cur_obj_recs;
	struct obj_alloc_recs_list partial;
	/* for user space test, process_id is stored in machine_id */
	uint16_t machine_id;
	uint8_t total_machines;
};
struct meta_shmem;

/* machine_id starts from 1 */
static WARN_UNUSED_RESULT inline struct circular_queue *
self_vcxl_addr_queue(struct meta_dram *meta_dram)
{
	if (meta_dram->machine_id < 1) {
		return NULL;
	}
	return (struct circular_queue *)(meta_dram->mem_start +
					 (meta_dram->machine_id - 1) *
						 circular_queue_size);
}

static WARN_UNUSED_RESULT inline struct circular_queue *
peer_vcxl_addr_queue(struct meta_dram *meta_dram, uint16_t peer)
{
	if (peer < 1) {
		return NULL;
	}
	return (struct circular_queue *)(meta_dram->mem_start +
					 (peer - 1) * circular_queue_size);
}

static WARN_UNUSED_RESULT inline struct circular_queue *
self_vcxl_procdeath_queue(struct meta_dram *meta_dram)
{
	if (meta_dram->machine_id < 1) {
		return NULL;
	}
	return (struct circular_queue *)(meta_dram->mem_start +
					 FUTEX_ADDR_QUEUE_SIZE +
					 OBJ_ALLOC_RECS_SIZE +
					 (meta_dram->machine_id - 1) *
						 circular_queue_size);
}

static WARN_UNUSED_RESULT inline struct circular_queue *
peer_vcxl_procdeath_queue(struct meta_dram *meta_dram, uint16_t peer)
{
	if (peer < 1) {
		return NULL;
	}
	return (struct circular_queue *)(meta_dram->mem_start +
					 FUTEX_ADDR_QUEUE_SIZE +
					 OBJ_ALLOC_RECS_SIZE +
					 (peer - 1) * circular_queue_size);
}

void execute_log(struct log_entry entries[], int length, struct buddy *buddy,
		 struct meta_dram *meta_dram);

void record_list_queue_append(struct log_entry redolog[], int *redolog_pos_ptr,
			      struct list_queue *lq, struct list_entry *le,
			      uint8_t *mem_start);
void record_list_queue_remove(struct log_entry redolog[], int *redolog_pos_ptr,
			      struct list_queue *lq, struct list_entry *le,
			      uint8_t *mem_start);

static inline void *shm_off_to_virtual_addr(struct meta_dram *meta_dram,
					    __u64 offset)
{
	if (offset == 0) {
		return NULL;
	}
	return (void *)(meta_dram->mem_start + offset);
}

struct obj_header *
record_obj_hdr_alloc(struct log_entry entries[], struct buddy *buddy_alloc,
		     struct list_queue *obj_allocs, int *redolog_pos_ptr,
		     struct meta_dram *meta_dram, struct shmem_optr obj_ptr,
		     uint64_t length, const unsigned char *prog_id);
void record_obj_hdr_free(struct log_entry entries[], int *redolog_pos_ptr,
			 struct meta_dram *meta_dram,
			 struct obj_header *obj_hdr);
int alloc_and_log_lockheld(struct log_entry entries[],
			   struct buddy *buddy_alloc,
			   struct meta_dram *meta_dram, uint64_t length,
			   int *redolog_pos_ptr, __u8 **obj_out);

#ifdef __cplusplus
}
#endif

#endif // ALLOC_LOG_H_
