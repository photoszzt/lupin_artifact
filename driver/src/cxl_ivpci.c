/* http://just4coding.com/2021/09/25/ivshmem-pci/ */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io_uring.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/version.h>
#include <linux/ktime.h>

#include "alloc_impl.h"
#include "cxl_alloc.h"
#include "cxl_dev.h"
#include "cxl_futex.h"
#include "fault_inject.h"
#include "uapi/vcxl_shm.h"
#include "vcxl_def.h"
// #include "memcpy_dma.h"
#include "ftrace_helper.h"
#include "proc_death.h"
#include "task_robust_def.h"
#include "cxl_cgroup.h"

#define DRV_NAME "cxl_ivpci"
#define DRV_VERSION "0.1"
#define DRV_FILE_FMT DRV_NAME "%d"

static int g_max_devices = 2;
MODULE_PARM_DESC(g_max_devices, "number of devices can be supported");
module_param(g_max_devices, int, 0400);

#ifdef INJECT_CRASH
int crash_fault = 0;
module_param(crash_fault, int, 0400);
MODULE_PARM_DESC(crash_fault, "crash fault to inject. 0 no crash fault");
#endif

/* store major device number shared by all ivshmem devices */
static dev_t g_ivpci_devno;
static int g_ivpci_major;

static struct class *g_ivpci_class;

/* number of devices owned by this driver */
static int g_ivpci_count;

static struct vcxl_ivpci_device *g_ivpci_devs;

static struct pci_device_id ivpci_id_table[] = {
	{ 0x1af4, 0x1110, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, ivpci_id_table);

static struct vcxl_ivpci_device *vcxl_ivpci_get_device(void)
{
	int i;

	for (i = 0; i < g_max_devices; i++) {
		if (g_ivpci_devs[i].dev == NULL) {
			return &g_ivpci_devs[i];
		}
	}

	return NULL;
}

static struct vcxl_ivpci_device *vcxl_ivpci_find_device(int minor)
{
	int i;
	for (i = 0; i < g_max_devices; i++) {
		if (g_ivpci_devs[i].dev != NULL &&
		    g_ivpci_devs[i].minor == minor) {
			return &g_ivpci_devs[i];
		}
	}

