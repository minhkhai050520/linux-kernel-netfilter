#include "myfw_hashtable.h"
#include "myfw_genl_ipc.h"
#include "myfw_rcu_api.h"
#include "myfw_stats.h"
#include <net/genetlink.h>

static struct genl_family myfw_genl_family;

static const struct nla_policy myfw_genl_policy[MYFW_ATTR_MAX + 1] = {
    [MYFW_ATTR_SRC_IP] = { .type = NLA_U32 },
    [MYFW_ATTR_DST_IP] = { .type = NLA_U32 },
    [MYFW_ATTR_SRC_PORT] = { .type = NLA_U16 },
    [MYFW_ATTR_DST_PORT] = { .type = NLA_U16 },
    [MYFW_ATTR_PROTO] = { .type = NLA_U8 },
    [MYFW_ATTR_ACTION] = { .type = NLA_U8 },
};

static int myfw_genl_get_stats(struct sk_buff *skb,
                               struct nft_counter_tot* total,
                               u32 portid, u32 seq,
                               struct netlink_callback *cb, int flags)
{
    void* hdr;
    struct nft_counter_tot new_stat = { };
    int ret;

    hdr = genlmsg_put(skb, portid, seq, &myfw_genl_family, flags, MYFW_CMD_GET_STATS);
    if (!hdr)
        return -EMSGSIZE;

    new_stat.rx_bytes = total->rx_bytes;
    new_stat.rx_packets = total->rx_packets;
    new_stat.dropped_packets = total->dropped_packets;

    ret = nla_put_s64(skb, MYFW_ATTR_RX_BYTES, new_stat.rx_bytes, MYFW_ATTR_PAD);
    if (ret)
        goto out_err;

    ret = nla_put_s64(skb, MYFW_ATTR_RX_PACKETS, new_stat.rx_packets, MYFW_ATTR_PAD);
    if (ret)
        goto out_err;

    ret = nla_put_s64(skb, MYFW_ATTR_DROPPED_PACKETS, new_stat.dropped_packets, MYFW_ATTR_PAD);
    if (ret)
        goto out_err;

    genlmsg_end(skb, hdr);

    return 0;
out_err:
    genlmsg_cancel(skb, hdr);
    return ret;
}

static int myfw_genl_do_get_stats(struct sk_buff *msg, struct genl_info *info)
{
    struct nft_counter_tot total;
    struct sk_buff *skb;
    int ret;

    skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!skb)
        return -ENOMEM;

    myfw_counter_do_dump(&myfw_priv, &total, false);

    ret = myfw_genl_get_stats(skb, &total, info->snd_portid, info->snd_seq, NULL, 0);
    if (ret)
        goto err;

    ret = genlmsg_reply(skb, info);

    return ret;
err:
    nlmsg_free(skb);
    return ret;
}

static int myfw_genl_add_rule(struct sk_buff *skb_2, struct genl_info *info)
{
    struct fw_tuple add_rule;
    memset(&add_rule, 0, sizeof(struct fw_tuple));
    int ret;

    if (!info->attrs[MYFW_ATTR_SRC_IP] ||
        !info->attrs[MYFW_ATTR_DST_IP] ||
        !info->attrs[MYFW_ATTR_SRC_PORT] ||
        !info->attrs[MYFW_ATTR_DST_PORT] ||
        !info->attrs[MYFW_ATTR_PROTO] ||
        !info->attrs[MYFW_ATTR_ACTION])
            goto out;
    
    add_rule.src_ip = nla_get_u32(info->attrs[MYFW_ATTR_SRC_IP]);

    if (add_rule.src_ip == 0)
        goto out;

    add_rule.dst_ip = nla_get_u32(info->attrs[MYFW_ATTR_DST_IP]);
    add_rule.src_port = nla_get_u16(info->attrs[MYFW_ATTR_SRC_PORT]);
    add_rule.dst_port = nla_get_u16(info->attrs[MYFW_ATTR_DST_PORT]);
    add_rule.proto = nla_get_u8(info->attrs[MYFW_ATTR_PROTO]);
    add_rule.action = nla_get_u8(info->attrs[MYFW_ATTR_ACTION]);

    ret = add_rule_rcu(&add_rule);
    
    if (ret)
        goto err;

    return 0;
err:
    pr_err("[MYFW] error occurred in %s\n", __func__);
out:
    return -EINVAL;
}

