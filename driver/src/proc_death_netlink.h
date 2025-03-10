#ifndef PROC_DEATH_NETLINK_H_
#define PROC_DEATH_NETLINK_H_

#include "common_macro.h"
#include "uapi/vcxl_proc_death_notify.h"

int init_procdeath_netlink(void) WARN_UNUSED_RESULT;
int exit_procdeath_netlink(void) WARN_UNUSED_RESULT;
void send_genlmsg(s64 proc_ids);
void send_genlmsg_gfpkernel(s64 proc_ids);

#endif // PROC_DEATH_NETLINK_H_
