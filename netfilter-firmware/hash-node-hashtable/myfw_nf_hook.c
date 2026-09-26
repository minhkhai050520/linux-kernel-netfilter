#include "myfw_nf_hook.h"
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>

unsigned int ipv4_nf_hook_fn(void *priv,
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