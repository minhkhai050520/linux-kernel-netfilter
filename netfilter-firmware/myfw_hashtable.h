#ifndef _MYFW_HASHTABLE_H
#define _MYFW_HASHTABLE_H

#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/netfilter_ipv4.h>


#define FW_RULE_HTABLE_BITS 8

enum fw_action {
    FW_ACTION_DROP = 0,
    FW_ACTION_ACCEPT
};

struct fw_tuple {
    u32 src_ip;
    u32 dst_ip;
    u16 src_port;
    u16 dst_port;
    u8  proto;
};

struct fw_rule_node {
    u32 src_ip;
    u32 dst_ip;
    u16 src_port;
    u16 dst_port;
    u8  proto;

    enum fw_action action;

    struct hlist_node node;
    struct rcu_head rcu;
};

u32 get_rule_hash(u32 src_ip);
extern struct hlist_head fw_rule_htable[1 << FW_RULE_HTABLE_BITS] __read_mostly;

#endif