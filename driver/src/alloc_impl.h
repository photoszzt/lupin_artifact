#ifndef VCXL_ALLOC_H_
#define VCXL_ALLOC_H_ 1

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <linux/types.h>
#include <linux/compiler.h>
#pragma GCC diagnostic pop

#include "log.h"
#include "common_macro.h"
#include "buddy_alloc.h"
#include "uapi/vcxl_shm.h"
#include "cxl_dev.h"

#define BUDDY_ALLOC_ALIGNMENT 4096

struct meta_shmem;

void assign_meta_info(struct meta_info *info, struct meta_shmem *meta_shmem,
		      struct meta_dram *meta_dram);
int vcxl_init_meta_impl(struct vcxl_ivpci_device *ivpci_dev,
			struct meta_dram *meta_dram, void *dev,
			struct meta_shmem **meta_shmem_ret);
void vcxl_hash_index_exit(void);
int recover_meta(struct vcxl_ivpci_device *ivpci_dev,
		 struct meta_dram *meta_dram, void *dev,
		 struct meta_shmem **meta_shmem_ret);
int atomic_allocate_memory(struct meta_shmem *meta_shmem,
			   struct meta_dram *meta_dram,
			   struct region_desc *alloc, __u8 **allocated);
int atomic_free_memory(struct meta_shmem *meta_shmem,
		       struct meta_dram *meta_dram,
		       struct region_desc *free_arg, void *dev);
int recover_with_redolog(struct meta_shmem *meta_shmem,
			 struct meta_dram *meta_dram);
void find_allocation(struct vcxl_find_alloc *find_alloc,
		     struct meta_shmem *meta_shmem,
		     struct meta_dram *meta_dram);
int check_allocation(struct region_desc *check, struct meta_dram *meta_dram,
		     struct meta_shmem *meta_shmem);

size_t metashmem_sizeof_alignment(size_t memory_size, size_t alignment);
struct buddy_embed_check metashmem_embed_offset(size_t memory_size,
						size_t alignment);
struct meta_shmem *
metashmem_init_alignment(unsigned char *at, unsigned char *main,
			 size_t memory_size, size_t alignment,
			 size_t *buddy_tree_size, bool output_cache_line);
struct meta_shmem *metashmem_embed_alignment(unsigned char *main,
					     size_t memory_size,
					     size_t alignment);
WARN_UNUSED_RESULT enum transfer_st
meta_lock_recover(struct meta_shmem *meta_shmem, struct meta_dram *meta_dram,
		  u16 old_machine_id);
void meta_lock_done_recover(struct meta_shmem *meta_shmem,
			    struct meta_dram *meta_dram, enum transfer_st st);
inline WARN_UNUSED_RESULT struct obj_header *
obj_header_head(uint8_t *mem_start, struct meta_shmem *meta_shmem);
inline WARN_UNUSED_RESULT struct obj_allocation_records *
obj_alloc_recs_head(uint8_t *mem_start, struct meta_shmem *meta_shmem);
void print_allocation(uint8_t *mem_start, struct meta_shmem *meta_shmem);

#endif // VCXL_ALLOC_H_
