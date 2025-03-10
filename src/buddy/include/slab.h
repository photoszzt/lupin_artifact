#ifndef SLAB_H
#define SLAB_H

#include <stdint.h>
#include <stdbool.h>

#include "shmem_obj.h"

union value_oid {
    uint8_t *value;
    struct shmem_oid val_oid;
};

union chunk_oid {
    struct chunk *chunks;
    struct shmem_oid chunk_oid;
};

union slab_oid {
    struct slab *slab;
    struct shmem_oid slab_oid;
};

union slab_list_oid {
    struct slab_list *slab_list;
    struct shmem_oid slab_list_oid;
};

union slab_allocator_oid {
    struct slab_allocator *slab_allocator;
    struct shmem_oid slab_allocator_oid;
};

struct chunk {
    bool flag;
    union value_oid value;
};

struct slab {
    union chunk_oid chunks; /* list of items */
    union slab_oid next;
};

struct slab_list {
    union slab_oid head;
    union slab_oid tail;
    union slab_list_oid next;
};

/* slab allocator 
 * 12 + 4*8 = 44 bytes
 * size of returns 48 bytes
**/
struct slab_allocator {
    uint32_t slab_count; /* how many slabs are currently allocated(full + partial) */
    uint32_t chunk_size; /* size of each chunk */
    uint32_t chunk_count; /* how many chunks are in each slab */
    union slab_list_oid slabs_full_head; /* address of head of the queue for full/partial/empty slabs */
    union slab_list_oid slabs_partial_head;
    union slab_list_oid slabs_empty_head;

    union slab_allocator_oid next; /* next slab allocator */
};

struct mem_allocator {
    struct buddy *buddy;
    union slab_allocator_oid allocators_head;
    union slab_allocator_oid allocators_tail;
    uint8_t num_allocators;
};

#endif // SLAB_H