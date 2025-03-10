#include <linux/types.h>
#include "shmem_ops.h"
#include "log.h"
#include "obj_header.h"
#include "bitset.h"
#include "fault_inject.h"

void execute_log(struct log_entry entries[], int length, struct buddy *buddy,
		 struct meta_dram *meta_dram)
{
	int i;
	uint64_t *dst, *dst2;
	char *dst_str;
	char *data;
	char *prog_id;
	size_t data_size, prog_id_size;
	struct obj_allocation_records *obj_alloc_recs;
	uint16_t slot;
	struct shmem_optr obj_ptr;

	for (i = 0; i < length; i++) {
		switch (entries[i].tp) {
		case BUDDY_ALLOC:
			buddy_malloc_apply(buddy,
					   entries[i].entry.buddyop_loge.pos);
			break;
		case BUDDY_FREE:
			buddy_free_apply(buddy,
					 entries[i].entry.buddyop_loge.pos);
			break;
		case VAL:
			dst = (uint64_t *)((uintptr_t)meta_dram->mem_start +
					   entries[i].entry.val_loge.off.off);
			*dst = entries[i].entry.val_loge.value;
			shmem_flush_cpu_cache(dst, sizeof(uint64_t));
			break;
		case TWO_VAL_LOG:
			dst = (uint64_t *)shmem_optr_direct_ptr(
				(uintptr_t)meta_dram->mem_start,
				entries[i].entry.two_val_loge.off1);
			BUG_ON(dst == NULL);
			*dst = entries[i].entry.two_val_loge.value1;
			dst2 = (uint64_t *)shmem_optr_direct_ptr(
				(uintptr_t)meta_dram->mem_start,
				entries[i].entry.two_val_loge.off2);
			BUG_ON(dst2 == NULL);
			*dst2 = entries[i].entry.two_val_loge.value2;
			crash_here(4, "crash before two flush in two_val_log");
			shmem_flush_cpu_cache(dst, sizeof(uint64_t));
			crash_here(5, "crash between two flush in two_val_log");
			shmem_flush_cpu_cache(dst2, sizeof(uint64_t));
			break;
		case STR:
			data = entries[i].entry.str_loge.val;
			data_size = strlen(data) + 1;
			dst_str = (char *)((uintptr_t)meta_dram->mem_start +
					   entries[i].entry.str_loge.off.off);
			memcpy(dst_str, data, data_size);
			shmem_flush_cpu_cache(dst_str, data_size);
			break;
		case OBJ_HDR_ALLOC:
			slot = entries[i].entry.obj_hdr_alloc_loge.slot;
			obj_ptr = entries[i].entry.obj_hdr_alloc_loge.obj_ptr;
			obj_alloc_recs = (struct obj_allocation_records *)
				shmem_optr_direct_ptr(
					(uintptr_t)meta_dram->mem_start,
					entries[i]
						.entry.obj_hdr_alloc_loge
						.alloc_rec_ptr);
			BUG_ON(obj_alloc_recs == NULL);

			bitset_set(obj_alloc_recs->slot_map.obj_alloc_bitset,
				   slot);
			obj_alloc_recs->slot_map.available_slots =
				entries[i]
					.entry.obj_hdr_alloc_loge
					.available_slots;

			obj_alloc_recs->obj_headers[slot].obj_optr = obj_ptr;
			obj_alloc_recs->obj_headers[slot].length =
				entries[i].entry.obj_hdr_alloc_loge.length;
			obj_alloc_recs->obj_headers[slot].machine_id =
				entries[i].entry.obj_hdr_alloc_loge.machine_id;
			obj_alloc_recs->obj_headers[slot].slot_id = slot;

			prog_id = entries[i].entry.obj_hdr_alloc_loge.prog_id;
			prog_id_size =
				entries[i].entry.obj_hdr_alloc_loge.prog_id_size;
			BUG_ON_ASSERT(strlen(prog_id) + 1 != prog_id_size);
			memcpy(obj_alloc_recs->obj_headers[slot].prog_id,
			       prog_id, prog_id_size);
			BUG_ON_ASSERT(strncmp(obj_alloc_recs->obj_headers[slot]
						      .prog_id,
					      prog_id, prog_id_size) != 0);
			// PRINT_INFO("alloc obj_hdr slot: %u, prog_id in loge: %s, prog_id in hdr: %s\n",
			//     slot, prog_id, obj_alloc_recs->obj_headers[slot].prog_id);

			crash_here(
				6,
				"crash before flush cpu cache in obj hdr alloc\n");
			/* flush both the bitset and the available slot */
			shmem_flush_cpu_cache(
				(uint8_t *)&obj_alloc_recs->slot_map,
				sizeof(obj_alloc_recs->slot_map));
			crash_here(7, "crash before flush obj hdr\n");
			shmem_flush_cpu_cache(
				(uint8_t *)&obj_alloc_recs->obj_headers[slot],
				sizeof(struct obj_header));
			crash_here(8, "crash after flush obj hdr\n");
			break;
		case OBJ_HDR_FREE:
			slot = entries[i].entry.obj_hdr_free_loge.slot;
			obj_alloc_recs = (struct obj_allocation_records *)
				shmem_optr_direct_ptr(
					(uintptr_t)meta_dram->mem_start,
					entries[i]
						.entry.obj_hdr_alloc_loge
						.alloc_rec_ptr);
			BUG_ON(obj_alloc_recs == NULL);
			bitset_clear(obj_alloc_recs->slot_map.obj_alloc_bitset,
				     slot);
			obj_alloc_recs->slot_map.available_slots =
				entries[i]
					.entry.obj_hdr_free_loge.available_slots;
			crash_here(9, "crash before flush slot_map\n");
			shmem_flush_cpu_cache(
				(uint8_t *)&obj_alloc_recs->slot_map,
				sizeof(obj_alloc_recs->slot_map));
			crash_here(10, "crash after flush slot_map\n");
			break;
		case INIT_OBJ_ALLOC_RECS:
			obj_alloc_recs = (struct obj_allocation_records *)
				shmem_optr_direct_ptr(
					(uintptr_t)meta_dram->mem_start,
					entries[i]
						.entry.init_obj_alloc_loge
						.new_obj_alloc_entry);
			BUG_ON(obj_alloc_recs == NULL);
			init_obj_allocation_records(obj_alloc_recs);
			meta_dram->cur_obj_recs = obj_alloc_recs;
			shmem_flush_cpu_cache(
				obj_alloc_recs,
				sizeof(struct obj_allocation_records));
			break;
		case COMMIT:
			break;
		case INVALID:
			PRINT_INFO("Invalid log entry type\n");
			break;
		default:
			PRINT_INFO("Unrecognized log entry type: %u\n",
				   (unsigned int)entries[i].tp);
			break;
		}
	}
}

