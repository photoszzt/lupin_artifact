/**
 * Notify process death
 *
*/
#include <linux/types.h>
#include <linux/interrupt.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include <linux/list.h>
#include "cxl_dev.h"
#include "log.h"
#include "proc_death.h"
#include "task_robust_def.h"
#include "uapi/vcxl_proc_death_notify.h"
#include "uapi/vcxl_shm.h"
#include "circular_queue.h"

int do_register_process(struct file *filp, __u16 uproc_logical_id)
{
    int rval = 0;
    struct vcxl_ivpci_device *ivpci_dev;
    struct vcxl_task_robust_bucket *hb = cxl_task_roburst_hashbucket(current);
    struct vcxl_task_robust_node* cursor = NULL, *node = NULL;
    struct vcxl_task_robust_node* newnode = kmalloc(sizeof(struct vcxl_task_robust_node), GFP_KERNEL);
    struct hlist_node* n;
    bool found = false;

    ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
    BUG_ON(ivpci_dev == NULL);
    BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);

    spin_lock(&hb->lock);
    hlist_for_each_entry_safe(cursor, n, (struct hlist_head *)hb, hlink) {
        if (cursor->task == current) {
            node = cursor;
            found = true;
            break;
        }
    }
    if (node == NULL) {
        node = newnode;
    }
    node->meta_dram = &ivpci_dev->meta_dram;
    node->regs_addr = ivpci_dev->regs_addr;
    node->user_proc_id = uproc_logical_id;
    node->task = current;
    /* must register node first before setting the robust list */
    node->rh = NULL;
    node->futex_tab = &ivpci_dev->futex_hash_table[0];
    if (!found) {
        hlist_add_head(&node->hlink, &hb->chain);
    }
    spin_unlock(&hb->lock);
    pr_info("register task_struct %p with logical id %u\n", current, uproc_logical_id);
    if (found) {
        kfree(newnode);
    }
    return rval;
}

/* process context */
struct vcxl_task_robust_node* notify_proc_death_to_other_machine(struct task_struct *tsk)
{
    struct vcxl_task_robust_bucket * hb = cxl_task_roburst_hashbucket(tsk);
    struct vcxl_task_robust_node* node;
    struct vcxl_task_robust_node* found_node = NULL;
    struct hlist_node* n;
    s64 proc_ids;
    int i, count, ret;
    struct circular_queue * q;
    pid_t proc;
    uint32_t ivpos_reg;

    spin_lock(&hb->lock);
    hlist_for_each_entry_safe(node, n, (struct hlist_head *)hb, hlink) {
        if (node->task == tsk) {
            found_node = node;
            break;
        }
    }
    spin_unlock(&hb->lock);
    if (found_node) {
        pr_info("found node, total machine %u, cur machine_id: %u\n", found_node->meta_dram->total_machines, found_node->meta_dram->machine_id);
        proc = task_tgid_vnr(tsk);
        proc_ids = compose_procdeath(proc, found_node->user_proc_id);

        for (i = 1; i <= found_node->meta_dram->total_machines; i++) {
            if (i != found_node->meta_dram->machine_id) {
                q = peer_vcxl_procdeath_queue(node->meta_dram, i);
                if (q == NULL) {
                    pr_err("vmid is not correct\n");
                    continue;
                }
                count = 0;
                ret = circular_queue_enqueue(q, proc_ids, found_node->meta_dram->machine_id);
                while (ret == -1) {
                    CPU_PAUSE();
                    count+=1;
                    if (count == 100) {
                        pr_err("fail to send fail notification of proc %u(%u) to machine %d\n",
                            node->user_proc_id, proc, i);
                        break;
                    }
                    ret = circular_queue_enqueue(q, proc_ids, found_node->meta_dram->machine_id);
                }
                ivpos_reg = compose_ivpos_msix_reg(CXL_IVPCI_VECTOR_PROC_DEATH_NOTIFY, i);
                iowrite32(ivpos_reg, found_node->regs_addr + DOORBELL_OFF);
            } else {
                send_genlmsg(proc_ids);
            }
        }
    }
    return found_node;
}

