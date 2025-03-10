#ifndef _VCXL_ALLOC_H_
#define _VCXL_ALLOC_H_ 1

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/fs.h>
#include <linux/compiler_types.h>
#endif
#include "uapi/vcxl_shm.h"
#include "log.h"
#include "shmem_obj.h"

/* The first page is allocated as a circular queue that contains address. */


int do_vcxl_init_meta(struct file *filp, struct meta_info __user *arg, __u8 tot_machines, bool test);
int do_vcxl_recover_meta(struct file *filp, struct meta_info __user *arg, __u8 tot_machines, bool test);
int do_vcxl_alloc(struct file *filp, struct region_desc __user *arg);
int do_vcxl_free(struct file *filp, struct region_desc __user *uarg);
int do_vcxl_find_alloc(struct file *filp, struct vcxl_find_alloc __user *uarg);
int do_vcxl_check_alloc(struct file *filp, struct region_desc __user *uarg);

#endif // _VCXL_ALLOC_H_