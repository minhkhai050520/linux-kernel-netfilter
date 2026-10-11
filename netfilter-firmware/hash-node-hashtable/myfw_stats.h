#ifndef _MYFW_STATS_H_
#define _MYFW_STATS_H_

#include <linux/types.h>

struct myfw_stats {
    u64 rx_packets;
    u64 rx_bytes;
    u64 dropped_packets;
};

extern void* fw_stats_ptr;

#endif