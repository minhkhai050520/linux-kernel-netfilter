#include "myfw_hashtable.h"
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Dev");
MODULE_DESCRIPTION("RCU Hashtable Initialization");

/* 2^8 = 256 buckets */
static DEFINE_READ_MOSTLY_HASHTABLE(myfw_rule_htable, 8);

// static DEFINE_SPINLOCK(myfw_write_lock);

static inline u32 get_rule_hash(u32 src_ip)
{
    return jhash_1word(src_ip, 0x9e3779b9);
}

static int __init myfw_init(void)
{
    hash_init(myfw_rule_htable);
    return 0;
}

static void __exit myfw_exit(void)
{
    pr_info("[MYFW_DAY1] Module unloaded.\n");
}

module_init(myfw_init);
module_exit(myfw_exit);