	return NULL;
}

static irqreturn_t vcxl_ivpci_interrupt_host_wake_all(int irq, void *dev_id)
{
	struct vcxl_ivpci_device *ivpci_dev = dev_id;

	if (unlikely(ivpci_dev == NULL)) {
		return IRQ_NONE;
	}

	dev_info(&ivpci_dev->dev->dev, PFX "interrupt: %d\n", irq);

	ivpci_dev->event_toggle = 1;
	wake_up_interruptible_all(&ivpci_dev->domain_wait_queue);

	return IRQ_HANDLED;
}

static irqreturn_t vcxl_ivpci_interrupt(int irq, void *dev_id)
{
	struct vcxl_ivpci_device *ivpci_dev = dev_id;

	if (unlikely(ivpci_dev == NULL)) {
		return IRQ_NONE;
	}

	dev_info(&ivpci_dev->dev->dev, PFX "interrupt: %d\n", irq);

	return IRQ_HANDLED;
}

static int
vcxl_ivpci_request_msix_vectors(struct vcxl_ivpci_device *vcxl_ivpci_dev, int n)
{
	int ret, i;
	// irq_handler_t handlers[CXL_IVPCI_VECTOR_SIZE] = {
	//     vcxl_ivpci_interrupt_host_wake_all,
	//     vcxl_ivpci_interrupt_process_wake_table_next,
	//     vcxl_ivpci_interrupt_process_wake_table_all,
	//     vcxl_ivpci_interrupt_proc_death_task_notify,
	// };

	ret = -EINVAL;

	dev_info(&vcxl_ivpci_dev->dev->dev, PFX "request msi-x vectors: %d\n",
		 n);

	vcxl_ivpci_dev->nvectors = n;

	vcxl_ivpci_dev->msix_entries =
		kmalloc(n * sizeof(struct msix_entry), GFP_KERNEL);
	if (vcxl_ivpci_dev->msix_entries == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	vcxl_ivpci_dev->msix_names =
		kmalloc(n * sizeof(*vcxl_ivpci_dev->msix_names), GFP_KERNEL);
	if (vcxl_ivpci_dev->msix_names == NULL) {
		ret = -ENOMEM;
		goto free_entries;
	}

	for (i = 0; i < n; i++) {
		vcxl_ivpci_dev->msix_entries[i].entry = i;
	}

	ret = pci_enable_msix_exact(vcxl_ivpci_dev->dev,
				    vcxl_ivpci_dev->msix_entries, n);
	if (ret) {
		dev_err(&vcxl_ivpci_dev->dev->dev,
			PFX "unable to enable msix: %d\n", ret);
		goto free_names;
	}
	for (i = 0; i < vcxl_ivpci_dev->nvectors; i++) {
		snprintf(vcxl_ivpci_dev->msix_names[i],
			 sizeof(*vcxl_ivpci_dev->msix_names), "%s%d-%d",
			 DRV_NAME, vcxl_ivpci_dev->minor, i);

		if (i == CXL_IVPCI_VECTOR_HOST_WAKE_ALL) {
			ret = request_irq(
				vcxl_ivpci_dev->msix_entries[i].vector,
				vcxl_ivpci_interrupt_host_wake_all, 0,
				vcxl_ivpci_dev->msix_names[i], vcxl_ivpci_dev);
		} else if (i == CXL_IVPCI_VECTOR_PROCESS_WAKE_TABLE_NEXT) {
			ret = request_irq(
				vcxl_ivpci_dev->msix_entries[i].vector,
				vcxl_ivpci_interrupt_process_wake_table_next, 0,
				vcxl_ivpci_dev->msix_names[i], vcxl_ivpci_dev);
		} else if (i == CXL_IVPCI_VECTOR_PROCESS_WAKE_TABLE_ALL) {
			ret = request_irq(
				vcxl_ivpci_dev->msix_entries[i].vector,
				vcxl_ivpci_interrupt_process_wake_table_all, 0,
				vcxl_ivpci_dev->msix_names[i], vcxl_ivpci_dev);
		} else if (i == CXL_IVPCI_VECTOR_PROC_DEATH_NOTIFY) {
			ret = request_irq(
				vcxl_ivpci_dev->msix_entries[i].vector,
				vcxl_ivpci_interrupt_proc_death_task_notify, 0,
				vcxl_ivpci_dev->msix_names[i], vcxl_ivpci_dev);
		} else {
			ret = request_irq(
				vcxl_ivpci_dev->msix_entries[i].vector,
				vcxl_ivpci_interrupt, 0,
				vcxl_ivpci_dev->msix_names[i], vcxl_ivpci_dev);
		}
		if (ret) {
			dev_err(&vcxl_ivpci_dev->dev->dev,
				PFX "unable to allocate irq for "
				    "msix entry %d with vector %d\n",
				i, vcxl_ivpci_dev->msix_entries[i].vector);
			goto release_irqs;
		}

		dev_info(&vcxl_ivpci_dev->dev->dev,
			 PFX "irq for msix entry: %d, vector: %d\n", i,
			 vcxl_ivpci_dev->msix_entries[i].vector);
	}

	return 0;

release_irqs:
	for (; i > 0; i--) {
		free_irq(vcxl_ivpci_dev->msix_entries[i - 1].vector,
			 vcxl_ivpci_dev);
	}
	pci_disable_msix(vcxl_ivpci_dev->dev);

free_names:
	kfree(vcxl_ivpci_dev->msix_names);

free_entries:
	kfree(vcxl_ivpci_dev->msix_entries);

error:
	return ret;
}

static void vcxl_ivpci_free_msix_vectors(struct vcxl_ivpci_device *ivpci_dev)
{
	int i;

	for (i = ivpci_dev->nvectors; i > 0; i--) {
		free_irq(ivpci_dev->msix_entries[i - 1].vector, ivpci_dev);
	}
	pci_disable_msix(ivpci_dev->dev);

	kfree(ivpci_dev->msix_names);
	kfree(ivpci_dev->msix_entries);
}

static int vcxl_ivpci_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(inode);
	struct vcxl_ivpci_device *ivpci_dev;

	ivpci_dev = vcxl_ivpci_find_device(minor);
	filp->private_data = (void *)ivpci_dev;
	BUG_ON(filp->private_data == NULL);

	dev_info(&ivpci_dev->dev->dev, PFX "open vcxl ivpci\n");

	return 0;
}

static vm_fault_t vcxl_ivpci_mmap_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct vcxl_ivpci_device *ivpci_dev =
		(struct vcxl_ivpci_device *)vma->vm_private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);
	if ((vmf->pgoff << PAGE_SHIFT) >= ivpci_dev->meta_dram.mem_length) {
		return VM_FAULT_SIGBUS;
	}

	vmf->page = virt_to_page(ivpci_dev->meta_dram.mem_start +
				 (vmf->pgoff << PAGE_SHIFT));
	get_page(vmf->page);
	return 0;
}

static const struct vm_operations_struct vcxl_ivpci_mmap_ops = {
	.fault = vcxl_ivpci_mmap_fault
};

