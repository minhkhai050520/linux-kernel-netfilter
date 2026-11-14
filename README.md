# Linux Kernel Netfilter Firewall

A Linux kernel module for packet inspection and filtering at the L3 layer (network layer). The project is designed around a custom firewall/monitoring extension that extracts packet metadata, matches it against a rule table, updates per-CPU counters, exposes statistics through procfs, and provides a Generic Netlink control plane for kernel-user communication.

This repository implements the firewall kernel module for a Raspberry Pi 4 / ARM64 Linux environment and is aligned with the design goals as follows: high-performance pre-routing inspection, RCU-based rule lookup, per-CPU statistics, and a Generic Netlink interface for telemetry and rule management.

---

## 1. Project Overview

The module registers a Netfilter hook on `NF_INET_PRE_ROUTING` and inspects IPv4 packets before they traverse subsequent Layer 3 hooks. It extracts the essential flow tuple:

- source IP
- destination IP
- source port
- destination port
- protocol

If a matching rule is found in the internal rule hashtable, the module can either accept or drop the packet. In addition, packet and byte counters are updated per CPU to reduce contention on multi-core ARM systems such as the Raspberry Pi 4.

The implementation is intentionally structured to resemble a minimal but realistic raw, mangle or NAT table in the PREROUTING hook, with separate components for:

- packet filtering logic
- rule storage and lookup
- statistics accounting
- procfs reporting
- Generic Netlink IPC

---

## 2. Objectives

The project addresses the following goals:

- Inspect network packets in kernel space.
- Match packet tuples against a dynamic rule set.
- Improves packet processing speed and prevents firewall interruptions during rule updates using RCU.
- Maintain per-CPU counters for statistics collection.
- Expose kernel telemetry via procfs and Generic Netlink.

---

## 3. Technical Architecture

### 3.1 Architecture

```text
[USER SPACE]
               +--------------------------------+
               |                                |
               |            myfw-ctl            |
               |   (Add/Remove Rules & Stats)   |
               +----------------+---------------+
                                |
                                | Generic Netlink (Command)
                                v
[KERNEL SPACE]
  +----------------------------------------------------------+

  |              Generic Netlink Family (genl)               |
  +------------+--------------------------------+------------+
               |                                |
               |                                |
               v                                v
  +-------------------------+      +-------------------------+
  |                         |      |                         |
  |    Per-CPU Collector    |      |   RCU Blocklist Hash    |
  |  (alloc_percpu counters)|      | (<linux/hashtable.h>)   |
  +------------+------------+      +------------+------------+
               |                                |
               |                                |
               +----------------+---------------+
                                |
                                v
               +--------------------------------+
               |                                |
               |     Netfilter Hook Handler     |
               |     (NF_INET_PRE_ROUTING)      |
               +----------------+---------------+
                                ^
                                | struct sk_buff
                       +--------+--------+

                       |   RPi 4 NIC     |
                       +-----------------+

```

### 3.2 Packet Parsing

The hook validates the skb and IP header, then parses TCP or UDP headers when applicable. It extracts tuples such as:

- `src_ip`
- `dst_ip`
- `src_port`
- `dst_port`
- `proto`

### 3.3 Rule Engine

The rule table is stored in a RCU hash table using Linux kernel hashtable APIs:

- table type: `struct hlist_head`
- bit size: `FW_RULE_HTABLE_BITS = 8`
- bucket count: `1 << 8` = 256

The RCU api is implemented in `myfw_rcu_api.c`:

- `lookup_rule_rcu()`
- `add_rule_rcu()`
- `del_rule_rcu()`
- `clean_hashtable_rcu()`

This pattern is intended to minimize lock contention, which is important on multi-core ARM systems.

### 3.4 Statistics Collection

Per-CPU counters are allocated using `alloc_percpu()`:

- `struct myfw_stats`
- `rx_packets`: The total count of packets hitting the myfw hook and accepted to pass through.
- `rx_bytes`: The total count of bytes hitting the myfw hook and accepted to pass through.
- `dropped_packets`: The total count of packets hitting the myfw hook and dropped.

