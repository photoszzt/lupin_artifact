#include <linux/interrupt.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include "proc_death.h"
#include "proc_death_netlink.h"
#include "uapi/vcxl_proc_death_notify.h"
#include "uapi/vcxl_udef.h"
#include "uapi/cxl_cgroup_udef.h"
#include "cxl_cgroup.h"
#include "atomic_int_op.h"
#include "alloc_impl.h"

static struct cxlcg_meta *meta = NULL;
static struct vcxl_ivpci_device *global_ivpci_dev;
static int os_gen_ids_snapshot[VCXL_MAX_SUPPORTED_MACHINES];
static int os_heartbeats[VCXL_MAX_SUPPORTED_MACHINES];
static uint64_t my_osii = 0;
static bool os_live[VCXL_MAX_SUPPORTED_MACHINES];

static force_inline void recoverable_lock_init(struct cxlcg_meta *meta)
{
	ttas_init(&meta->meta_lock);
}

static force_inline void recoverable_lock(struct cxlcg_meta *meta)
{
	int gen_id = get_os_gen_id(global_ivpci_dev->meta_dram.machine_id - 1);
	ttas_lock_with_gen(&meta->meta_lock,
			   global_ivpci_dev->meta_dram.machine_id, gen_id);
}

static force_inline void recoverable_unlock(struct cxlcg_meta *meta)
{
	ttas_unlock(&meta->meta_lock, global_ivpci_dev->meta_dram.machine_id);
}

static force_inline WARN_UNUSED_RESULT enum transfer_st
recoverable_lock_recover(struct cxlcg_meta *meta, u16 old_machine_id)
{
	int gen_id = get_os_gen_id(global_ivpci_dev->meta_dram.machine_id - 1);
	return ttas_lock_recover(&meta->meta_lock, old_machine_id,
				 global_ivpci_dev->meta_dram.machine_id,
				 gen_id);
}

static force_inline void recoverable_lock_done_recover(struct cxlcg_meta *meta,
						       enum transfer_st st)
{
	done_ttas_lock_recover(&meta->meta_lock,
			       global_ivpci_dev->meta_dram.machine_id, st);
}

static uint32_t hash_func(const char *word)
{
	uint32_t hash = 0;
	int i;

	for (i = 0; word[i] != '\0'; i++)
		hash = 31 * hash + (uint32_t)word[i];

	return hash;
}

// in softirq context;
static void do_leader_election(void)
{
	s64 cur_leader_osii, msg = 0;
	uint32_t cur_leader_os_id, cur_leader_gen_id;
	s64 tmp;

	cur_leader_osii = atomic64_read_acquire(&meta->cur_leader_osii);
	cur_leader_os_id = GET_OS_ID(cur_leader_osii);
	cur_leader_gen_id = GET_GEN_ID(cur_leader_osii);
	// PRINT_INFO("current leader: machine_id = %u, gen_id = %d\n",
	// 	   GET_OS_ID(cur_leader_osii), GET_GEN_ID(cur_leader_osii));

	if (cur_leader_osii == 0 ||
	    cur_leader_gen_id !=
		    atomic_read_acquire(
			    &meta->os_generation_ids[cur_leader_os_id - 1])) {
		tmp = atomic64_cmpxchg(&meta->cur_leader_osii, cur_leader_osii,
				       my_osii);
		if (tmp == cur_leader_osii) {
			msg = atomic64_read_acquire(&meta->cur_leader_osii);
			SET_MSG_TYPE(msg, MSG_TYPE_LEADER_CHANGE);
			send_genlmsg(msg);
			PRINT_INFO(
				"leader change detected, current leader = %u, gen_id = %u, new leader: machine_id = %u, gen_id = %d\n",
				cur_leader_os_id, cur_leader_gen_id,
				GET_OS_ID(msg), GET_GEN_ID(msg));
		}
	}
}

int lock_transfer_test(struct file *filp, __u16 machine_id)
{
	struct vcxl_ivpci_device *ivpci_dev;
	enum transfer_st st;
	int ret;
	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);
	BUG_ON(ivpci_dev != global_ivpci_dev);

	st = meta_lock_recover(ivpci_dev->meta_shmem, &ivpci_dev->meta_dram,
			       machine_id);
	if (st == TRANSFERED) {
		PRINT_INFO("help to recover OS %d", machine_id);
		print_allocation(ivpci_dev->meta_dram.mem_start,
				 ivpci_dev->meta_shmem);
		ret = recover_with_redolog(global_ivpci_dev->meta_shmem,
					   &global_ivpci_dev->meta_dram);
		if (ret != 0) {
			PRINT_ERR("fail to recover with redolog: %d\n", -ret);
			return -ret;
		}
		print_allocation(ivpci_dev->meta_dram.mem_start,
				 ivpci_dev->meta_shmem);
	}
	meta_lock_done_recover(ivpci_dev->meta_shmem, &ivpci_dev->meta_dram,
			       st);
	return 0;
}

