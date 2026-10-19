#include "myfw_hashtable.h"
#include "myfw_rcu_api.h"
#include "myfw_nf_hook.h"
#include "myfw_spinlock.h"
#include "myfw_procfs.h"
#include "myfw_stats.h"
#include <linux/inet.h>
#include <linux/module.h>
#include <linux/u64_stats_sync.h>

static struct nf_hook_ops my_nf_hook_ops;

static int __init myfw_init(void)
{
    int ret;
    struct fw_rule_node tmp_rule;

    hash_init(fw_rule_htable);

    memset(&tmp_rule, 0, sizeof(struct fw_rule_node));

    tmp_rule.src_ip = in_aton("192.168.1.102");
    tmp_rule.dst_ip = 0;
    tmp_rule.src_port = 0;
    tmp_rule.dst_port = htons(3000);
    tmp_rule.proto = IPPROTO_TCP;
    tmp_rule.action = FW_ACTION_DROP;
    add_rule_rcu(&tmp_rule);

    myfw_priv.fw_stats_ptr = alloc_percpu(struct myfw_stats);

    if (!myfw_priv.fw_stats_ptr)
        return -ENOMEM;

    ret = myfw_init_procfs();
    if (ret < 0) {
        free_percpu(myfw_priv.fw_stats_ptr);
        return ret;
    }

    my_nf_hook_ops.hook = ipv4_nf_hook_fn;
    my_nf_hook_ops.pf = NFPROTO_IPV4;
    my_nf_hook_ops.hooknum = NF_INET_PRE_ROUTING;
    my_nf_hook_ops.priority = NF_IP_PRI_FIRST;

    ret = nf_register_net_hook(&init_net, &my_nf_hook_ops);
    if (ret < 0)
    {
        pr_err("[MYFW] Failed to register Netfilter hook!\n");
        clean_hashtable_rcu();
        return ret;
    }

    return 0;
}

static void __exit myfw_exit(void)
{
    nf_unregister_net_hook(&init_net, &my_nf_hook_ops);

    clean_hashtable_rcu();

    rcu_barrier();

    free_percpu(myfw_priv.fw_stats_ptr);
    myfw_cleanup_procfs();

    pr_info("[MYFW_DAY1] Module unloaded.\n");
}

module_init(myfw_init);
module_exit(myfw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Dev");
MODULE_DESCRIPTION("RCU Hashtable Initialization");