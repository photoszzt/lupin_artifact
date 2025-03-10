#ifndef CXL_CGROUP_H_
#define CXL_CGROUP_H_

#ifdef USE_JJ_AB_LOCK
#define JJ_AB_MAX_PROC_SHIFT 6
#include "jj_ab_spin/jj_abortable_spinlock.h"
#elif defined(USE_TATAS)
#include "tatas/tatas_spinlock.h"
#endif

#include <linux/types.h>
#include <linux/fs.h>
#include "vcxl_def.h"
#include "cxl_dev.h"
#include "log.h"
#include "uapi/cxl_cgroup_udef.h"

#define SECRET_STR_LEN 20
#define MAX_PROC_PER_GROUP 16
#define MAX_GROUP_NUM 50
#define MAX_HOST_NUM 16

#define MSG_TYPE_OS_FAILURE 0
#define MSG_TYPE_LEADER_CHANGE 1
#define MSG_TYPE_PROC_FAILURE 2

struct global_proc_id {
	uint64_t osii; // OS instance identifier
	uint64_t local_proc_id;
};

struct cxlcg_args {
	char secret_str[20];
	struct global_proc_id proc_id;
};

struct member_proc {
	struct global_proc_id proc_id;
	struct task_struct *tsk;
	bool in_use;
};

struct cxlcg_entry {
	struct member_proc procs[MAX_PROC_PER_GROUP];
	char secret_str[SECRET_STR_LEN];
	bool in_use;
};

struct cxlcg_meta {
	struct cxlcg_entry entries[MAX_GROUP_NUM];
	atomic_t os_generation_ids
		[VCXL_MAX_SUPPORTED_MACHINES]; // generation ID table
	atomic_t os_heartbeats[VCXL_MAX_SUPPORTED_MACHINES]; // generation ID table
	atomic64_t
		cur_leader_osii; // OS instance identifier, os_id (8 bits) : gen_id (8 bits)

	struct vcxl_ivpci_device *ivpci_dev;

	struct ttas meta_lock;
};

void init_cxlcg_meta(struct vcxl_ivpci_device *ivpci_dev, void *addr);
void recover_cxlcg_meta(struct vcxl_ivpci_device *ivpci_dev, void *addr);
void deinit_cxlcg_meta(void);
void increment_gen_id(void);
int get_os_gen_id(u16 machine_id);
void do_os_failure_detection(struct file *filp);

int do_create_cxlcg(struct cxlcg_args *args);
int do_delete_cxlcg(struct cxlcg_args *args);
int do_join_cxlcg(struct cxlcg_args *args);
int do_leave_cxlcg(struct cxlcg_args *args);
int lock_transfer_test(struct file *filp, __u16 machine_id);

void notify_proc_failure(struct task_struct *tsk);

#endif // CXL_CGROUP_H_
