#ifndef _MYFW_STATS_H_
#define _MYFW_STATS_H_

#include "myfw_hashtable.h"

struct myfw_stats {
    u64_stats_t rx_packets;
    u64_stats_t rx_bytes;
    u64_stats_t dropped_packets;
    struct u64_stats_sync myfw_counter_sync;
};

struct nft_counter_tot {
    s64 rx_bytes;
    s64 rx_packets;
    s64 dropped_packets;
};

struct myfw_stats_counter_percpu {
    struct myfw_stats __percpu *fw_stats_ptr;
};

extern struct myfw_stats_counter_percpu myfw_priv;

void myfw_counter_do(struct myfw_stats_counter_percpu *myfw_priv,
                     const struct sk_buff *skb,
                     u8 action);

void myfw_counter_do_dump(struct myfw_stats_counter_percpu *myfw_priv,
                          struct nft_counter_tot *total,
                          bool reset);

#endif /* _MYFW_STATS_H_ */