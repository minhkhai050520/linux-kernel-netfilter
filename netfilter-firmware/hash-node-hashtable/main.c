#include <linux/module.h>
#include "myfw_hashtable.h"
#include "myfw_rcu_api.h"
#include "myfw_nf_hook.h"
#include "myfw_spinlock.h"

static struct nf_hook_ops my_nf_hook_ops;

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