bool stop_heartbeats = false; // just for testing
// in process context when calling from ioctl
void do_os_failure_detection(struct file *filp)
{
	static uint64_t timeout_times[VCXL_MAX_SUPPORTED_MACHINES] = { 0 };
	struct vcxl_ivpci_device *ivpci_dev;
	uint64_t msg = 0;
	uint32_t os_id = GET_OS_ID(my_osii);
	enum transfer_st st;
	int ret;
	struct timespec64 det_time, netlink_sent, nlsend_ela;
	// struct timespec64 ts;
	// int snap_gen;
	ivpci_dev = (struct vcxl_ivpci_device *)filp->private_data;
	BUG_ON(ivpci_dev == NULL);
	BUG_ON(ivpci_dev->meta_dram.mem_start == NULL);
	BUG_ON(os_id == 0);

	// machine_id starts from 1
	if (stop_heartbeats == false) {
		atomic_fetch_add(1, &meta->os_heartbeats[os_id - 1]);
	}
	for (int i = 0; i < ivpci_dev->meta_dram.total_machines; i++) {
		if (os_heartbeats[i] !=
		    atomic_read_acquire(&meta->os_heartbeats[i])) {
			os_heartbeats[i] =
				atomic_read_acquire(&meta->os_heartbeats[i]);
			os_live[i] = true;
			// ktime_get_real_ts64(&ts);
			// PRINT_INFO("OS %d is live at %lld.%09ld\n", i + 1,
			// 	   ts.tv_sec, ts.tv_nsec);
			timeout_times[i] = 0;
		} else {
			timeout_times[i]++;
			// ktime_get_real_ts64(&ts);
			// if (os_live[i]) {
			// 	PRINT_INFO("OS %d might die at %lld.%09ld\n",
			// 		   i + 1, ts.tv_sec, ts.tv_nsec);
			// }
			if (timeout_times[i] >= 2) {
				if (os_live[i]) {
					atomic_fetch_add(
						1, &meta->os_generation_ids[i]);
					SET_OS_ID(msg, i + 1);
					SET_GEN_ID(msg, os_gen_ids_snapshot[i]);
					SET_MSG_TYPE(msg, MSG_TYPE_OS_FAILURE);
					ktime_get_real_ts64(&det_time);
					send_genlmsg_gfpkernel(msg);
					ktime_get_real_ts64(&netlink_sent);
					nlsend_ela = timespec64_sub(
						netlink_sent, det_time);
					PRINT_INFO(
						"OS heartbeats: timeout! OS %d maybe dead, machine_id = %u, gen_id = %d, at %lld.%09ld s, netlink_send elapsed: %lld.%09ld s\n",
						i + 1, i + 1,
						os_gen_ids_snapshot[i],
						det_time.tv_sec,
						det_time.tv_nsec,
						nlsend_ela.tv_sec,
						nlsend_ela.tv_nsec);
					// lock transfer
					st = meta_lock_recover(
						ivpci_dev->meta_shmem,
						&ivpci_dev->meta_dram, i + 1);
					if (st == TRANSFERED) {
						PRINT_INFO(
							"help to recover OS %d",
							i + 1);
						ret = recover_with_redolog(
							ivpci_dev->meta_shmem,
							&ivpci_dev->meta_dram);
						if (ret != 0) {
							PRINT_ERR(
								"fail to recover with redolog: %d\n",
								-ret);
						}
					}
					meta_lock_done_recover(
						ivpci_dev->meta_shmem,
						&ivpci_dev->meta_dram, st);
				}
				os_gen_ids_snapshot[i] = atomic_read_acquire(
					&meta->os_generation_ids[i]);
				os_live[i] = false;

				// reset the alarm
				timeout_times[i] = 0;
			}
		}
	}

	// for (int i = 0; i < global_ivpci_dev->meta_dram.total_machines; i++) {
	// 	if (os_gen_ids_snapshot[i] !=
	// 	    atomic_read_acquire(&meta->os_generation_ids[i])) {
	// 		snap_gen = os_gen_ids_snapshot[i];
	// 		os_gen_ids_snapshot[i] = atomic_read_acquire(
	// 			&meta->os_generation_ids[i]);
	// 		SET_OS_ID(msg, i + 1);
	// 		SET_GEN_ID(msg, os_gen_ids_snapshot[i]);
	// 		SET_MSG_TYPE(msg, MSG_TYPE_OS_FAILURE);
	// 		send_genlmsg(msg);
	// 		PRINT_INFO(
	// 			"OS failure detected: machine_id = %u, snap_gen = %d, new gen_id = %d\n",
	// 			i + 1, snap_gen, os_gen_ids_snapshot[i]);
	// 	}
	// }
}

