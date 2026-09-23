#ifndef _MYFW_HASHTABLE_H
#define _MYFW_HASHTABLE_H

#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/spinlock_types.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>

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

#endif