static int vcxl_ivpci_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long len = 0, off = 0, start = 0, vsize = 0;
	// unsigned long physical = 0;
	struct vcxl_ivpci_device *ivpci_dev = NULL;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	// dev_info(&ivpci_dev->dev->dev, PFX "mmap vcxl ivpci bar2\n");

	/* `vma->vm_start` and `vma->vm_end` had aligned to page size */
	WARN_ON(offset_in_page(vma->vm_start));
	WARN_ON(offset_in_page(vma->vm_end));
	vsize = vma->vm_end - vma->vm_start;

	off = vma->vm_pgoff << PAGE_SHIFT;
	start = ivpci_dev->pgmap_bar2.range.start;

	/* Align up to page size. */
	len = PAGE_ALIGN((start & ~PAGE_MASK) +
			 ivpci_dev->meta_dram.mem_length);
	start &= PAGE_MASK;

	// dev_info(&ivpci_dev->dev->dev,
	// 	 PFX "mmap vma pgoff: %lu, 0x%0lx - 0x%0lx,"
	// 	     " aligned length: %lu\n",
	// 	 vma->vm_pgoff, vma->vm_start, vma->vm_end, len);

	if (vsize + off > len) {
		dev_err(&ivpci_dev->dev->dev,
			PFX "mmap overflow the end, %lx - %lx + %lx > %lx",
			vma->vm_end, vma->vm_start, off, len);
		ret = -EINVAL;
		goto error;
	}

	vma->vm_ops = &vcxl_ivpci_mmap_ops;
	vma->vm_private_data = ivpci_dev;

	// physical = off + start;
	// vma->vm_pgoff = off >> PAGE_SHIFT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
	vm_flags_set(vma, VM_SHARED | VM_DONTEXPAND | VM_DONTDUMP);
#else
	vma->vm_flags |= VM_SHARED | VM_DONTEXPAND | VM_DONTDUMP;
#endif

	// if (io_remap_pfn_range(vma, vma->vm_start, physical >> PAGE_SHIFT,
	// 		       vsize, vma->vm_page_prot)) {
	// 	dev_err(&ivpci_dev->dev->dev, PFX "mmap bar2 failed\n");
	// 	ret = -ENXIO;
	// 	goto error;
	// }

	ret = 0;

error:
	return ret;
}

// static ssize_t vcxl_ivpci_read(struct file *filp, char __user *buffer,
// 			       size_t len, loff_t *poffset)
// {
// 	int ret = 0;
// 	unsigned long offset;
// 	struct vcxl_ivpci_device *ivpci_dev;
//
// 	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
//
// 	BUG_ON(ivpci_dev == NULL);
// 	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);
//
// 	offset = *poffset;
// 	dev_info(&ivpci_dev->dev->dev, PFX "read ivpci bar2, offset: %lu\n",
// 		 offset);
//
// 	/* beyoud the end */
// 	if (len > ivpci_dev->meta_dram.mem_length - offset) {
// 		len = ivpci_dev->meta_dram.mem_length - offset;
// 	}
//
// 	if (offset < FUTEX_ADDR_QUEUE_SIZE) {
// 		dev_err(&ivpci_dev->dev->dev,
// 			PFX "read ivpci bar2, offset: %lu, invalid offset\n",
// 			offset);
// 		return -EINVAL;
// 	}
//
// 	if (len == 0) {
// 		return 0;
// 	}
//
// 	ret = copy_to_user(
// 		buffer, shm_off_to_virtual_addr(&ivpci_dev->meta_dram, offset),
// 		len);
// 	if (ret != 0) {
// 		dev_err(&ivpci_dev->dev->dev,
// 			PFX "read ivpci bar2, copy_to_user() failed: %d\n",
// 			ret);
// 		return ret;
// 	}
//
// 	*poffset += len;
// 	return len;
// }