/* The log entry operations erase the next op to zero */
/**
 * Set the log entry to be a buddy op log entry
*/
void set_buddyop_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
			   enum entry_type tp, struct buddy_tree_pos pos)
{
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = tp;
	crash_here(13, "crash in %s", __func__);
	entry[*redolog_pos_ptr].entry.buddyop_loge.pos = pos;
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_value_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
			 struct shmem_optr off, uint64_t value)
{
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = VAL;
	entry[*redolog_pos_ptr].entry.val_loge.off = off;
	crash_here(14, "crash in %s", __func__);
	entry[*redolog_pos_ptr].entry.val_loge.value = value;
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_two_value_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
			     struct shmem_optr off1, uint64_t value1,
			     struct shmem_optr off2, uint64_t value2)
{
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = TWO_VAL_LOG;
	entry[*redolog_pos_ptr].entry.two_val_loge.off1 = off1;
	crash_here(15, "crash in %s", __func__);
	entry[*redolog_pos_ptr].entry.two_val_loge.value1 = value1;
	entry[*redolog_pos_ptr].entry.two_val_loge.off2 = off2;
	entry[*redolog_pos_ptr].entry.two_val_loge.value2 = value2;
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_str_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
		       struct shmem_optr off, char prog_id[])
{
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = STR;
	entry[*redolog_pos_ptr].entry.str_loge.off = off;
	crash_here(16, "crash in %s", __func__);
	memcpy(entry[*redolog_pos_ptr].entry.str_loge.val, prog_id,
	       strlen(prog_id) + 1);
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_obj_hdr_alloc_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
				 struct shmem_optr alloc_rec_ptr,
				 struct shmem_optr obj_offset, uint64_t length,
				 const unsigned char *prog_id, uint16_t slot,
				 uint16_t machine_id, uint16_t available_slots)
{
	size_t prog_id_size = strlen(prog_id) + 1;
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = OBJ_HDR_ALLOC;
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.alloc_rec_ptr =
		alloc_rec_ptr;
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.slot = slot;
	crash_here(17, "crash in %s", __func__);
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.obj_ptr = obj_offset;
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.length = length;
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.available_slots =
		available_slots;
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.prog_id_size =
		(uint8_t)prog_id_size;
	memcpy(entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.prog_id,
	       prog_id, prog_id_size);
	entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.machine_id =
		machine_id;

	// PRINT_INFO("the prog_id in log entry is %s, slot %u\n", entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.prog_id,
	//     entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.slot);
	BUG_ON_ASSERT(
		strncmp(entry[*redolog_pos_ptr].entry.obj_hdr_alloc_loge.prog_id,
			prog_id, prog_id_size) != 0);
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_obj_hdr_free_log_entry(struct log_entry entry[], int *redolog_pos_ptr,
				struct shmem_optr alloc_rec_ptr, uint16_t slot,
				uint16_t available_slots)
{
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = OBJ_HDR_FREE;
	entry[*redolog_pos_ptr].entry.obj_hdr_free_loge.alloc_rec_ptr =
		alloc_rec_ptr;
	entry[*redolog_pos_ptr].entry.obj_hdr_free_loge.slot = slot;
	crash_here(18, "crash in %s", __func__);
	entry[*redolog_pos_ptr].entry.obj_hdr_free_loge.available_slots =
		available_slots;
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_init_obj_allocs_log_entry(struct log_entry entry[],
				   int *redolog_pos_ptr,
				   struct shmem_optr init_alloc_ptr)
{
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);
	entry[*redolog_pos_ptr].tp = INIT_OBJ_ALLOC_RECS;
	crash_here(19, "crash in %s", __func__);
	entry[*redolog_pos_ptr].entry.init_obj_alloc_loge.new_obj_alloc_entry =
		init_alloc_ptr;
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entry[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}

void set_commit_log_entry(struct log_entry entries[], int *redolog_pos_ptr)
{
	uint64_t csum;
	BUG_ON(*redolog_pos_ptr == REDOLOG_ENTRIES);

	entries[*redolog_pos_ptr].tp = COMMIT;
	crash_here(20, "crash in %s", __func__);
	csum = util_checksum_compute(entries,
				     ((uint64_t)*redolog_pos_ptr) *
					     sizeof(struct log_entry),
				     NULL, 0);
	entries[*redolog_pos_ptr].entry.commit_loge.check_sum = csum;
	*redolog_pos_ptr += 1;
	if (*redolog_pos_ptr < REDOLOG_ENTRIES) {
		entries[*redolog_pos_ptr] = EMPTY_LOG_ENTRY;
	}
}
