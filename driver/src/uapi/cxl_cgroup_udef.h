#ifndef CXL_CGROUP_UDEF_H_
#define CXL_CGROUP_UDEF_H_
#include <linux/types.h>

// layout: msg_type 63-62, os_id 61-48, proc_id 47-32, gen_id 31-0
#define MSG_TYPE_SHIFT 62
#define OS_ID_SHIFT 48
#define PROC_ID_SHIFT 32
#define MSG_TYPE_MASK (((__u64)0x3) << MSG_TYPE_SHIFT)
#define CLEAR_MSG_TYPE (~MSG_TYPE_MASK)
#define OS_ID_MASK (((__u64)0x3fff) << OS_ID_SHIFT)
#define CLEAR_OS_ID (~OS_ID_MASK)
#define PROC_ID_MASK ((__u64)0xffff << PROC_ID_SHIFT)
#define CLEAR_PROC_ID (~PROC_ID_MASK)
#define GEN_ID_MASK ((__u64)0xffffffff)
#define CLEAR_GEN_ID (~GEN_ID_MASK)

#define GET_OS_ID(x) (__u32)(((__u64)x & OS_ID_MASK) >> OS_ID_SHIFT)
#define GET_PROC_ID(x) (__u32)(((__u64)x & PROC_ID_MASK) >> PROC_ID_SHIFT)
#define GET_GEN_ID(x) (__u32)((__u64)x & GEN_ID_MASK)
#define GET_MSG_TYPE(x) (__u32)(((__u64)x & MSG_TYPE_MASK) >> MSG_TYPE_SHIFT)

#define SET_OS_ID(x, y) (x = (((__u64)x & CLEAR_OS_ID) + ((__u64)(y) << 48)))
#define SET_PROC_ID(x, y) \
	(x = (((__u64)x & CLEAR_PROC_ID) + ((__u64)(y) << 32)))
#define SET_GEN_ID(x, y) \
	(x = (((__u64)x & CLEAR_GEN_ID) + ((__u64)(y) & GEN_ID_MASK)))
#define SET_MSG_TYPE(x, y) \
	(x = (((__u64)x & CLEAR_MSG_TYPE) + ((__u64)(y) << 62)))

_Static_assert(MSG_TYPE_MASK == 0xc000000000000000UL,
	       "MSG_TYPE_MASK is not set correctly");
_Static_assert(OS_ID_MASK == 0x3fff000000000000UL,
	       "OS_ID_MASK is not set correctly");
_Static_assert(PROC_ID_MASK == 0xffff00000000UL,
	       "PROC_ID_MASK is not set correctly");
_Static_assert(GEN_ID_MASK == 0xffffffff, "GEN_ID_MASK is not set correctly");

#endif // CXL_CGROUP_UDEF_H_
