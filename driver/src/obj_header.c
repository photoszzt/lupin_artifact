#include "bitset.h"
#include "log.h"
#include "common_macro.h"
#if !defined(__KERNEL__) && !defined(MODULE)
#include <string.h>
#endif

void init_obj_allocation_records(struct obj_allocation_records *records)
{
	uint16_t i = 0;
	records->le.pe_next.off = 0;
	records->le.pe_prev.off = 0;
	memset(records->slot_map.obj_alloc_bitset, 0, SIZEOF_OBJ_BITSET);
	records->slot_map.available_slots = NUM_OBJ_SLOTS;
	for (i = 0; i < NUM_OBJ_SLOTS; i++) {
		records->obj_headers[i].slot_id = i;
	}
}

struct obj_header *
record_obj_hdr_alloc(struct log_entry entries[], struct buddy *buddy_alloc,
		     struct list_queue *obj_allocs, int *redolog_pos_ptr,
		     struct meta_dram *meta_dram, struct shmem_optr obj_ptr,
		     uint64_t length, const unsigned char *prog_id)
{
	uint16_t i = 0;
	uint16_t slot = 0;
	struct shmem_optr recs_optr;
	struct obj_header *ret_hdr;
	uint16_t available_slots =
		meta_dram->cur_obj_recs->slot_map.available_slots - 1;
	recs_optr = shmem_optr_from_ptr((uintptr_t)meta_dram->mem_start,
					meta_dram->cur_obj_recs);

	// PRINT_INFO("record_obj_hdr_alloc passed in prog_id: %s\n", prog_id);
	if (meta_dram->cur_obj_recs->slot_map.available_slots > 0) {
		for (i = 0; i < NUM_OBJ_SLOTS; i++) {
			if (!bitset_test(meta_dram->cur_obj_recs->slot_map
						 .obj_alloc_bitset,
					 i)) {
				slot = i;
				break;
			}
		}
		ret_hdr = &meta_dram->cur_obj_recs->obj_headers[slot];
	} else {
		// TODO: Need to allocate a new obj_hdr
		int ret;
		__u8 *obj;
		ret = alloc_and_log_lockheld(
			entries, buddy_alloc, meta_dram,
			sizeof(struct obj_allocation_records), redolog_pos_ptr,
			&obj);
		if (ret) {
			return NULL;
		}
		record_list_queue_append(
			entries, redolog_pos_ptr, obj_allocs,
			&((struct obj_allocation_records *)obj)->le,
			meta_dram->mem_start);
		recs_optr = shmem_optr_from_ptr((uintptr_t)meta_dram->mem_start,
						obj);
		set_init_obj_allocs_log_entry(entries, redolog_pos_ptr,
					      recs_optr);
		slot = 0;
		available_slots = NUM_OBJ_SLOTS - 1;
		ret_hdr =
			&((struct obj_allocation_records *)obj)->obj_headers[0];
	}
	set_obj_hdr_alloc_log_entry(entries, redolog_pos_ptr, recs_optr,
				    obj_ptr, length, prog_id, slot,
				    meta_dram->machine_id, available_slots);
	return ret_hdr;
}

void record_obj_hdr_free(struct log_entry entries[], int *redolog_pos_ptr,
			 struct meta_dram *meta_dram,
			 struct obj_header *obj_hdr)
{
	struct shmem_optr alloc_recs_optr;
	struct obj_allocation_records *alloc_recs;

	alloc_recs_optr.off = get_obj_allocation_records_offset(
		obj_hdr, meta_dram->mem_start);
	alloc_recs = (struct obj_allocation_records *)shmem_optr_direct_ptr(
		(uintptr_t)meta_dram->mem_start, alloc_recs_optr);
	set_obj_hdr_free_log_entry(entries, redolog_pos_ptr, alloc_recs_optr,
				   obj_hdr->slot_id,
				   alloc_recs->slot_map.available_slots + 1);
}