static int myfw_genl_del_rule(struct sk_buff *skb_2, struct genl_info *info)
{
    struct fw_tuple del_rule;
    memset(&del_rule, 0, sizeof(struct fw_tuple));
    int ret;

    if (!info->attrs[MYFW_ATTR_SRC_IP] ||
        !info->attrs[MYFW_ATTR_DST_IP] ||
        !info->attrs[MYFW_ATTR_SRC_PORT] ||
        !info->attrs[MYFW_ATTR_DST_PORT] ||
        !info->attrs[MYFW_ATTR_PROTO])
            goto out;
    
    del_rule.src_ip = nla_get_u32(info->attrs[MYFW_ATTR_SRC_IP]);

    if (del_rule.src_ip == 0)
        goto out;

    del_rule.dst_ip = nla_get_u32(info->attrs[MYFW_ATTR_DST_IP]);
    del_rule.src_port = nla_get_u16(info->attrs[MYFW_ATTR_SRC_PORT]);
    del_rule.dst_port = nla_get_u16(info->attrs[MYFW_ATTR_DST_PORT]);
    del_rule.proto = nla_get_u8(info->attrs[MYFW_ATTR_PROTO]);

    ret = del_rule_rcu(&del_rule);
    
    if (ret)
        goto err;

    return 0;
err:
    pr_err("[MYFW] error occurred in %s\n", __func__);
out:
    return -EINVAL;
}

static int myfw_genl_reset_stats(struct sk_buff *skb_2, struct genl_info *info)
{
    struct nft_counter_tot dummy;
    myfw_counter_do_dump(&myfw_priv, &dummy, true);
    return 0;
}

static int myfw_genl_put_rule(struct sk_buff *skb,
                              const struct fw_rule_node *rule,
                              u32 portid, u32 seq,
                              struct netlink_callback *cb, int flags)
{
    void *hdr;
    int ret;

    hdr = genlmsg_put(skb, portid, seq, &myfw_genl_family,
                      flags, MYFW_CMD_GET_RULES);
    if (!hdr)
        return -EMSGSIZE;

    ret = nla_put_u32(skb, MYFW_ATTR_SRC_IP, rule->src_ip);
    if (ret)
        goto err;

    ret = nla_put_u32(skb, MYFW_ATTR_DST_IP, rule->dst_ip);
    if (ret)
        goto err;

    ret = nla_put_u16(skb, MYFW_ATTR_SRC_PORT, rule->src_port);
    if (ret)
        goto err;

    ret = nla_put_u16(skb, MYFW_ATTR_DST_PORT, rule->dst_port);
    if (ret)
        goto err;

    ret = nla_put_u8(skb, MYFW_ATTR_PROTO, rule->proto);
    if (ret)
        goto err;

    ret = nla_put_u8(skb, MYFW_ATTR_ACTION, rule->action);
    if (ret)
        goto err;

    genlmsg_end(skb, hdr);
    return 0;

err:
    genlmsg_cancel(skb, hdr);
    return ret;
}

static int myfw_genl_dump_rules(struct sk_buff *skb, struct netlink_callback *cb)
{
    struct fw_rule_node *rule;
    int last_idx = 0;
    int bkt;
    int ret = 0;

    rcu_read_lock();

    hash_for_each_rcu(fw_rule_htable, bkt, rule, node) {
        
        if (last_idx < cb->args[0]) {
            last_idx++;
            continue;
        }

        ret = myfw_genl_put_rule(skb, rule, NETLINK_CB(cb->skb).portid,
                                 cb->nlh->nlmsg_seq, cb, NLM_F_MULTI);
        if (ret)
            break;
        
        last_idx++;
    }

    cb->args[0] = last_idx;
    rcu_read_unlock();
    return ret ?: skb->len;
}

static const struct genl_small_ops myfw_ops[] = {
    {
        .cmd = MYFW_CMD_ADD_RULE,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = myfw_genl_add_rule,
        .flags = GENL_UNS_ADMIN_PERM,
    },
    {
        .cmd = MYFW_CMD_DEL_RULE,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = myfw_genl_del_rule,
        .flags = GENL_UNS_ADMIN_PERM,
    },
    {
        .cmd = MYFW_CMD_GET_RULES,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .dumpit = myfw_genl_dump_rules,
    },
    {
        .cmd = MYFW_CMD_GET_STATS,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = myfw_genl_do_get_stats,
        .flags = GENL_UNS_ADMIN_PERM,
    },
    {
        .cmd = MYFW_CMD_RESET_STATS,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = myfw_genl_reset_stats,
    },
};

static struct genl_family myfw_genl_family __ro_after_init = {
    .name = "MYFW_GENL",
    .version = 1,
    .maxattr = MYFW_ATTR_MAX,
    .policy = myfw_genl_policy,
    .netnsok = true,
    .module = THIS_MODULE,
    .small_ops = myfw_ops,
    .n_small_ops = ARRAY_SIZE(myfw_ops),
};

int myfw_init_netlink(void)
{
    int ret;
    pr_info("myfw: initializing netlink\n");

    ret = genl_register_family(&myfw_genl_family);
    if (ret)
        goto failure;

    return 0;

failure:
    pr_err("myfw: failed to register netlink family %s\n", __func__);
    return -EINVAL;
}

void myfw_unregister_netlink(void)
{
    genl_unregister_family(&myfw_genl_family);
}