static struct timer_list timer;
static const int repeat_ms = 4;
// in softirq context;
// static void detect_and_notify_failures(struct timer_list *t)
// {
// 	do_os_failure_detection();
// 	// do_leader_election();
// 	mod_timer(t, jiffies + msecs_to_jiffies(repeat_ms));
// }

void increment_gen_id(void)
{
	atomic_fetch_add(
		1,
		&meta->os_generation_ids[global_ivpci_dev->meta_dram.machine_id -
					 1]);
}

int get_os_gen_id(u16 machine_id)
{
	return atomic_read_acquire(&meta->os_generation_ids[machine_id]);
}

// os_id starts from 1
void init_cxlcg_meta(struct vcxl_ivpci_device *ivpci_dev, void *addr)
{
	uint32_t machine_id;
	int i;

	global_ivpci_dev = ivpci_dev;

	meta = (struct cxlcg_meta *)addr;
	memset((void *)meta, 0, CXLCG_META_SIZE);
	for (i = 0; i < VCXL_MAX_SUPPORTED_MACHINES; i++) {
		os_live[i] = true;
		os_heartbeats[i] = 0;
		meta->os_heartbeats[i] = (atomic_t)ATOMIC_INIT(0);
	}

	recoverable_lock_init(meta);

	machine_id = global_ivpci_dev->meta_dram.machine_id;
	memcpy(os_gen_ids_snapshot, meta->os_generation_ids,
	       sizeof(os_gen_ids_snapshot));
	SET_OS_ID(my_osii, machine_id);
	SET_GEN_ID(my_osii, atomic_read_acquire(
				    &meta->os_generation_ids[machine_id - 1]));
	atomic64_set_release(&meta->cur_leader_osii, my_osii);
	PRINT_INFO("init: machine_id = %u, gen_id = %u\n", GET_OS_ID(my_osii),
		   GET_GEN_ID(my_osii));

	/* init timer to detect OS failure */
	// timer_setup(&timer, detect_and_notify_failures, 0);
	// mod_timer(&timer, jiffies + msecs_to_jiffies(repeat_ms));
}

void recover_cxlcg_meta(struct vcxl_ivpci_device *ivpci_dev, void *addr)
{
	uint32_t machine_id;
	int i, j;
	enum transfer_st st;

	global_ivpci_dev = ivpci_dev;

	meta = (struct cxlcg_meta *)addr;

	/* increment generation ID */
	increment_gen_id();
	machine_id = global_ivpci_dev->meta_dram.machine_id;
	SET_OS_ID(my_osii, machine_id);
	SET_GEN_ID(my_osii, atomic_read_acquire(
				    &meta->os_generation_ids[machine_id - 1]));

	/* avoid sending OS failure notification to itself */
	/* get snapshot after increment generation ID */
	memcpy(os_gen_ids_snapshot, meta->os_generation_ids,
	       sizeof(os_gen_ids_snapshot));
	for (i = 0; i < VCXL_MAX_SUPPORTED_MACHINES; i++) {
		os_live[i] = true;
	}

	// remove dead processes from CxlCG
	st = recoverable_lock_recover(meta, ivpci_dev->meta_dram.machine_id);
	if (st != TRANSFERED) {
		recoverable_lock(meta);
		st = TRANSFERED;
	}
	// find it first
	for (i = 0; i < MAX_GROUP_NUM; i++) {
		if (meta->entries[i].in_use == true) {
			for (j = 0; j < MAX_PROC_PER_GROUP; j++) {
				if (meta->entries[i].procs[j].in_use == true &&
				    GET_OS_ID(meta->entries[i]
						      .procs[j]
						      .proc_id.osii) ==
					    machine_id) {
					// remove it from the cgroup
					meta->entries[i].procs[j].proc_id.osii =
						0;
					meta->entries[i]
						.procs[j]
						.proc_id.local_proc_id = 0;
					meta->entries[i].procs[j].tsk = NULL;
					meta->entries[i].procs[j].in_use =
						false;
				}
			}
		}
	}
	recoverable_unlock(meta);
	PRINT_INFO("recover: machine_id = %u, gen_id = %u\n",
		   GET_OS_ID(my_osii), GET_GEN_ID(my_osii));

	/* init timer to detect OS failure */
	// timer_setup(&timer, detect_and_notify_failures, 0);
	// mod_timer(&timer, jiffies + msecs_to_jiffies(repeat_ms));
}