static long vcxl_ivpci_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	int ret;
	struct vcxl_ivpci_device *ivpci_dev;
	struct cxlcg_args cxlcg_args;
	u16 ivposition;
	u16 vector;
	u32 value;

	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;

	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	switch (cmd) {
	case IOCTL_WAKE_DOMAIN:
		if (copy_from_user(&value, (u32 __user *)arg, sizeof(value))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy from user failed");
			return -1;
		}
		vector = value & 0xffff;
		ivposition = value & 0xffff0000 >> 16;
		dev_info(
			&ivpci_dev->dev->dev,
			PFX
			"ring doorbell: value: %u(0x%x), vector: %u, peer id: %u\n",
			value, value, vector, ivposition);
		if (vector == CXL_IVPCI_VECTOR_HOST_WAKE_ALL) {
			iowrite32(value & 0xffffffff,
				  ivpci_dev->regs_addr + DOORBELL_OFF);
		} else {
			dev_err(&ivpci_dev->dev->dev,
				PFX "invalid vector: %u\n", vector);
			return -EINVAL;
		}
		break;
	case IOCTL_WAIT_DOMAIN:
		dev_info(&ivpci_dev->dev->dev, PFX "wait for interrupt\n");
		ret = wait_event_interruptible(ivpci_dev->domain_wait_queue,
					       (ivpci_dev->event_toggle == 1));
		if (ret == 0) {
			dev_info(&ivpci_dev->dev->dev, PFX "wakeup\n");
			ivpci_dev->event_toggle = 0;
		} else if (ret == -ERESTARTSYS) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "interrupted by signal\n");
			return ret;
		} else {
			dev_err(&ivpci_dev->dev->dev,
				PFX "unknown failed: %d\n", ret);
			return ret;
		}
		break;
	case IOCTL_WAIT:
		dev_info(&ivpci_dev->dev->dev, PFX "futex wait\n");
		return do_vcxl_cond_wait(ivpci_dev,
					 (struct vcxl_cond_wait __user *)arg);

	case IOCTL_WAKE:
		dev_info(&ivpci_dev->dev->dev, PFX "futex wake\n");
		return do_vcxl_wake(filp, (struct vcxl_cond_wake __user *)arg);

	case IOCTL_IVPOSITION:
		// dev_info(&ivpci_dev->dev->dev, PFX "get ivposition: %u\n",
		// 	 ivpci_dev->meta_dram.machine_id);
		if (copy_to_user((u32 __user *)arg,
				 &ivpci_dev->meta_dram.machine_id,
				 sizeof(u32))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy to user failed");
			return -1;
		}
		break;

	case IOCTL_INIT_META:
		// dev_info(&ivpci_dev->dev->dev, PFX "initilize metadata\n");
		return do_vcxl_init_meta(filp, NULL, (__u8)arg, false);

	case IOCTL_INIT_META_TEST:
		// dev_info(&ivpci_dev->dev->dev, PFX "initilize metadata test\n");
		return do_vcxl_init_meta(filp, (struct meta_info __user *)arg,
					 0, true);

	case IOCTL_RECOVER_META:
		// dev_info(&ivpci_dev->dev->dev, PFX "recover metadata\n");
		return do_vcxl_recover_meta(filp, NULL, (__u8)arg, false);

	case IOCTL_RECOVER_META_TEST:
		// dev_info(&ivpci_dev->dev->dev, PFX "recover metadata test\n");
		return do_vcxl_recover_meta(
			filp, (struct meta_info __user *)arg, 0, true);

	case IOCTL_ALLOC:
		// dev_info(&ivpci_dev->dev->dev, PFX "allocate memory\n");
		return do_vcxl_alloc(filp, (struct region_desc __user *)arg);

	case IOCTL_FREE:
		// dev_info(&ivpci_dev->dev->dev, PFX "free memory\n");
		return do_vcxl_free(filp, (struct region_desc __user *)arg);

	case IOCTL_FIND_ALLOC:
		// dev_info(&ivpci_dev->dev->dev, PFX "find allocation\n");
		return do_vcxl_find_alloc(filp,
					  (struct vcxl_find_alloc __user *)arg);

	case IOCTL_CHECK_ALLOC:
		// dev_info(&ivpci_dev->dev->dev, PFX "check allocation\n");
		return do_vcxl_check_alloc(filp,
					   (struct region_desc __user *)arg);

		// case IOCTL_MEMCPY_DMA:
		//   dev_info(&ivpci_dev->dev->dev, PFX "memcpy with dma\n");
		//   return do_vcxl_memcpy_dma(filp, (struct memcpy_desc __user *)arg);

	case IOCTL_SET_ROBUST_LIST:
		dev_info(&ivpci_dev->dev->dev, PFX "set robust list\n");
		return do_set_robust_list(
			filp, (struct robust_mutex_list __user *)arg);

	case IOCTL_REGISTER_PROCESS:
		dev_info(&ivpci_dev->dev->dev, PFX "register process\n");
		return do_register_process(filp, (__u16)arg);

	case IOCTL_INTR_HOST:
		dev_info(&ivpci_dev->dev->dev, PFX "intr host\n");
		iowrite32(arg, ivpci_dev->regs_addr + DOORBELL_OFF);
		break;

	case IOCTL_CREATE_CXLCG:
		if (copy_from_user(&cxlcg_args, (const void __user *)arg,
				   sizeof(struct cxlcg_args))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy from user failed");
			return -1;
		}
		do_create_cxlcg(&cxlcg_args);
		break;

	case IOCTL_DELETE_CXLCG:
		if (copy_from_user(&cxlcg_args, (const void __user *)arg,
				   sizeof(struct cxlcg_args))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy from user failed");
			return -1;
		}
		do_delete_cxlcg(&cxlcg_args);
		break;

	case IOCTL_JOIN_CXLCG:
		if (copy_from_user(&cxlcg_args, (const void __user *)arg,
				   sizeof(struct cxlcg_args))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy from user failed");
			return -1;
		}
		do_join_cxlcg(&cxlcg_args);
		break;

	case IOCTL_LEAVE_CXLCG:
		if (copy_from_user(&cxlcg_args, (const void __user *)arg,
				   sizeof(struct cxlcg_args))) {
			dev_err(&ivpci_dev->dev->dev,
				PFX "copy from user failed");
			return -1;
		}
		do_leave_cxlcg(&cxlcg_args);
		break;

	case IOCTL_CHECK_OS_FAILURE:
		do_os_failure_detection(filp);
		break;

	case IOCTL_LOCK_TRANSFER_TEST:
		dev_info(&ivpci_dev->dev->dev, PFX "lock transfer test\n");
		return lock_transfer_test(filp, (__u16)arg);

	default:
		dev_err(&ivpci_dev->dev->dev, PFX "bad ioctl command: %d\n",
			cmd);
		return -1;
	}

	return 0;
}

