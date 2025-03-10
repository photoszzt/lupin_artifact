#include <linux/interrupt.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include "proc_death_netlink.h"
#include "uapi/vcxl_proc_death_notify.h"

static const struct genl_multicast_group procdeath_mcast_grps[] = {
	{
		.name = PROC_DEATH_GENL_MCAST_GROUP_NAME,
	},
};

static const struct nla_policy procdeath_attr_policy[PROCDEATH_ATTR_MAX + 1] = {
	[PROCDEATH_ATTR_ID] = { .type = NLA_U64 },
};

static struct genl_family procdeath_gnl_family = {
	.hdrsize = 0,
	.name = PROC_DEATH_GENL_FAMILY_NAME,
	.version = PROC_DEATH_GENL_VERSION,
	.module = THIS_MODULE,
	.maxattr = PROCDEATH_ATTR_MAX,
	.policy = procdeath_attr_policy,
	.mcgrps = procdeath_mcast_grps,
	.n_mcgrps = ARRAY_SIZE(procdeath_mcast_grps),
};

int init_procdeath_netlink(void)
{
	return genl_register_family(&procdeath_gnl_family);
}

int exit_procdeath_netlink(void)
{
	return genl_unregister_family(&procdeath_gnl_family);
}

void send_genlmsg_gfpkernel(s64 proc_ids)
{
	int rc;
	struct sk_buff *skb;
	void *msg_head;

	skb = genlmsg_new(sizeof(uint64_t), GFP_KERNEL);
	if (skb == NULL) {
		PRINT_ERR("%s: %d failed to allocate skb for netlink\n",
			  __FILE__, __LINE__);
		return;
	}
	msg_head = genlmsg_put(skb, 0, 0, &procdeath_gnl_family, 0,
			       PROCDEATH_CMD_POLLOUT);
	if (!msg_head) {
		PRINT_ERR("%s: %d failed to put msg_head\n", __FILE__,
			  __LINE__);
		nlmsg_free(skb);
		return;
	}
	rc = nla_put_u64_64bit(skb, PROCDEATH_ATTR_ID, proc_ids,
			       PROCDEATH_ATTR_PAD);
	if (rc) {
		PRINT_ERR("%s: %d failed to put id: %d\n", __FILE__, __LINE__,
			  rc);
		nlmsg_free(skb);
		return;
	}
	genlmsg_end(skb, msg_head);
	genlmsg_multicast(&procdeath_gnl_family, skb, 0, 0, GFP_KERNEL);
}

void send_genlmsg(s64 proc_ids)
{
	int rc;
	struct sk_buff *skb;
	void *msg_head;

	skb = genlmsg_new(sizeof(uint64_t), GFP_ATOMIC);
	if (skb == NULL) {
		PRINT_ERR("%s: %d failed to allocate skb for netlink\n",
			  __FILE__, __LINE__);
		return;
	}
	msg_head = genlmsg_put(skb, 0, 0, &procdeath_gnl_family, 0,
			       PROCDEATH_CMD_POLLOUT);
	if (!msg_head) {
		PRINT_ERR("%s: %d failed to put msg_head\n", __FILE__,
			  __LINE__);
		nlmsg_free(skb);
		return;
	}
	rc = nla_put_u64_64bit(skb, PROCDEATH_ATTR_ID, proc_ids,
			       PROCDEATH_ATTR_PAD);
	if (rc) {
		PRINT_ERR("%s: %d failed to put id: %d\n", __FILE__, __LINE__,
			  rc);
		nlmsg_free(skb);
		return;
	}
	genlmsg_end(skb, msg_head);
	genlmsg_multicast(&procdeath_gnl_family, skb, 0, 0, GFP_ATOMIC);
}