void deinit_cxlcg_meta(void)
{
	del_timer(&timer);
}

int do_create_cxlcg(struct cxlcg_args *args)
{
	int hash, ret = 0;

	hash = hash_func((uint8_t *)args->secret_str) % MAX_GROUP_NUM;
	dev_info(&global_ivpci_dev->dev->dev,
		 PFX "create CxlCG: secret = %s, hash = %d\n", args->secret_str,
		 hash);

	recoverable_lock(meta);
	if (meta->entries[hash].in_use == true) {
		dev_info(&global_ivpci_dev->dev->dev,
			 PFX "create CxlCG failed: cgroup %d not available\n",
			 hash);
		ret = -1;
		goto unlock_out;
	}
	memcpy(&meta->entries[hash].secret_str, args->secret_str,
	       SECRET_STR_LEN);
	meta->entries[hash].in_use = true;

unlock_out:
	recoverable_unlock(meta);
	return ret;
}

int do_delete_cxlcg(struct cxlcg_args *args)
{
	int hash, ret = 0;

	hash = hash_func((uint8_t *)args->secret_str) % MAX_GROUP_NUM;
	dev_info(&global_ivpci_dev->dev->dev,
		 PFX "delete CxlCG: secret = %s, hash = %d\n", args->secret_str,
		 hash);

	recoverable_lock(meta);
	memset(&meta->entries[hash], 0, sizeof(struct cxlcg_entry));
	recoverable_unlock(meta);
	return ret;
}

int do_join_cxlcg(struct cxlcg_args *args)
{
	int hash, i, ret = 0;

	hash = hash_func((uint8_t *)args->secret_str) % MAX_GROUP_NUM;
	args->proc_id.osii = my_osii;
	dev_info(
		&global_ivpci_dev->dev->dev,
		PFX
		"join CxlCG: secret = %s, hash = %d, global proc ID = %u, %u, %lld\n",
		args->secret_str, hash, GET_OS_ID(args->proc_id.osii),
		GET_GEN_ID(args->proc_id.osii), args->proc_id.local_proc_id);

	recoverable_lock(meta);
	// see if it is already a member
	for (i = 0; i < MAX_PROC_PER_GROUP; i++) {
		if (meta->entries[hash].procs[i].in_use == true &&
		    GET_OS_ID(meta->entries[hash].procs[i].proc_id.osii) ==
			    GET_OS_ID(args->proc_id.osii) &&
		    GET_GEN_ID(meta->entries[hash].procs[i].proc_id.osii) ==
			    GET_GEN_ID(args->proc_id.osii) &&
		    meta->entries[hash].procs[i].proc_id.local_proc_id ==
			    args->proc_id.local_proc_id) {
			dev_info(
				&global_ivpci_dev->dev->dev,
				PFX
				"join CxlCG failed: already a member of cgroup %d available\n",
				hash);
			goto unlock_out;
		}
	}
	// find an available slot
	for (i = 0; i < MAX_PROC_PER_GROUP; i++)
		if (meta->entries[hash].procs[i].in_use ==
		    false) // only check the local process ID
			break;
	if (i >= MAX_PROC_PER_GROUP) {
		dev_info(
			&global_ivpci_dev->dev->dev,
			PFX
			"join CxlCG failed: cgroup %d available, but no available slot\n",
			hash);
		ret = -1;
		goto unlock_out;
	}
	// add it to the cgroup
	meta->entries[hash].procs[i].proc_id.osii = args->proc_id.osii;
	meta->entries[hash].procs[i].proc_id.local_proc_id =
		args->proc_id.local_proc_id;
	meta->entries[hash].procs[i].tsk =
		current->group_leader; // storing the group leader
	meta->entries[hash].procs[i].in_use = true;

unlock_out:
	recoverable_unlock(meta);
	return ret;
}