// static int vcxl_ivpci_uring_cmd(struct io_uring_cmd *ioucmd,
//                                 unsigned int issue_flags) {
//   struct vcxl_ivpci_device *ivpci_dev =
//       (struct vcxl_ivpci_device *)ioucmd->file->private_data;
//
//   BUILD_BUG_ON(sizeof(struct memcpy_desc) > sizeof(ioucmd->pdu));
//
//   if (ioucmd->cmd_op == IOCTL_URING_CMD_MEMCPY_DMA) {
//     return uring_cmd_memcpy_dma(ivpci_dev, ioucmd, issue_flags);
//   } else {
//     DEV_ERR(ivpci_dev->dev, "unrecognized iouring cmd: %d\n", ioucmd->cmd_op);
//     return -EINVAL;
//   }
// }

// static long vcxl_ivpci_write(struct file *filp, const char __user *buffer,
// 			     size_t len, loff_t *poffset)
// {
// 	int ret = 0;
// 	unsigned long offset;
// 	struct vcxl_ivpci_device *ivpci_dev =
// 		(struct vcxl_ivpci_device *)filp->private_data;
//
// 	BUG_ON(ivpci_dev == NULL);
// 	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);
//
// 	dev_info(&ivpci_dev->dev->dev, PFX "write ivpci bar2\n");
//
// 	offset = *poffset;
//
// 	if (len > ivpci_dev->meta_dram.mem_length - offset) {
// 		len = ivpci_dev->meta_dram.mem_length - offset;
// 	}
//
// 	if (len == 0) {
// 		return 0;
// 	}
//
// 	ret = copy_from_user(shm_off_to_virtual_addr(&ivpci_dev->meta_dram,
// 						     offset),
// 			     buffer, len);
// 	if (ret != 0) {
// 		return ret;
// 	}
//
// 	*poffset += len;
// 	return len;
// }

// static loff_t vcxl_ivpci_lseek(struct file *filp, loff_t offset, int whence)
// {
// 	loff_t newpos = -1;
// 	struct vcxl_ivpci_device *ivpci_dev;
//
// 	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
//
// 	BUG_ON(ivpci_dev == NULL);
// 	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);
//
// 	dev_info(&ivpci_dev->dev->dev,
// 		 PFX "lseek ivpci bar2, offset: %llu, whence: %d\n", offset,
// 		 whence);
//
// 	switch (whence) {
// 	case 2:
// 		// cannot seek pass device end
// 		return -EINVAL;
// 	case 1: /* SEEK_CUR */
// 		newpos = offset + filp->f_pos;
// 		break;
// 	case 0: /* SEEK_SET */
// 		newpos = offset;
// 		break;
// 	default:
// 		// unrecognized whence
// 		return -EINVAL;
// 	}
// 	if (newpos > ivpci_dev->meta_dram.mem_length) {
// 		newpos = ivpci_dev->meta_dram.mem_length;
// 	}
// 	// cannot seek backwards
// 	if (newpos < 0) {
// 		return -EINVAL;
// 	}
// 	filp->f_pos = newpos;
// 	return newpos;
// }

static int vcxl_ivpci_release(struct inode *inode, struct file *filp)
{
	struct vcxl_ivpci_device *ivpci_dev;
	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;

	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

	dev_info(&ivpci_dev->dev->dev, PFX "release ivpci\n");

	return 0;
}

static struct file_operations ivpci_ops = {
	.owner = THIS_MODULE,
	.open = vcxl_ivpci_open,
	.mmap = vcxl_ivpci_mmap,
	.unlocked_ioctl = vcxl_ivpci_ioctl,
	// .uring_cmd = vcxl_ivpci_uring_cmd,
	// .read = vcxl_ivpci_read,
	// .write = vcxl_ivpci_write,
	// .llseek = vcxl_ivpci_lseek,
	.release = vcxl_ivpci_release,
};