void send_msg_to_other_machines(uint64_t msg, struct vcxl_ivpci_device *ivpci_dev) {
    struct circular_queue *q;
    uint32_t ivpos_reg;
    int i, count, ret;

    dev_info(&ivpci_dev->dev->dev, PFX "trying to send message to other machines\n");
    for (i = 1; i <= ivpci_dev->meta_dram.total_machines; i++) {
        if (i != ivpci_dev->meta_dram.machine_id) {
            dev_info(&ivpci_dev->dev->dev, PFX "trying to send message to machine %d\n", i);
            q = peer_vcxl_procdeath_queue(&ivpci_dev->meta_dram, i);
            if (q == NULL) {
                pr_err("vmid is not correct\n");
                continue;
            }
            count = 0;
            ret = circular_queue_enqueue(q, msg, ivpci_dev->meta_dram.machine_id);
            while (ret == -1) {
                CPU_PAUSE();
                count += 1;
                if (count == 100) {
                    pr_err("failed to send fail notification of proc %lld to machine %d\n",
                        msg, i);
                    break;
                }
                ret = circular_queue_enqueue(q, msg, ivpci_dev->meta_dram.machine_id);
            }
            ivpos_reg = compose_ivpos_msix_reg(CXL_IVPCI_VECTOR_PROC_DEATH_NOTIFY, i);
            iowrite32(ivpos_reg, ivpci_dev->regs_addr + DOORBELL_OFF);
            dev_info(&ivpci_dev->dev->dev, PFX "sent message to machine %d\n", i);
        }
    }
}

void remove_robust_node(struct task_struct *tsk, struct vcxl_task_robust_node* node)
{
    struct vcxl_task_robust_bucket * hb = cxl_task_roburst_hashbucket(tsk);

    spin_lock(&hb->lock);
    hlist_del(&node->hlink);
    spin_unlock(&hb->lock);
    kfree(node);
}

/**
 * returns NULL if nothing found in the queue
 * returns a pointer to an array if elements found
 * caller needs to free the array
 */
static s64* fetch_vals(struct meta_dram* meta_dram, int* idx_out)
{
    struct circular_queue* q = self_vcxl_procdeath_queue(meta_dram);
    s64* vals = kmalloc_array(CQUEUE_SIZE, sizeof(s64), GFP_ATOMIC);
    bool got_val = false;
    s64 proc_ids = -1;
    int idx = 0, ret = 0;

    if (q == NULL) {
        kfree(vals);
        pr_err("fail to find procdeath queue; make sure the vm id starts from 1\n");
        return NULL;
    }
    ret = circular_queue_dequeue(q, &proc_ids, meta_dram->machine_id);
    while (ret != -1) {
        if (!got_val) {
            got_val = true;
        }
        vals[idx] = proc_ids;
        idx += 1;
        if (idx == CQUEUE_SIZE) {
            break;
        }
        ret = circular_queue_dequeue(q, &proc_ids, meta_dram->machine_id);
    }
    if (!got_val) {
        kfree(vals);
        return NULL;
    }
    *idx_out = idx;
    return vals;
}

static void process_proc_death(struct meta_dram* meta_dram)
{
    int i, idx;
    s64* vals = fetch_vals(meta_dram, &idx);

    if (vals == NULL) {
        /* empty queue */
        return;
    }
    for (i = 0; i < idx; i++) {
        send_genlmsg(vals[i]);
    }
    kfree(vals);
}

irqreturn_t vcxl_ivpci_interrupt_proc_death_task_notify(int irq, void *dev_id)
{
    struct vcxl_ivpci_device *ivpci_dev = (struct vcxl_ivpci_device *)dev_id;

    if (unlikely(ivpci_dev == NULL)) {
        return IRQ_NONE;
    }

    dev_info(&ivpci_dev->dev->dev, PFX "interrupt: %d\n", irq);
    process_proc_death(&ivpci_dev->meta_dram);
    return IRQ_HANDLED;
}

/* in softirq */
void poll_proc_death_task(struct tasklet_struct *t)
{
    int i, idx;
    struct vcxl_proc_death_task *task = from_tasklet(task, t, tasklet);
    s64* vals = fetch_vals(task->meta_dram, &idx);

    if (vals == NULL) {
        /* empty queue */
        tasklet_schedule(t);
        return;
    }
    for (i = 0; i < idx; i++) {
        send_genlmsg(vals[i]);
    }
    kfree(vals);
    tasklet_schedule(t);
}
