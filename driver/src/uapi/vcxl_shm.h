#ifndef _UAPI_LINUX_VCXL_SHM_H
#define _UAPI_LINUX_VCXL_SHM_H

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include <linux/futex.h>

#include "vcxl_udef.h"

#ifdef __cplusplus
extern "C" {
#else
#ifndef static_assert
#define static_assert _Static_assert
#endif
#endif

enum interrupt_vector {
	CXL_IVPCI_VECTOR_HOST_WAKE_ALL = 0,
	CXL_IVPCI_VECTOR_PROCESS_WAKE_TABLE_NEXT = 1,
	CXL_IVPCI_VECTOR_PROCESS_WAKE_TABLE_ALL = 2,
	CXL_IVPCI_VECTOR_PROC_DEATH_NOTIFY = 3,
};
#define CXL_IVPCI_VECTOR_SIZE 4
#define CXL_IVPCI_VECTOR_OTHER (CXL_IVPCI_VECTOR_SIZE + 1)

enum wait_types {
	VCXL_WAIT_UNDEFINED = 0,
	VCXL_WAIT_IF_EQUAL = 1,
	VCXL_WAIT_IF_EQUAL_TIMEOUT = 2
} __attribute__((packed));

enum wake_types {
	WAKE_NEXT = 0,
	WAKE_ALL = 1,
};

struct vcxl_cond_wait {
	/* Input: Offset of the 64bit word to check */
	__u64 offset;
	/* Input: Offset of the waiter array */
	__u64 waiter_arr_offset;
	/* Input: Monotonic time to wake at in seconds */
	__u64 wake_time_sec;
	/* Input: Value that will be compared with the futex word in the offset */
	__u64 value;
	/* Input: Monotonic time to wake at in nanoseconds */
	__u32 wake_time_nsec;
	/* Input: Type of wait; */
	__u8 wait_type;
	/* Output: Number of times the wait was woken up */
	__u32 wakes;
};
static_assert(sizeof(struct vcxl_cond_wait) == 48,
	      "vcxl_cond_wait size is not 40");

struct vcxl_cond_wake {
	/* Input: Offset of the 64bit word to check */
	__u64 offset;
	/* Input: lower 16 bit is the vector; higher 16 bit is the peer */
	__u32 arg;
};
static_assert(sizeof(struct vcxl_cond_wake) == 16,
	      "vcxl_cond_wake size is not 16");

struct region_desc {
	/* Input for free/check, output for alloc, find: offset of the memory region
   */
	__u64 offset;
	/* Input for free/check/alloc, output for find: length of the memory region */
	__u64 length;
	/* Input for find/free/check/alloc: a \0 terminated string identifier for this
   * memory region */
	char prog_id[PROG_ID_SIZE];
};
static_assert(sizeof(struct region_desc) == 32, "region_desc size is not 32");

struct vcxl_find_alloc {
	struct region_desc desc;
	/* Output: 1 if this allocation already existed, or 0 if it was newly
   * allocated */
	int existing;
};

// meta info for test
struct meta_info {
	__u64 alignment;
	__u64 mem_size;
	__u64 buddy_struct_offset;
	__u64 buddy_flags;
	int ret;
	/* Input: total number of machines */
	__u8 total_machines;
};
static_assert(sizeof(struct meta_info) == 40, "meta_info size is not 40");

enum MEMCPY_TYPE {
	DRAM_TO_CXL = 0,
	CXL_TO_DRAM = 1,
};

struct memcpy_desc {
	/**
   * if memcpy_type == CXL_TO_DRAM,
   * src is offset and dest is dram user virtual addr
   * if memcpy_type == DRAM_TO_CXL,
   * src is dram user virtual addr and dest is offset
   */
	__u64 src_ptr;
	__u64 dest_ptr;
	__u64 size;
	enum MEMCPY_TYPE memcpy_type;
};
static_assert(sizeof(struct memcpy_desc) == 32, "memcpy_desc is not 32");

struct robust_mutex_base {
	struct robust_mutex_base *next;
	struct robust_mutex_base *prev;
	/* absolute offset of the futex in CXL memory */
	__u64 futex_offset;
};

struct robust_mutex_list {
	/*
   * The head of the list. Points back to itself if empty:
   */
	struct robust_mutex_base list;

	/*
   * The death of the thread may race with userspace setting
   * up a lock's links. So to handle this race, userspace first
   * sets this field to the address of the to-be-taken lock,
   * then does the lock acquire, and then adds itself to the
   * list, and then clears this field. Hence the kernel will
   * always have full knowledge of all locks that the thread
   * _might_ have taken.
   */
	struct robust_mutex_base *list_op_pending;
};

#define IOCTL_MAGIC ('f')
#define IOCTL_WAKE \
	_IOW(IOCTL_MAGIC, 1, struct vcxl_cond_wake) /* user to kernel */
#define IOCTL_WAIT \
	_IOWR(IOCTL_MAGIC, 2, struct vcxl_cond_wait) /* both direction */
#define IOCTL_IVPOSITION _IOR(IOCTL_MAGIC, 3, __u32) /* kernel to user */
#define IOCTL_WAKE_DOMAIN _IOW(IOCTL_MAGIC, 4, __u32) /* user to kernel */
#define IOCTL_WAIT_DOMAIN _IO(IOCTL_MAGIC, 5)
#define IOCTL_ALLOC _IOWR(IOCTL_MAGIC, 6, struct region_desc)
#define IOCTL_FREE _IOW(IOCTL_MAGIC, 7, struct region_desc)
#define IOCTL_FIND_ALLOC _IOWR(IOCTL_MAGIC, 8, struct vcxl_find_alloc)
/* Initialize the buddy allocator and other metadata */
#define IOCTL_INIT_META _IOW(IOCTL_MAGIC, 9, __u8)
/* Recover the metadata */
#define IOCTL_RECOVER_META _IOW(IOCTL_MAGIC, 10, __u8)
#define IOCTL_INIT_META_TEST _IOWR(IOCTL_MAGIC, 11, struct meta_info)
#define IOCTL_RECOVER_META_TEST _IOWR(IOCTL_MAGIC, 12, struct meta_info)
#define IOCTL_CHECK_ALLOC _IOW(IOCTL_MAGIC, 13, struct region_desc)
// #define IOCTL_MEMCPY_DMA _IOW(IOCTL_MAGIC, 14, struct memcpy_desc)
#define IOCTL_SET_ROBUST_LIST _IOW(IOCTL_MAGIC, 15, __u64)
#define IOCTL_REGISTER_PROCESS _IOW(IOCTL_MAGIC, 16, __u32)
#define IOCTL_INTR_HOST _IOW(IOCTL_MAGIC, 17, __u32)

// #define IOCTL_URING_CMD_MEMCPY_DMA _IOW(IOCTL_MAGIC, 20, struct memcpy_desc)

#define IOCTL_CREATE_CXLCG _IOW(IOCTL_MAGIC, 30, __u64)
#define IOCTL_DELETE_CXLCG _IOW(IOCTL_MAGIC, 31, __u64)
#define IOCTL_JOIN_CXLCG _IOW(IOCTL_MAGIC, 32, __u64)
#define IOCTL_LEAVE_CXLCG _IOW(IOCTL_MAGIC, 33, __u64)
// #define IOCTL_FAILURE_NOTIFY        _IOW(IOCTL_MAGIC, 34, __u64)
// #define IOCTL_LEADER_CHANGE_NOTIFY  _IOW(IOCTL_MAGIC, 35, __u64)
#define IOCTL_CHECK_OS_FAILURE _IO(IOCTL_MAGIC, 36)
#define IOCTL_LOCK_TRANSFER_TEST _IOW(IOCTL_MAGIC, 37, __u16)

#define IVSHMEM_IVPOS_SHIFT 16
#define IVSHMEM_VEC_SHIFT 0
#define IVSHMEM_IVPOS_MASK ((uint32_t)0xffff0000)
#define IVSHMEM_VEC_MASK ((uint32_t)0xffff)

struct ivpos_reg {
	enum interrupt_vector vec;
	uint16_t ivpos;
};

static inline uint32_t compose_ivpos_msix_reg(enum interrupt_vector vector,
					      uint16_t machine_id)
{
	return ((uint32_t)vector << IVSHMEM_VEC_SHIFT) |
	       ((uint32_t)machine_id << IVSHMEM_IVPOS_SHIFT);
}

static inline struct ivpos_reg decompose_ivpos_msix_reg(uint32_t reg)
{
	return (struct ivpos_reg){
		.vec = (enum interrupt_vector)((reg & IVSHMEM_VEC_MASK) >>
					       IVSHMEM_VEC_SHIFT),
		.ivpos = (uint16_t)((reg & IVSHMEM_IVPOS_MASK) >>
				    IVSHMEM_IVPOS_SHIFT),
	};
}

#ifdef __cplusplus
}
#endif

#endif // _UAPI_LINUX_VCXL_SHM_H