static int vcxl_ivpci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *id)
{
	int ret;
	struct vcxl_ivpci_device *ivpci_dev;
	dev_t devno;
	u8 revision;

	dev_info(&pdev->dev, PFX "probing for device: %s\n", pci_name(pdev));

	if (g_ivpci_count >= g_max_devices) {
		dev_err(&pdev->dev, PFX
			"reach the maxinum number of devices, "
			"please adapt the `g_max_devices` value, reload the driver\n");
		ret = -1;
		goto out;
	}

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, PFX "unable to enable device: %d\n", ret);
		goto out;
	}

	/* Reserved PCI I/O and memory resources for this device */
	ret = pci_request_regions(pdev, DRV_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, PFX "unable to reserve resources: %d\n",
			ret);
		goto disable_device;
	}

	ivpci_dev = vcxl_ivpci_get_device();
	BUG_ON(ivpci_dev == NULL);

	pci_read_config_byte(pdev, PCI_REVISION_ID, &revision);

	dev_info(&pdev->dev, PFX "device %d:%d, revision: %d\n", g_ivpci_major,
		 ivpci_dev->minor, revision);

	/* Pysical address of BAR0, BAR1, BAR2 */
	ivpci_dev->bar0_addr = pci_resource_start(pdev, 0);
	ivpci_dev->bar0_len = pci_resource_len(pdev, 0);
	ivpci_dev->bar1_addr = pci_resource_start(pdev, 1);
	ivpci_dev->bar1_len = pci_resource_len(pdev, 1);
	ivpci_dev->pgmap_bar2.range.start = pci_resource_start(pdev, 2);
	ivpci_dev->pgmap_bar2.range.end = pci_resource_end(pdev, 2);
	ivpci_dev->pgmap_bar2.nr_range = 1;
	ivpci_dev->pgmap_bar2.type = MEMORY_DEVICE_GENERIC;
	ivpci_dev->meta_dram.mem_length = pci_resource_len(pdev, 2);

	init_waitqueue_head(&ivpci_dev->domain_wait_queue);

	dev_info(&pdev->dev, PFX "BAR0: 0x%0llx, %lld\n", ivpci_dev->bar0_addr,
		 ivpci_dev->bar0_len);
	dev_info(&pdev->dev, PFX "BAR1: 0x%0llx, %lld\n", ivpci_dev->bar1_addr,
		 ivpci_dev->bar1_len);
	dev_info(&pdev->dev, PFX "BAR2: 0x%0llx, %lld\n",
		 ivpci_dev->pgmap_bar2.range.start,
		 ivpci_dev->meta_dram.mem_length);

	ivpci_dev->regs_addr =
		ioremap(ivpci_dev->bar0_addr, ivpci_dev->bar0_len);
	if (!ivpci_dev->regs_addr) {
		dev_err(&pdev->dev, PFX "unable to ioremap bar0, size: %lld\n",
			ivpci_dev->bar0_len);
		goto release_regions;
	}

	// since cxl memory behaves like a normal RAM, we can use memremap
	// ivpci_dev->meta_dram.mem_start = memremap(ivpci_dev->bar2_addr,
	// ivpci_dev->meta_dram.mem_length, MEMREMAP_WB);
	ivpci_dev->meta_dram.mem_start =
		devm_memremap_pages(&pdev->dev, &ivpci_dev->pgmap_bar2);
	if (!ivpci_dev->meta_dram.mem_start) {
		dev_err(&pdev->dev, PFX "unable to ioremap bar2, size: %lld\n",
			ivpci_dev->meta_dram.mem_length);
		goto iounmap_bar0;
	}
	dev_info(&pdev->dev, PFX "BAR2 map: %p\n",
		 ivpci_dev->meta_dram.mem_start);

	/*
   * Create character device file.
   */
	cdev_init(&ivpci_dev->cdev, &ivpci_ops);
	ivpci_dev->cdev.owner = THIS_MODULE;

	devno = MKDEV(g_ivpci_major, ivpci_dev->minor);
	ret = cdev_add(&ivpci_dev->cdev, devno, 1);
	if (ret < 0) {
		dev_err(&pdev->dev,
			PFX "unable to add chrdev %d:%d to system: %d\n",
			g_ivpci_major, ivpci_dev->minor, ret);
		goto memunmap_bar2;
	}

	if (device_create(g_ivpci_class, NULL, devno, NULL, DRV_FILE_FMT,
			  ivpci_dev->minor) == NULL) {
		dev_err(&pdev->dev, PFX "unable to create device file: %d:%d\n",
			g_ivpci_major, ivpci_dev->minor);
		goto delete_chrdev;
	}

	ivpci_dev->dev = pdev;
	pci_set_drvdata(pdev, ivpci_dev);

	if (revision == 1) {
		/* Only process the MSI-X interrupt. */
		ivpci_dev->meta_dram.machine_id =
			(u16)ioread32(ivpci_dev->regs_addr + IVPOSITION_OFF);
		if (ivpci_dev->meta_dram.machine_id >
		    VCXL_MAX_SUPPORTED_MACHINES) {
			dev_err(&pdev->dev,
				PFX
				"machine id is larger than supported number of machines %d\n",
				VCXL_MAX_SUPPORTED_MACHINES);
			goto destroy_device;
		}

		dev_info(&pdev->dev, PFX "device ivposition: %u, MSI-X: %s\n",
			 ivpci_dev->meta_dram.machine_id,
			 (ivpci_dev->meta_dram.machine_id == 0) ? "no" : "yes");

		if (ivpci_dev->meta_dram.machine_id != 0) {
			ret = vcxl_ivpci_request_msix_vectors(ivpci_dev, 8);
			if (ret != 0) {
				goto destroy_device;
			}
		}
	}

	g_ivpci_count++;
	dev_info(&pdev->dev, PFX "device probed: %s\n", pci_name(pdev));
	return 0;

