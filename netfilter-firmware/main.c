#include "myfw_hashtable.h"
#include "myfw_rcu_api.h"
#include "myfw_nf_hook.h"
#include "myfw_spinlock.h"
#include "myfw_procfs.h"
#include "myfw_stats.h"
#include <linux/inet.h>
#include <linux/module.h>
#include <linux/u64_stats_sync.h>
#include "myfw_genl_ipc.h"

static struct nf_hook_ops my_nf_hook_ops;

static int __init myfw_init(void)
{
    int ret;

    /* Initialize the RCU hashtable */
    hash_init(fw_rule_htable);

    /* Initialize the per-CPU statistics */
    myfw_priv.fw_stats_ptr = alloc_percpu(struct myfw_stats);

    if (!myfw_priv.fw_stats_ptr)
    {
        ret = -ENOMEM;
        goto failure;
    }

    ret = myfw_init_netlink();

    if (ret)
        goto failure;

    /* Initialize the procfs operations */
    ret = myfw_init_procfs();
    if (ret)
        goto failure;

    /* Register the Netfilter hook */
    my_nf_hook_ops.hook = ipv4_nf_hook_fn;
    my_nf_hook_ops.pf = NFPROTO_IPV4;
    my_nf_hook_ops.hooknum = NF_INET_PRE_ROUTING;
    my_nf_hook_ops.priority = NF_IP_PRI_FIRST;

    ret = nf_register_net_hook(&init_net, &my_nf_hook_ops);
    if (ret)
        goto failure;

    return 0;

failure:
    pr_err("[MYFW] Failed to initialize netfilter hook %s\n", __func__);
    clean_hashtable_rcu();
    rcu_barrier();
    if (myfw_priv.fw_stats_ptr)
        free_percpu(myfw_priv.fw_stats_ptr);
    myfw_unregister_netlink();
    myfw_cleanup_procfs();
    return ret;
}

static void __exit myfw_exit(void)
{
    pr_info("[myfw] Module unloaded.\n");

    /* Unregister the Netfilter hook */
    nf_unregister_net_hook(&init_net, &my_nf_hook_ops);

    /* Delete procfs operations */
    myfw_cleanup_procfs();

    /* Unregister the generic netlink */
    myfw_unregister_netlink();

     /* Free per CPU variables */
    free_percpu(myfw_priv.fw_stats_ptr);

    /* Clean hash table RCU*/
    clean_hashtable_rcu();
    rcu_barrier();
}

module_init(myfw_init);
module_exit(myfw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Dev");
MODULE_DESCRIPTION("Add a simple firewall to the linux kernel");