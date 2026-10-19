#include "myfw_stats.h"
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/bottom_half.h>

struct myfw_stats_counter_percpu myfw_priv;

void myfw_counter_do(struct myfw_stats_counter_percpu *myfw_priv,
                     const struct sk_buff *skb,
                     u8 action)
{
    struct myfw_stats *this_cpu;

    local_bh_disable();
    this_cpu = this_cpu_ptr(myfw_priv->fw_stats_ptr);

    u64_stats_update_begin(&myfw_priv->fw_stats_ptr->myfw_counter_sync);

    if (action == FW_ACTION_ACCEPT) {
        u64_stats_inc(&this_cpu->rx_packets);
        u64_stats_add(&this_cpu->rx_bytes, skb->len);
    }
    else
        u64_stats_inc(&this_cpu->dropped_packets);

    u64_stats_update_end(&myfw_priv->fw_stats_ptr->myfw_counter_sync);
    local_bh_enable();
}

static void myfw_counter_fetch(struct myfw_stats_counter_percpu *myfw_priv,
                        struct nft_counter_tot *total)
{
    struct myfw_stats *this_cpu;
    u64 bytes, packets, dropped_packets;
    unsigned int start;
    int cpu;

    memset(total, 0, sizeof(*total));
    for_each_possible_cpu(cpu) {
        struct u64_stats_sync *myfw_sync = per_cpu_ptr(&myfw_priv->fw_stats_ptr->myfw_counter_sync, cpu);

        this_cpu = per_cpu_ptr(myfw_priv->fw_stats_ptr, cpu);

        do {
            start = u64_stats_fetch_begin(myfw_sync);
            bytes = u64_stats_read(&this_cpu->rx_bytes);
            packets = u64_stats_read(&this_cpu->rx_packets);
            dropped_packets = u64_stats_read(&this_cpu->dropped_packets);
        } while (u64_stats_fetch_retry(myfw_sync, start));

        total->rx_bytes += bytes;
        total->rx_packets += packets;
        total->dropped_packets += dropped_packets;
    }
}

static void myfw_counter_reset(struct myfw_stats_counter_percpu *myfw_priv,
                     const struct nft_counter_tot *total)
{
    struct myfw_stats *this_cpu;

    local_bh_disable();
    this_cpu = this_cpu_ptr(myfw_priv->fw_stats_ptr);

    u64_stats_update_begin(&myfw_priv->fw_stats_ptr->myfw_counter_sync);

    u64_stats_add(&this_cpu->rx_packets, -total->rx_packets);
    u64_stats_add(&this_cpu->rx_bytes, -total->rx_bytes);
    u64_stats_add(&this_cpu->dropped_packets, -total->dropped_packets);

    u64_stats_update_end(&myfw_priv->fw_stats_ptr->myfw_counter_sync);
    local_bh_enable();
}

void myfw_counter_do_dump(struct myfw_stats_counter_percpu *myfw_priv,
                          struct nft_counter_tot *total,
                          bool reset)
{
    myfw_counter_fetch(myfw_priv, total);

    if (reset)
        myfw_counter_reset(myfw_priv, total);
}