destroy_device:
	devno = MKDEV(g_ivpci_major, ivpci_dev->minor);
	device_destroy(g_ivpci_class, devno);
	ivpci_dev->dev = NULL;

delete_chrdev:
	cdev_del(&ivpci_dev->cdev);

memunmap_bar2:
	devm_memunmap_pages(&pdev->dev, &ivpci_dev->pgmap_bar2);

iounmap_bar0:
	iounmap(ivpci_dev->regs_addr);

release_regions:
	pci_release_regions(pdev);

disable_device:
	pci_disable_device(pdev);

out:
	pci_set_drvdata(pdev, NULL);
	return ret;
}

static void vcxl_ivpci_remove(struct pci_dev *pdev)
{
	int devno;
	struct vcxl_ivpci_device *ivpci_dev;

	dev_info(&pdev->dev, PFX "removing ivshmem device: %s\n",
		 pci_name(pdev));

	ivpci_dev = pci_get_drvdata(pdev);
	BUG_ON(ivpci_dev == NULL);

	// tasklet_kill(&ivpci_dev->task_proc_death.tasklet);
	vcxl_ivpci_free_msix_vectors(ivpci_dev);

	ivpci_dev->dev = NULL;

	devno = MKDEV(g_ivpci_major, ivpci_dev->minor);
	device_destroy(g_ivpci_class, devno);

	cdev_del(&ivpci_dev->cdev);

	devm_memunmap_pages(&pdev->dev, &ivpci_dev->pgmap_bar2);
	iounmap(ivpci_dev->regs_addr);

	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

static struct pci_driver vcxl_ivpci_driver = {
	.name = DRV_NAME,
	.id_table = ivpci_id_table,
	.probe = vcxl_ivpci_probe,
	.remove = vcxl_ivpci_remove,
};

/*
 * Nuke all other threads in the group.
 */
// static int zap_other_threads_ivpci(struct task_struct *p)
// {
// 	struct task_struct *t;
// 	int count = 0;
//
// 	p->signal->group_stop_count = 0;
//
// 	for (t = p; (t = next_thread(t)) != p;) {
// 		task_clear_jobctl_pending(t, JOBCTL_PENDING_MASK);
// 		count++;
//
// 		/* Don't bother with already dead threads */
// 		if (t->exit_state)
// 			continue;
// 		sigaddset(&t->pending.signal, SIGKILL);
// 		signal_wake_up(t, 1);
// 	}
//
// 	return count;
// }

static void (*do_group_exit_orig)(int exit_code);
static void (*do_exit_orig)(long exit_code);

static void hook_do_exit(long exit_code)
{
	if (current == current->group_leader) {
		int exit_code_prog = exit_code >> 8;
		if (exit_code_prog != 0) {
			// pr_info("name: %s exit abnormally, exit code: %d\n",
			// 	current->comm, exit_code_prog);
			// 	wait until other threads are entering to do_exit
			while (atomic_read(&current->signal->live) > 1)
				;
			notify_proc_failure(current);
		}
	}
	do_exit_orig(exit_code);
}

// from: https://github.com/iovisor/bcc/blob/4578cd9daa88460b96e0af1295f17dfa26a3d011/tools/exitsnoop_example.txt#L130C1-L138C43
// Linux keeps process termination information in 'exit_code', an int
// within struct 'task_struct' defined in <linux/sched.c>
//     - if the process terminated normally:
//         - the exit value is in bits 15:8
//         - the least significant 8 bits of exit_code are zero (bits 7:0)
//     - if the process terminates abnormally:
//         - the signal number (>= 1) is in bits 6:0
//         - bit 7 indicates a 'core dump' action, whether a core dump was
//           actually done depends on ulimit.
static void hook_do_group_exit(int exit_code)
{
	// int exit_code_prog = exit_code >> 8;
	// u64 end_notify, end_mm_release, elapsed_notify, elapsed_release;
	// u64 start_notify = ktime_get_ns();
	//
	// either exit normally but a non zero exit code or terminates abnormally
	// send failure notification

	// if (exit_code_prog != 0) {
	// 	// pr_info("name: %s exit abnormally, exit code: %d\n",
	// 	// 	current->comm, exit_code_prog);
	// 	notify_proc_failure(current);
	// }

	// hook_do_exit(exit_code);
	// end_notify = ktime_get_ns();
	do_group_exit_orig(exit_code);
	// end_mm_release = ktime_get_ns();
	// elapsed_notify = end_notify - start_notify;
	// elapsed_release = end_mm_release - end_notify;
	// pr_info("name: %s, notify elapsed: %llu ns, mm release elapsed: %llu ns\n",
	// 	tsk->comm, elapsed_notify, elapsed_release);
}

static void (*exit_panic_orig)(const char *fmt, ...);
static void hook_panic(const char *fmt, ...)
{
	struct timespec64 ts;
	increment_gen_id();
	ktime_get_real_ts64(&ts);
	pr_info(PFX "increment gen_id before panic!!!, panic at %lld.%09ld\n",
		ts.tv_sec, ts.tv_nsec);
	// pr_info(PFX "panic!!!\n");
	while (true)
		;
}

/* Declare the struct that ftrace needs to hook the syscall */
static struct ftrace_hook hooks[] = {
	// HOOK("do_group_exit", hook_do_group_exit, &do_group_exit_orig),
	HOOK("panic", hook_panic, &exit_panic_orig),
	HOOK("do_exit", hook_do_exit, &do_exit_orig),
};

static int __init vcxl_ivpci_init(void)
{
	int ret, i, minor;

	pr_info(PFX
		"*********************************************************\n");
	pr_info(PFX "module loading\n");

	ret = alloc_chrdev_region(&g_ivpci_devno, 0, g_max_devices, DRV_NAME);
	if (ret < 0) {
		pr_err(PFX "unable to allocate major number: %d\n", ret);
		goto out;
	}

	g_ivpci_devs = kzalloc(sizeof(struct vcxl_ivpci_device) * g_max_devices,
			       GFP_KERNEL);
	if (g_ivpci_devs == NULL) {
		goto unregister_chrdev;
	}
	init_vcxl_task_robust_hashtable();

	minor = MINOR(g_ivpci_devno);
	for (i = 0; i < g_max_devices; i++) {
		g_ivpci_devs[i].minor = minor;
		minor += 1;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	g_ivpci_class = class_create(DRV_NAME);
#else
	g_ivpci_class = class_create(THIS_MODULE, DRV_NAME);
#endif
	if (g_ivpci_class == NULL) {
		pr_err(PFX "unable to create the struct class\n");
		goto free_devs;
	}

	g_ivpci_major = MAJOR(g_ivpci_devno);
	pr_info(PFX "major: %d, minor: %d\n", g_ivpci_major,
		MINOR(g_ivpci_devno));

	ret = pci_register_driver(&vcxl_ivpci_driver);
	if (ret < 0) {
		pr_err(PFX "unable to register driver: %d\n", ret);
		goto destroy_class;
	}

	ret = init_procdeath_netlink();
	if (ret) {
		pr_err(PFX "fail to init netlink");
		goto destroy_class;
	}

	ret = fh_install_hooks(hooks, ARRAY_SIZE(hooks));
	if (ret) {
		pr_err(PFX "unable to hook exit_mm_release\n");
		goto destroy_class;
	}

	pr_info(PFX "module loaded\n");
	return 0;

destroy_class:
	class_destroy(g_ivpci_class);

free_devs:
	kfree(g_ivpci_devs);

unregister_chrdev:
	unregister_chrdev_region(g_ivpci_devno, g_max_devices);

out:
	return -1;
}

static void __exit vcxl_ivpci_exit(void)
{
	int ret;

	deinit_cxlcg_meta();
	vcxl_hash_index_exit();
	ret = exit_procdeath_netlink();
	if (ret) {
		pr_err(PFX "fail to exit procdeath netlink");
	}
	pci_unregister_driver(&vcxl_ivpci_driver);

	class_destroy(g_ivpci_class);

	kfree(g_ivpci_devs);
	exit_vcxl_task_robust_hashtable();

	unregister_chrdev_region(g_ivpci_devno, g_max_devices);

	fh_remove_hooks(hooks, ARRAY_SIZE(hooks));

	pr_info(PFX "module unloaded\n");
	pr_info(PFX
		"*********************************************************\n");
}

/************************************************
 * Just for eliminating the compiling warnings.
 ************************************************/

#define mymodinit(initfn)                         \
	static inline initcall_t __inittest(void) \
	{                                         \
		return initfn;                    \
	}                                         \
	int init_module(void) __cold __attribute__((alias(#initfn)));

#define mymodexit(exitfn)                         \
	static inline exitcall_t __exittest(void) \
	{                                         \
		return exitfn;                    \
	}                                         \
	void cleanup_module(void) __cold __attribute__((alias(#exitfn)));

mymodinit(vcxl_ivpci_init);
mymodexit(vcxl_ivpci_exit);

MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Demo PCI driver for emulated cxl using ivshmem device");
MODULE_VERSION(DRV_VERSION);
