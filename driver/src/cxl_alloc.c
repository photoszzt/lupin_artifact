#include <linux/types.h>
#include <linux/dev_printk.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#include "uapi/vcxl_shm.h"
#include "cxl_dev.h"
#include "cxl_alloc.h"
#include "circular_queue.h"
#include "log.h"
#include "alloc_impl.h"
#include "vcxl_def.h"
#include "proc_death.h"

int do_vcxl_init_meta(struct file *filp, struct meta_info __user *uarg,
		      __u8 utot_machine, bool test)
{
	int rval = 0;
	struct vcxl_ivpci_device *ivpci_dev;
	struct meta_info info;
	__u8 tot_machine;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	if (test) {
		if (copy_from_user(&info, uarg, sizeof(info))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy_from_user failed\n");
			return -1;
		}
		tot_machine = info.total_machines;
	} else {
		tot_machine = utot_machine;
	}

	PRINT_INFO("init_meta: machine_id = %u, total_machines = %u\n",
		   ivpci_dev->meta_dram.machine_id, tot_machine);
	if (ivpci_dev->meta_dram.machine_id > VCXL_MAX_SUPPORTED_MACHINES) {
		dev_err(&ivpci_dev->dev->dev,
			PFX
			"The maximum supported number of machines is %d, the current machine is # %d\n",
			VCXL_MAX_SUPPORTED_MACHINES,
			ivpci_dev->meta_dram.machine_id);
		return -EINVAL;
	}
	if (tot_machine > VCXL_MAX_SUPPORTED_MACHINES) {
		dev_err(&ivpci_dev->dev->dev,
			PFX
			"The maximum supported number of machines is %d, the current total machine is %d\n",
			VCXL_MAX_SUPPORTED_MACHINES,
			ivpci_dev->meta_dram.total_machines);
		return -EINVAL;
	}
	vcxl_init_meta_impl(ivpci_dev, &ivpci_dev->meta_dram, ivpci_dev->dev,
			    &ivpci_dev->meta_shmem);
	assign_meta_info(&info, ivpci_dev->meta_shmem, &ivpci_dev->meta_dram);
	ivpci_dev->meta_dram.total_machines = tot_machine;
	// ivpci_dev->task_proc_death.meta_dram = &ivpci_dev->meta_dram;
	// tasklet_setup(&ivpci_dev->task_proc_death.tasklet, poll_proc_death_task);
	// tasklet_schedule(&ivpci_dev->task_proc_death.tasklet);
	if (test) {
		if (copy_to_user(uarg, &info, sizeof(info))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy_to_user failed\n");
			return -1;
		}
	}
	return rval;
}

int do_vcxl_recover_meta(struct file *filp, struct meta_info __user *uarg,
			 __u8 utot_machine, bool test)
{
	int rval = 0;
	struct vcxl_ivpci_device *ivpci_dev;
	struct meta_info info;
	__u8 tot_machine;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	if (test) {
		if (copy_from_user(&info, uarg, sizeof(info))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy_from_user failed\n");
			return -1;
		}
		tot_machine = info.total_machines;
	} else {
		tot_machine = utot_machine;
	}
	PRINT_INFO("recover_meta: machine_id = %u, total_machines = %u\n",
		   ivpci_dev->meta_dram.machine_id, tot_machine);
	if (tot_machine > VCXL_MAX_SUPPORTED_MACHINES) {
		dev_err(&ivpci_dev->dev->dev,
			PFX
			"The maximum supported number of machines is %d, the current total machine is %d\n",
			VCXL_MAX_SUPPORTED_MACHINES,
			ivpci_dev->meta_dram.total_machines);
		return -EINVAL;
	}
	rval = recover_meta(ivpci_dev, &ivpci_dev->meta_dram, ivpci_dev->dev,
			    &ivpci_dev->meta_shmem);
	if (rval) {
		return rval;
	}
	assign_meta_info(&info, ivpci_dev->meta_shmem, &ivpci_dev->meta_dram);
	ivpci_dev->meta_dram.total_machines = tot_machine;
	// ivpci_dev->task_proc_death.meta_dram = &ivpci_dev->meta_dram;
	// tasklet_setup(&ivpci_dev->task_proc_death.tasklet, poll_proc_death_task);
	// tasklet_schedule(&ivpci_dev->task_proc_death.tasklet);
	if (test) {
		if (copy_to_user(uarg, &info, sizeof(info))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy_to_user failed\n");
			return -1;
		}
	}
	return rval;
}

int do_vcxl_find_alloc(struct file *filp, struct vcxl_find_alloc __user *uarg)
{
	struct vcxl_ivpci_device *ivpci_dev;
	struct vcxl_find_alloc arg;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	if (copy_from_user(&arg, uarg, sizeof(arg))) {
		dev_err(&ivpci_dev->dev->dev, PFX "fail to get argument\n");
		return -EFAULT;
	}
	find_allocation(&arg, ivpci_dev->meta_shmem, &ivpci_dev->meta_dram);
	if (copy_to_user(uarg, &arg, sizeof(arg))) {
		dev_err(&ivpci_dev->dev->dev, PFX "copy_to_user failed\n");
		return -1;
	}
	return 0;
}

int do_vcxl_check_alloc(struct file *filp, struct region_desc __user *uarg)
{
	struct region_desc arg;
	struct vcxl_ivpci_device *ivpci_dev;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	if (copy_from_user(&arg, uarg, sizeof(arg))) {
		dev_err(&ivpci_dev->dev->dev, PFX "fail to get argument\n");
		return -EFAULT;
	}
	return check_allocation(&arg, &ivpci_dev->meta_dram,
				ivpci_dev->meta_shmem);
}

int do_vcxl_alloc(struct file *filp, struct region_desc __user *uarg)
{
	int rval = 0;
	struct vcxl_ivpci_device *ivpci_dev;
	struct region_desc arg = { .length = 0, .offset = 0, .prog_id = { 0 } };
	__u8 *ret;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	if (copy_from_user(&arg, uarg, sizeof(struct region_desc))) {
		dev_err(&ivpci_dev->dev->dev, PFX "fail to get argument\n");
		return -EFAULT;
	}
	BUG_ON_ASSERT(arg.length == 0);
	// PRINT_INFO("do_vcxl_alloc: copy from user prog_id: %s\n", arg.prog_id);
	rval = atomic_allocate_memory(ivpci_dev->meta_shmem,
				      &ivpci_dev->meta_dram, &arg, &ret);
	if (rval) {
		return rval;
	}
	arg.offset = (__u8 *)ret - ivpci_dev->meta_dram.mem_start;
	if (copy_to_user(uarg, &arg, sizeof(arg))) {
		dev_err(&ivpci_dev->dev->dev, PFX "copy_to_user failed\n");
		return -1;
	}

	return rval;
}

int do_vcxl_free(struct file *filp, struct region_desc __user *uarg)
{
	int rval = 0;
	struct vcxl_ivpci_device *ivpci_dev;
	struct region_desc arg;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	if (copy_from_user(&arg, uarg, sizeof(arg))) {
		return -EFAULT;
	}
	atomic_free_memory(ivpci_dev->meta_shmem, &ivpci_dev->meta_dram, &arg,
			   ivpci_dev->dev);
	return rval;
}
