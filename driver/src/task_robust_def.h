#ifndef TASK_ROBUST_DEF_H_
#define TASK_ROBUST_DEF_H_

#define TASK_ROBURST_BITS 12
#define TASK_ROBURST_TAB_SIZE (1 << TASK_ROBURST_BITS)

/**
 * the hashed task and robust list head entry. This avoids
 * changing the task_struct.
*/
struct vcxl_task_robust_node {
    struct hlist_node hlink;
    struct task_struct *task;
    struct robust_mutex_list __user *rh;
    struct meta_dram* meta_dram;
    u8 __iomem* regs_addr;
    struct vcxl_futex_hash_bucket* futex_tab;
    /* this is the process's logical id */
    uint16_t user_proc_id;
};

struct vcxl_task_robust_bucket {
    struct hlist_head chain;
    spinlock_t lock;
};

extern struct vcxl_task_robust_bucket task_robust_table[TASK_ROBURST_TAB_SIZE];

void init_vcxl_task_robust_hashtable(void);
void exit_vcxl_task_robust_hashtable(void);
long do_set_robust_list(struct file *filp,
                        struct robust_mutex_list __user *uhead);
struct vcxl_task_robust_bucket *cxl_task_roburst_hashbucket(struct task_struct* curr);

#endif // TASK_ROBUST_DEF_H_
