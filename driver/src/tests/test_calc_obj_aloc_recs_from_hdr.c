#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>
#include "obj_header.h"

void main(void)
{
    // uint8_t* addr = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    // struct obj_allocation_records *obj_alloc_recs = (struct obj_allocation_records *)(addr + 4096);
    struct obj_allocation_records *obj_alloc_recs = calloc(1, sizeof(struct obj_allocation_records));
    struct obj_header* obj_hdr = &obj_alloc_recs->obj_headers[3];
    obj_hdr->slot_id = 3;
    uint64_t oar_offet = get_obj_allocation_records_offset(obj_hdr, (uint8_t *)obj_alloc_recs);
    fprintf(stderr, "calculated offset is 0x%lx\n", oar_offet);
    uintptr_t calculated_ptr = (uintptr_t)((uint8_t *)obj_alloc_recs + oar_offet);
    if ((uintptr_t)obj_alloc_recs != calculated_ptr) {
        fprintf(stderr, "expected obj_alloc_recs: 0x%lx, calculated: 0x%lx\n",
            (uintptr_t)obj_alloc_recs, calculated_ptr);
        abort();
    }
}