The implementation uses `u64_stats_t` and `u64_stats_sync` helpers for safe reads and writes across CPUs.

### 3.5 Procfs Interface

A proc entry is created as:

- `/proc/myfw_stats`

The proc file has two roles:

- read: dumps aggregated values
```text
cat /proc/myfw_stats
```
- write: resets counters
```text
echo 0 > /proc/myfw_stats
```

### 3.6 Generic Netlink IPC

The kernel module registers a Generic Netlink family named:

- `MYFW_GENL`

Supported commands include:

- `MYFW_CMD_ADD_RULE`
- `MYFW_CMD_DEL_RULE`
- `MYFW_CMD_GET_RULES`
- `MYFW_CMD_GET_STATS`
- `MYFW_CMD_RESET_STATS`

These commands are defined in `myfw_genl_ipc.h` and implemented in `myfw_genl_ipc.c`.

### 3.7 Generic Netlink CLI (`myfw_ctl`)

The CLI interface source code is located in the [`package/myfw-ctl`](https://github.com) directory.

#### Key Features
* **Rule Management:** Add, delete, and list active firewall rules.
* **Traffic Monitoring:** View and reset firewall packet/byte statistics.

#### API Reference

| Function | Description |
| :--- | :--- |
| `myfw_cmd_add()` | Builds a firewall rule from parameters (source/destination IP, ports, protocol, action) and sends it to the kernel. |
| `myfw_cmd_del()` | Builds a rule match based on IPs, ports, and protocol, then requests the kernel to remove the existing rule. |
| `myfw_cmd_show()` | Sends a dump request to retrieve and print all currently configured firewall rules. |
| `myfw_cmd_stat_show()` | Sends a stats request to retrieve and display packet/byte counters and dropped packet counts. |
| `myfw_cmd_stat_reset()` | Sends a reset request to clear all firewall statistics counters in the kernel. |

#### Usage Examples

* **Add a firewall rule:**
  ```bash
  myfw_ctl rule add --src-ip <ip> --dst-ip <ip> --src-port <port> --dst-port <port> --proto <tcp|udp|icmp> --action <drop|accept>
  ```

* **Delete a firewall rule:**
  ```bash
  myfw_ctl rule del --src-ip <ip> --dst-ip <ip> --src-port <port> --dst-port <port> --proto <tcp|udp|icmp>
  ```

* **List active rules:**
  ```bash
  myfw_ctl rule show
  ```

* **Show statistics:**
  ```bash
  myfw_ctl stat show
  ```

* **Reset statistics:**
  ```bash
  myfw_ctl stat reset
  ```

---

## 4. Repository Structure

```text
linux-kernel-netfilter/
├── Dockerfile
├── build-container.sh
├── README.md
├── linux-custom/                  # custom Linux kernel tree
├── toolchains/                    # toolchain files
└── netfilter-firmware/
    ├── Makefile
    ├── main.c
    ├── myfw_hashtable.h
    ├── myfw_hashtable.c
    ├── myfw_nf_hook.h
    ├── myfw_nf_hook.c
    ├── myfw_rcu_api.h
    ├── myfw_rcu_api.c
    ├── myfw_spinlock.h
    ├── myfw_spinlock.c
    ├── myfw_stats.h
    ├── myfw_stats.c
    ├── myfw_procfs.h
    ├── myfw_procfs.c
    ├── myfw_genl_ipc.h
    └── myfw_genl_ipc.c
```

---

## 5. Build Instructions

### 5.1 Prerequisites
#### Adding Linux kernel source tree for ARM64
- Build this project https://github.com/minhkhai050520/rpi4-router-project, and copy /output/build/linux-custom to linux-kernel-netfilter repository
- Run build-container.sh to create the build env and clone the toolchain.
```bash
cd linux-kernel-netfilter
./build-container.sh
```
### 5.2 Build the module

From the project root:

```bash
cd netfilter-firmware
make
```

The module is built against the kernel tree defined by:

```make
KDIR ?= $(abspath $(CURDIR)/../linux-custom)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-
```

### 5.3 Deploy to target

On a Raspberry Pi or ARM64 Linux target, copy the generated module file and load it:

```bash
insmod myfw.ko
```

To unload:

```bash
rmmod myfw
```

To check:

```bash
lsmod
```
---

## 6. Benchmarking and Validation

This section documents the measured test results collected during runtime validation of the kernel module. The objective was to evaluate throughput stability, resilience under SYN flood conditions, and memory behavior.
```bash
┌─────────────────────────────────────────────────────────┐
│                  TRAFFIC GENERATOR PC                   │
│  • iperf3 client (TCP/UDP Throughput Test)              │
│  • hping3 (SYN Flood / Stress Test)                     │
└────────────────────────────┬────────────────────────────┘
                             │
                             │ 1 Gbps Ethernet Link (192.168.10.0/24)
                             ▼
┌─────────────────────────────────────────────────────────┐
│         RASPBERRY PI 4 / ARM64 (DEVICE UNDER TEST)      │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │        RPi 4 Physical NIC (eth0: 192.168.10.1)    │  │
│  └─────────────────────────┬─────────────────────────┘  │
│                            │ struct sk_buff             │
│                            ▼                            │
│  [ KERNEL SPACE ]                                       │
│  ┌───────────────────────────────────────────────────┐  │
│  │              Netfilter Hook Handler               │  │
│  │            (NF_INET_PRE_ROUTING hook)             │  │
│  └──────────────┬─────────────────────┬──────────────┘  │
│                 │ Rule Lookup         │ Update Stats    │
│                 ▼                     ▼                 │
│  ┌─────────────────────────┐ ┌───────────────────────┐  │
│  │ RCU Blocklist Hashtable │ │   Per-CPU Collector   │  │
│  │   (Fast Rule Matching)  │ │   (Packet/Byte Stats) │  │
│  └──────────────┬──────────┘ └───────────┬───────────┘  │
│                 │                        │              │
│  ═══════════════╪════════════════════════╪════════════  │
│                 │    Generic Netlink     │              │
│                 ▼                        ▼              │
│  [ USER SPACE ]                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │            myfw-ctl / myfw-loop-test.sh           │  │
│  │        (Rule Management & Stat Monitoring)        │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘

```
### 6.1 Throughput Impact

Test target: Raspberry Pi / ARM64 environment with a 1 Gbps link.

Tooling: `iperf3`

#### 6.1.1 Before module insertion

##### TCP

Command:

```bash
iperf3 -c 192.168.10.1 -B 192.168.10.72 -p 4000
```

Observed output:

```text
[  5]   0.00-10.01  sec  1.10 GBytes   943 Mbits/sec    0            sender
[  5]   0.00-10.01  sec  1.10 GBytes   941 Mbits/sec                  receiver
```

Result: average throughput was approximately `941-943 Mbits/sec` with zero retransmissions.

##### UDP

Command:

```bash
iperf3 -c 192.168.10.1 -B 192.168.10.72 -p 4000 -u
```

Observed output:

```text
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.000 ms  0/906 (0%)  sender
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.041 ms  0/906 (0%)  receiver
```

Result: UDP throughput remained stable at roughly `1.05 Mbits/sec` with zero packet loss and negligible jitter.

#### 6.1.2 After module insertion

Module listing:

```bash
# lsmod
Module                  Size  Used by
myfw                   24576  0
...
```

##### TCP

Command:

```bash
iperf3 -c 192.168.10.1 -B 192.168.10.72 -p 4000
```

Observed output:

```text
[  5]   0.00-10.00  sec  1.10 GBytes   943 Mbits/sec    0            sender
[  5]   0.00-10.00  sec  1.10 GBytes   941 Mbits/sec                  receiver
```

Result: throughput remained effectively unchanged compared with the pre-load baseline.

##### UDP

Command:

```bash
iperf3 -c 192.168.10.1 -B 192.168.10.72 -p 4000 -u
```

Observed output:

```text
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.000 ms  0/906 (0%)  sender
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.024 ms  0/906 (0%)  receiver
```

Result: UDP performance remained stable, with no measurable packet loss or jitter degradation.

#### 6.1.3 After module insertion and continuous rule add/remove loop

The module was exercised with the script `myfw-loop-test.sh`, which continuously inserted and removed rules in a repeated 20-second cycle.

##### TCP

Observed output:

```text
[  5]   0.00-10.00  sec  1.10 GBytes   943 Mbits/sec    0            sender
[  5]   0.00-10.01  sec  1.10 GBytes   940 Mbits/sec                  receiver
```

Result: The measured throughput stayed nearly the same as when no rules were being changed. This shows that rapidly adding and deleting rules does not significantly slow down network performance

##### UDP

Observed output:

```text
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.000 ms  0/906 (0%)  sender
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.385 ms  0/906 (0%)  receiver
```

Result: UDP performance remained stable with zero packet loss and minimal jitter increase.

Conclusion for throughput impact:

- The module did not significantly reduce network throughput under the tested conditions.
- No major throughput degradation was observed while the module was active, even under repeated rule updates.

---

### 6.2 SYN Flood / Stress Test

#### 6.2.1 Rule setup

Observed configuration:

```bash
# myfw-ctl -fw rule show
Rule -> Src:    192.168.10.5:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: DROP
Rule -> Src:    192.168.10.4:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: ACCEPT
```

#### 6.2.2 SYN flood with hping3

Command:

```bash
sudo hping3 -a 192.168.10.4 -S -p 4000 -c 10000000 -i u1 192.168.10.1
```

Observed output:

```text
10000000 packets transmitted, 0 packets received, 100% packet loss
round-trip min/avg/max = 0.0/0.0/0.0 ms
```

CPU snapshot:

```text
Average:     all    0.00    0.00    0.04    0.00    0.00   23.93    0.00    0.00    0.00   76.02
Average:       0    0.00    0.00    0.03    0.00    0.00   95.57    0.00    0.00    0.00    4.40
Average:       1    0.00    0.00    0.00    0.01    0.00    0.00    0.00    0.00    0.00   99.99
```

Kernel statistics:

```bash
# myfw-ctl -fw stat show
======================================
         MYFW HOOK STATISTICS     
======================================
 Rx Packets   : 9999971
 Rx Bytes     : 399998840
 Dropped Pkts : 0
======================================
```

Interpretation:

- The firewall module maintained stability without crashing the kernel or stalling the system.
- The counter output confirms the module processed a very large number of packets without drops (9999971 packets) in the custom stat view.

#### 6.2.3 Stress test with iperf3

The system was reset and then evaluated under concurrent traffic with an additional accept rule for the iperf source:

```bash
# myfw-ctl -fw stat reset
# myfw-ctl -fw rule show
Rule -> Src:    192.168.10.5:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: DROP
Rule -> Src:    192.168.10.4:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: ACCEPT
Rule -> Src:   192.168.10.72:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: ACCEPT
```

Observed output during 30-second iperf3 stress test:

```text
[SUM]   0.00-30.00  sec  3.29 GBytes   941 Mbits/sec                  receiver
[SUM]   0.00-30.00  sec  3.30 GBytes   944 Mbits/sec    0             sender
```

CPU snapshot:

```text
Average:     all    0.38    0.00   14.69    0.01    0.00   24.79    0.00    0.00    0.00   60.12
Average:       0    0.00    0.00    0.15    0.00    0.00   94.62    0.00    0.00    0.00    5.23
Average:       1    0.61    0.00   16.65    0.04    0.00    0.00    0.00    0.00    0.00   82.70
Average:       2    0.91    0.00   40.60    0.00    0.00    0.00    0.00    0.00    0.00   58.49
Average:       3    0.08    0.00    4.26    0.00    0.00    0.00    0.00    0.00    0.00   95.66
```

Statistics after the stress run:

```bash
# myfw-ctl -fw stat show
======================================
         MYFW HOOK STATISTICS     
======================================
 Rx Packets   : 1186173
 Rx Bytes     : 3594118680
 Dropped Pkts : 0
======================================
```

Observations:

- The module remained stable under sustained traffic.
- Throughput remained near the expected `941-944 Mbits/sec` band.
- The stress test produced no dropped packets under these accept-path conditions.

---

### 6.3 Memory Safety Validation

Memory validation was performed with `kmemleak` and `/proc/meminfo` after runtime testing.

Command:

```bash
# echo scan > /sys/kernel/debug/kmemleak
# cat /sys/kernel/debug/kmemleak
```

Observed output:

```text
#
#
```

This indicates that no obvious leak was reported by `kmemleak` during the inspection window.

System memory summary:

```bash
# cat /proc/meminfo
MemTotal:        3848368 kB
MemFree:         3768432 kB
MemAvailable:    3757168 kB
...
Slab:              33748 kB
SReclaimable:       5436 kB
SUnreclaim:        28312 kB
KernelStack:        1920 kB
PageTables:          484 kB
...
```

Interpretation:

- No immediate memory leak symptoms were observed during the validation period.
- Memory usage remained stable and did not show a sustained upward trend after repeated traffic.
- The module appears to be in a stable state for the tested workload.

---

### 6.4 Result Summary

The measured validation results indicate that the module is stable enough for a prototype/educational Linux firewall implementation under the tested conditions:

- Throughput remained stable before and after module insertion.
- No material degradation was observed during repeated rule add/remove operations.
- The system stayed responsive under SYN flood pressure.
- No obvious kernel memory leak was detected in the validation window.

These results are suitable for a prototype-level performance baseline, but they should still be expanded with broader benchmarking on more diverse hardware and traffic patterns.

---

## 7. Future Work

Planned enhancements for a more complete production-grade version include:

- adding IPv6 support
- extending Generic Netlink notifications for asynchronous alerts
- Improving a robust user-space
- extending benchmark automation and regression test coverage
---

## 8. Summary

This kernel module is a compact, educational, and practical network filtering prototype. It brings together the essential components of a Linux kernel firewall extension:

- packet capture at the Netfilter pre-routing layer
- dynamic rules stored in an RCU hash table
- per-CPU statistics accounting
- kernel telemetry via procfs
- user-kernel communication via Generic Netlink
---

### 9. References

#### Netlink & Generic Netlink
* [Introduction to Netlink — The Linux Kernel Documentation](https://docs.kernel.org/userspace-api/netlink/intro.html)
* [Netlink Handbook — The Linux Kernel Documentation](https://docs.kernel.org/userspace-api/netlink/index.html)
* [Netlink Library (libnl) Core Documentation](https://www.infradead.org/~tgr/libnl/doc/core.html)
* [Generic Netlink Library (libnl-genl) API Reference](https://www.infradead.org/~tgr/libnl/doc/api/group__genl.html)
* [mac80211_hwsim.c (Linux v6.12.61) — Kernel Generic Netlink Example](https://elixir.bootlin.com/linux/v6.12.61/source/drivers/net/wireless/virtual/mac80211_hwsim.c#L6306)
* [iw (nl80211 Userspace CLI Utility) — GitHub](https://github.com/Distrotech/iw)

#### Kernel Synchronization & Per-CPU Operations
* [this_cpu Operations — The Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/_sources/core-api/this_cpu_ops.rst.txt)
* [What is RCU? ("Read, Copy, Update") — The Linux Kernel Documentation](https://docs.kernel.org/RCU/whatisRCU.html)
* [hashtable.h (Linux v6.12.61) — Kernel RCU-enabled Hash Table](https://elixir.bootlin.com/linux/v6.12.61/source/include/linux/hashtable.h)

#### Virtual Filesystem & Statistics
* [The seq_file Interface — The Linux Kernel Documentation](https://docs.kernel.org/filesystems/seq_file.html)
* [u64_stats_sync.h (Linux v6.12.61) — 64-bit Statistics Synchronization](https://elixir.bootlin.com/linux/v6.12.61/source/include/linux/u64_stats_sync.h)

#### Embedded Build System
* [The Buildroot User Manual — Adding New Packages](https://buildroot.org/downloads/manual/manual.html#adding-packages)