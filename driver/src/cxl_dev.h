#ifndef VCXL_DEV_H
#define VCXL_DEV_H 1

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/memremap.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include "uapi/vcxl_shm.h"
#include "log.h"

#define IVPOSITION_OFF 0x08 /* VM ID */
#define DOORBELL_OFF 0x0c /* Doorbell */

#define WAIT_TABLE_BITS 12
#define WAIT_TABLE_SIZE (1 << WAIT_TABLE_BITS)

/**
 * Hash buckets are shared by all the futex keys that hash to the same location.
 * Each key may have multiple vcxl_futex_q structures, one for each task waiting on a
 * futex.
 */
struct vcxl_futex_hash_bucket {
	atomic_t waiters;
	spinlock_t lock;
	unsigned long irqsave_flags;
	struct plist_head chain;
} ____cacheline_aligned_in_smp;

/**
 * struct vcxl_futex_q - The hashed futex queue entry, one per waiting task
 * @list: priority sorted list of tasks waiting on this futex
 * @task: the task waiting on the futex
 * @lock_ptr: the hash bucket lock
 * @key: the key the futex is hashed on; for shared cxl memory, the key is the offset in the shared memory.
 */
struct vcxl_futex_q {
	struct plist_node list;
	struct task_struct *task;
	spinlock_t *lock_ptr;
	unsigned long *irqsave_flags;
	u64 key;
};

struct vcxl_proc_death_task {
	struct tasklet_struct tasklet;
	struct meta_dram *meta_dram;
};

struct vcxl_ivpci_device {
	/* Kernel virtual address of SHARED_MEMORY_BAR */
	// u8 *kernel_mapped_shm;
	struct meta_dram meta_dram;
	/* Kernel virtual address of REGISTER_BAR */
	u8 __iomem *regs_addr;

	struct pci_dev *dev;
	struct cdev cdev;
	int minor;

	// u32                 ivposition;

	resource_size_t bar0_addr;
	resource_size_t bar0_len;
	resource_size_t bar1_addr;
	resource_size_t bar1_len;
	// resource_size_t    bar2_addr;
	// resource_size_t    bar2_len;
	struct dev_pagemap pgmap_bar2;

	struct vcxl_futex_hash_bucket futex_hash_table[WAIT_TABLE_SIZE];
	// struct vcxl_proc_death_task task_proc_death;

	// this is the original api;
	wait_queue_head_t domain_wait_queue;
	int event_toggle;
	char (*msix_names)[256];
	struct msix_entry *msix_entries;
	int nvectors;
	struct meta_shmem *meta_shmem;
};

#endif // VCXL_DEV_H
