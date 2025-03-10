#ifndef OBJ_HEADER_H_
#define OBJ_HEADER_H_ 1
#include <linux/types.h>
#if defined(__KERNEL__) || defined(MODULE)
#include <linux/build_bug.h>
#else
#include <assert.h>
#endif

#include "shmem_obj.h"
#include "list_queue.h"
#include "uapi/vcxl_udef.h"
#include "vcxl_def.h"

#ifdef __cplusplus
extern "C" {
#endif

// ensure the header fits in a cacheline
struct obj_header {
	struct list_entry le;
	__u16 machine_id;
	__u16 slot_id; // records which slot the allocation is from
	__u8 prog_id[PROG_ID_SIZE];
	struct shmem_optr obj_optr;
	uint64_t length; // length of the allocation
};
_Static_assert(sizeof(struct obj_header) == 48,
	       "obj header size is not 48 bytes");

#define NUM_OBJ_SLOTS 340
#define SIZEOF_OBJ_BITSET 43
struct obj_allocation_records {
	struct list_entry le;
	struct {
		unsigned char obj_alloc_bitset[SIZEOF_OBJ_BITSET];
		uint16_t available_slots;
	} slot_map;
	struct obj_header obj_headers[NUM_OBJ_SLOTS];
};
_Static_assert(sizeof(struct obj_allocation_records) == OBJ_ALLOC_RECS_SIZE,
	       "obj_allocation_records size is not 16384 bytes");

static inline uint64_t
get_obj_allocation_records_offset(struct obj_header *obj_hdr, __u8 *mem_start)
{
	uint64_t obj_hdr_offset = (uint64_t)((uint8_t *)obj_hdr - mem_start);
	uint64_t obj_headers_offset =
		obj_hdr_offset - obj_hdr->slot_id * sizeof(struct obj_header);
	return obj_headers_offset -
	       offsetof(struct obj_allocation_records, obj_headers);
}

void init_obj_allocation_records(struct obj_allocation_records *records);

static force_inline struct obj_header *
next_obj_header(uint8_t *mem_start, struct obj_header *obj_hdr)
{
	return container_of((struct list_entry *)shmem_optr_direct_ptr(
				    (uintptr_t)mem_start, obj_hdr->le.pe_next),
			    struct obj_header, le);
}

static force_inline struct obj_allocation_records *
next_obj_allocation_records(uint8_t *mem_start,
			    struct obj_allocation_records *obj_alloc_recs)
{
	return container_of((struct list_entry *)shmem_optr_direct_ptr(
				    (uintptr_t)mem_start,
				    obj_alloc_recs->le.pe_next),
			    struct obj_allocation_records, le);
}

#define PROG_ID_OFF (offsetof(struct obj_header, prog_id))
#define OBJ_HEADER_SIZE sizeof(struct obj_header)

#ifdef __cplusplus
}
#endif

#endif // OBJ_HEADER_H_
