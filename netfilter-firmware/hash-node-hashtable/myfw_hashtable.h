#ifndef _MYFW_HASHTABLE_H
#define _MYFW_HASHTABLE_H

#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/spinlock_types.h>

struct fw_rule_node {
    u32 src_ip;
    u32 dst_ip;
    u16 src_port;
    u16 dst_port;
    u8  proto;
    u8  action;

    struct hlist_node node;
    struct rcu_head rcu;
};

#endif