int do_leave_cxlcg(struct cxlcg_args *args)
{
	int hash, i, ret = 0;

	hash = hash_func((uint8_t *)args->secret_str) % MAX_GROUP_NUM;
	dev_info(
		&global_ivpci_dev->dev->dev,
		PFX
		"leave CxlCG: secret = %s, hash = %d, global proc ID = %u, %u, %lld\n",
		args->secret_str, hash, GET_OS_ID(args->proc_id.osii),
		GET_GEN_ID(args->proc_id.osii), args->proc_id.local_proc_id);

	recoverable_lock(meta);
	// find it first
	for (i = 0; i < MAX_PROC_PER_GROUP; i++) {
		if (meta->entries[hash].procs[i].in_use == true &&
		    GET_OS_ID(meta->entries[hash].procs[i].proc_id.osii) ==
			    GET_OS_ID(args->proc_id.osii) &&
		    GET_GEN_ID(meta->entries[hash].procs[i].proc_id.osii) ==
			    GET_GEN_ID(args->proc_id.osii) &&
		    meta->entries[hash].procs[i].proc_id.local_proc_id ==
			    args->proc_id.local_proc_id) {
			break;
		}
	}
	if (i >= MAX_PROC_PER_GROUP) {
		dev_info(
			&global_ivpci_dev->dev->dev,
			PFX
			"leave CxlCG failed: already removed or not a member of cgroup %d\n",
			hash);
		goto unlock_out;
	}
	// remove it from the cgroup
	meta->entries[hash].procs[i].proc_id.osii = 0;
	meta->entries[hash].procs[i].proc_id.local_proc_id = 0;
	meta->entries[hash].procs[i].tsk = NULL;
	meta->entries[hash].procs[i].in_use = false;

unlock_out:
	recoverable_unlock(meta);
	return ret;
}

// do scan in a lock-free manner, otherwise soft lookup may occur (not sure why)
void notify_proc_failure(struct task_struct *tsk)
{
	uint64_t msg = 0;
	bool found = false;
	int i, j;
	struct timespec64 det_ts, nlsend_ts, nlsend_ela, nlsend_beg,
		nlsend_after_notif_ela;

	if (meta == NULL)
		return;

	// PRINT_INFO("process failure callback: task = %p\n", tsk);
	recoverable_lock(meta);
	// find it first
	for (i = 0; i < MAX_GROUP_NUM; i++) {
		if (meta->entries[i].in_use == true) {
			for (j = 0; j < MAX_PROC_PER_GROUP; j++) {
				if (meta->entries[i].procs[j].in_use == true &&
				    meta->entries[i].procs[j].tsk ==
					    tsk->group_leader) { // matching on the group leader
					found = true;
					break;
				}
			}
			// once found, send notification to user space
			if (found == true) {
				msg = meta->entries[i].procs[j].proc_id.osii;
				SET_PROC_ID(msg,
					    meta->entries[i]
						    .procs[j]
						    .proc_id.local_proc_id);
				SET_MSG_TYPE(msg, MSG_TYPE_PROC_FAILURE);

				ktime_get_real_ts64(&det_ts);
				// PRINT_INFO(
				// 	"process failure detected: machine_id = %u, gen_id = %u, proc_id = %u, at %lld.%09ld s\n",
				// 	GET_OS_ID(msg), GET_GEN_ID(msg),
				// 	GET_PROC_ID(msg), det_ts.tv_sec,
				// 	det_ts.tv_nsec);

				// when a process fails, it's removed from the cgroup
				// A process has to rejoin the cgroup
				meta->entries[i].procs[j].in_use = false;
				meta->entries[i].procs[j].tsk = NULL;
				break;
			}
		}
	}
	recoverable_unlock(meta);
	if (found) {
		ktime_get_real_ts64(&nlsend_beg);
		send_genlmsg_gfpkernel(msg);
		ktime_get_real_ts64(&nlsend_ts);
		nlsend_after_notif_ela = timespec64_sub(nlsend_ts, det_ts);
		nlsend_ela = timespec64_sub(nlsend_ts, nlsend_beg);
		PRINT_INFO(
			"process failure detected: machine_id = %u, gen_id = %u, proc_id = %u, at %lld.%09ld s, nlsend after detect elapsed: %lld.%09ld s, nlsend elapsed: %lld.%09ld s\n",
			GET_OS_ID(msg), GET_GEN_ID(msg), GET_PROC_ID(msg),
			det_ts.tv_sec, det_ts.tv_nsec,
			nlsend_after_notif_ela.tv_sec,
			nlsend_after_notif_ela.tv_nsec, nlsend_ela.tv_sec,
			nlsend_ela.tv_nsec);
		// send_msg_to_other_machines(msg, global_ivpci_dev);
	}
}
