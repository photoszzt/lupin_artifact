#ifndef VCXL_PROC_DEATH_NOTIFY_H_
#define VCXL_PROC_DEATH_NOTIFY_H_

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bit 0-31 pid
 * bit 32-47 logical id
*/
#define PROCDEATH_PID_SHIFT 0
#define PROCDEATH_PID_MASK 0xffffffff
#define PROCDEATH_LOGICALID_MASK 0xff00000000
#define PROCDEATH_LOGICALID_SHIFT 32

#define PROC_DEATH_GROUP 21
#define PROC_DEATH_PROTO NETLINK_GENERIC
#define PROC_DEATH_GENL_MCAST_GROUP_NAME "pd_mc_grp"
#define PROC_DEATH_GENL_FAMILY_NAME "procdeath"
#define PROC_DEATH_GENL_VERSION 0x1

enum {
    PROCDEATH_ATTR_UNSPEC,
    PROCDEATH_ATTR_ID,
    PROCDEATH_ATTR_PAD,
    __PROCDEATH_ATTR_MAX,
};
#define PROCDEATH_ATTR_MAX (__PROCDEATH_ATTR_MAX - 1)

enum {
    PROCDEATH_CMD_UNSPEC,
    PROCDEATH_CMD_POLLOUT,
    __PROCDEATH_CMD_MAX,
};
#define PROCDEATH_CMD_MAX (__PROCDEATH_CMD_MAX - 1)

union Uint64_t {
    uint64_t num;
    char buffer[8];
};
_Static_assert(sizeof(union Uint64_t) == sizeof(uint64_t), "union Uint64_t size larger than uint64_t");

static WARN_UNUSED_RESULT force_inline uint32_t procdeath_get_pid(uint64_t val)
{
    return (uint32_t)((val & (uint64_t)PROCDEATH_PID_MASK) >> PROCDEATH_PID_SHIFT);
}

static WARN_UNUSED_RESULT force_inline uint16_t procdeath_get_logicalid(uint64_t val)
{
    return (uint16_t)((val & (uint64_t)PROCDEATH_LOGICALID_MASK) >> PROCDEATH_LOGICALID_SHIFT);
}

_Static_assert(sizeof(pid_t) == sizeof(uint32_t), "pid_t should be 32 bits");
static WARN_UNUSED_RESULT force_inline uint64_t compose_procdeath(pid_t pid, uint16_t logical_id)
{
    return (uint64_t)pid << PROCDEATH_PID_SHIFT | ((uint64_t)logical_id) << PROCDEATH_LOGICALID_SHIFT;
}

#ifdef __cplusplus
}
#endif

#endif // VCXL_PROC_DEATH_NOTIFY_H_
