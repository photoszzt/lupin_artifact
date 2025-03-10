#ifndef PROC_DEATH_H_
#define PROC_DEATH_H_

#include <linux/types.h>
#include "common_macro.h"
#include "task_robust_def.h"
#include "proc_death_netlink.h"
#include "cxl_dev.h"

void poll_proc_death_task(struct tasklet_struct *t);
struct vcxl_task_robust_node* notify_proc_death_to_other_machine(struct task_struct *tsk) WARN_UNUSED_RESULT;
void remove_robust_node(struct task_struct *tsk, struct vcxl_task_robust_node* node);
int do_register_process(struct file *filp, __u16 uproc_logical_id) WARN_UNUSED_RESULT;
irqreturn_t vcxl_ivpci_interrupt_proc_death_task_notify(int irq, void *dev_id);
void send_msg_to_other_machines(uint64_t msg, struct vcxl_ivpci_device *ivpci_dev);

#endif // PROC_DEATH_H_
