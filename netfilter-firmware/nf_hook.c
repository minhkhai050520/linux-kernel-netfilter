#include <linux/init.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>

static struct nf_hook_ops my_nf_hook_ops;

static unsigned int ipv4_my_nf_hook_fn(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
    const struct iphdr *iph;
    struct udphdr *uh;
    const struct tcphdr *th;

    u32 len;

    if (!skb)
        return NF_ACCEPT;

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
        return NF_ACCEPT;

    iph = ip_hdr(skb);

    if (iph->ihl < 5 || iph->version != 4) {
        pr_info("[MYFW] Invalid IP header!\n");
        return NF_ACCEPT;
    }

    len = iph->ihl*4;

    switch (iph->protocol) {
        case IPPROTO_TCP:
            if (!pskb_may_pull(skb, len + sizeof(struct tcphdr)))
                return NF_ACCEPT;
            iph = ip_hdr(skb);
            th = tcp_hdr(skb);
            pr_info("[MYFW] TCP from %pI4:%u to %pI4:%u\n", &iph->saddr, ntohs(th->source), &iph->daddr, ntohs(th->dest));
            break;
        case IPPROTO_UDP:
            if (!pskb_may_pull(skb, len + sizeof(struct udphdr)))
                return NF_ACCEPT;
            iph = ip_hdr(skb);
            uh = udp_hdr(skb);
            pr_info("[MYFW] UDP from %pI4:%u to %pI4:%u\n", &iph->saddr, ntohs(uh->source), &iph->daddr, ntohs(uh->dest));
            break;
        case IPPROTO_ICMP:
            pr_info("[MYFW] ICMP from %pI4 to %pI4\n", &iph->saddr, &iph->daddr);
            break;
        default:
            pr_info("[MYFW] Other protocol: %u\n", iph->protocol);
            break;
    }

    return NF_ACCEPT;
}

static int __init myfw_init(void)
{
    my_nf_hook_ops.hook = ipv4_my_nf_hook_fn;
    my_nf_hook_ops.pf = NFPROTO_IPV4;
    my_nf_hook_ops.hooknum = NF_INET_PRE_ROUTING;
    my_nf_hook_ops.priority = NF_IP_PRI_FIRST;

    if (nf_register_net_hook(&init_net, &my_nf_hook_ops) < 0)
    {
        pr_err("[MYFW] Failed to register Netfilter hook!\n");
        return -1;
    }

    pr_info("[MYFW] Netfilter Hook registered successfully!\n");
    return 0;
}

static void __exit myfw_exit(void)
{
    /* Unregister hook khi rmmod de tranh crash Kernel */
    nf_unregister_net_hook(&init_net, &my_nf_hook_ops);
    pr_info("[MYFW] Netfilter Hook unregistered. Goodbye!\n");
}

module_init(myfw_init);
module_exit(myfw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Dev");
MODULE_DESCRIPTION("Netfilter Hook");