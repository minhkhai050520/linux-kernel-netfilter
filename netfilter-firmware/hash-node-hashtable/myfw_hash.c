#include "myfw_hashtable.h"
#include <linux/module.h>

static struct nf_hook_ops my_nf_hook_ops;

/* 2^8 = 256 buckets */
static DEFINE_READ_MOSTLY_HASHTABLE(fw_rule_htable, 8);

// static DEFINE_SPINLOCK(myfw_write_lock);

static inline u32 get_rule_hash(u32 src_ip)
{
    return jhash_1word(src_ip, 0x9e3779b9);
}

static u8 lookup_rule_rcu(const struct fw_tuple *tuple)
{
    struct fw_rule_node *rule;
    u32 key = get_rule_hash(tuple->src_ip);
    u8 action = FW_ACTION_ACCEPT;

    rcu_read_lock();

    hash_for_each_possible_rcu(fw_rule_htable, rule, node, key) {
        if (rule->src_ip == tuple->src_ip &&
            rule->dst_ip == tuple->dst_ip &&
            rule->src_port == tuple->src_port &&
            rule->dst_port == tuple->dst_port &&
            rule->proto == tuple->proto) {

                action = rule->action;
                break;
        }
    }

    rcu_read_unlock();

    return action;
}

static unsigned int ipv4_nf_hook_fn(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
    const struct iphdr *iph;
    struct udphdr *uh;
    const struct tcphdr *th;
    struct fw_tuple tuple;
    u8 action;
    u32 len;

    if (!skb)
        return NF_ACCEPT;

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
        return NF_ACCEPT;

    iph = ip_hdr(skb);

    if (iph->ihl < 5 || iph->version != 4) {
        pr_err("[MYFW] Invalid IP header!\n");
        return NF_ACCEPT;
    }

    tuple.src_ip = iph->saddr;
    tuple.dst_ip = iph->daddr;
    tuple.proto = iph->protocol;
    tuple.src_port = 0;
    tuple.dst_port = 0;

    len = iph->ihl*4;

    switch (iph->protocol) {
        case IPPROTO_TCP:
            if (!pskb_may_pull(skb, len + sizeof(struct tcphdr)))
                return NF_ACCEPT;
            iph = ip_hdr(skb);
            th = tcp_hdr(skb);
            tuple.src_port = th->source;
            tuple.dst_port = th->dest;
            break;
        case IPPROTO_UDP:
            if (!pskb_may_pull(skb, len + sizeof(struct udphdr)))
                return NF_ACCEPT;
            iph = ip_hdr(skb);
            uh = udp_hdr(skb);
            tuple.src_port = uh->source;
            tuple.dst_port = uh->dest;
            break;
        case IPPROTO_ICMP:
            break;
        default:
            pr_info("[MYFW] Other protocol: %u\n", iph->protocol);
            break;
    }

    action = lookup_rule_rcu(&tuple);

    if (action == FW_ACTION_DROP) {
        pr_info("[MYFW] UDP from %pI4:%u to %pI4:%u\n", &tuple.src_ip, ntohs(tuple.src_port), &tuple.dst_ip, ntohs(tuple.dst_port));
        return action;
    }

    return action;
}

static int __init myfw_init(void)
{
    int ret;

    hash_init(fw_rule_htable);

    my_nf_hook_ops.hook = ipv4_nf_hook_fn;
    my_nf_hook_ops.pf = NFPROTO_IPV4;
    my_nf_hook_ops.hooknum = NF_INET_PRE_ROUTING;
    my_nf_hook_ops.priority = NF_IP_PRI_FIRST;

    ret = nf_register_net_hook(&init_net, &my_nf_hook_ops);
    if (ret < 0)
    {
        pr_err("[MYFW] Failed to register Netfilter hook!\n");
        return -1;
    }

    return 0;
}

static void __exit myfw_exit(void)
{
    nf_unregister_net_hook(&init_net, &my_nf_hook_ops);
    pr_info("[MYFW_DAY1] Module unloaded.\n");
}

module_init(myfw_init);
module_exit(myfw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Dev");
MODULE_DESCRIPTION("RCU Hashtable Initialization");