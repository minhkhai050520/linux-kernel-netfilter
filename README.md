# Linux Kernel Netfilter Firewall Extension

A Linux kernel module for packet inspection and filtering at the IPv4 Netfilter hook layer. The project is designed around a custom firewall/monitoring extension that extracts packet metadata, matches it against a rule table, updates per-CPU counters, exposes statistics through procfs, and provides a Generic Netlink control plane for kernel-user communication.

This repository implements the core kernel-side functionality for a Raspberry Pi 4 / ARM64 Linux environment and is aligned with the design goals described in the project specification: high-performance pre-routing inspection, RCU-based rule lookup, per-CPU statistics, and a Generic Netlink interface for telemetry and rule management.

---

## 1. Project Overview

The module registers a Netfilter hook on `NF_INET_PRE_ROUTING` and inspects IPv4 packets before they are forwarded deeper in the networking stack. It extracts the essential flow tuple:

- source IP
- destination IP
- source port
- destination port
- protocol

If a matching rule is found in the internal rule hashtable, the module can either accept or drop the packet. In addition, packet and byte counters are updated per CPU to reduce contention and improve scalability on multi-core ARM systems such as the Raspberry Pi 4.

The implementation is intentionally structured to resemble a minimal but realistic production kernel extension, with separate components for:

- packet filtering logic
- rule storage and lookup
- statistics accounting
- procfs reporting
- Generic Netlink IPC

---

## 2. Objectives

The project addresses the following goals:

- Inspect network packets in kernel space without introducing unnecessary latency.
- Match packet tuples against a dynamic rule set in a scalable way.
- Provide fast read-side lookup using RCU-aware patterns.
- Maintain per-CPU counters for statistics collection.
- Expose kernel telemetry via procfs and Generic Netlink.
- Serve as a modular foundation for user-space firewall tooling.

---

## 3. Technical Architecture

### 3.1 Netfilter Hook

The module registers a hook function at `NF_INET_PRE_ROUTING`:

- hook function: `ipv4_nf_hook_fn()`
- protocol family: `NFPROTO_IPV4`
- priority: `NF_IP_PRI_FIRST`

This ensures the packet is processed as early as possible in the receive path.

### 3.2 Packet Parsing

The hook validates the skb and IP header, then parses TCP or UDP headers when applicable. It extracts tuples such as:

- `src_ip`
- `dst_ip`
- `src_port`
- `dst_port`
- `proto`

The filter logic supports rule matching with wildcards for fields that are left zero-valued, which makes matching more flexible.

### 3.3 Rule Engine

The rule table is stored in a hash table using Linux kernel hashtable APIs:

- table type: `struct hlist_head`
- bit size: `FW_RULE_HTABLE_BITS = 8`
- bucket count: `1 << 8` = 256
- read lock: `rcu_read_lock()`
- write update: `spin_lock_bh()`

The lookup path is RCU-aware and is implemented in `myfw_rcu_api.c`:

- `lookup_rule_rcu()`
- `add_rule_rcu()`
- `del_rule_rcu()`
- `clean_hashtable_rcu()`

This pattern is intended to minimize lock contention on the hot path, which is important on multi-core ARM systems.

### 3.4 Statistics Collection

Per-CPU counters are allocated using `alloc_percpu()`:

- `struct myfw_stats`
- `rx_packets`
- `rx_bytes`
- `dropped_packets`

The implementation uses `u64_stats_t` and `u64_stats_sync` helpers for safe reads and writes across CPUs. This reduces lock contention compared with a single global counter while still preserving correctness under concurrent access.

### 3.5 Procfs Interface

A proc entry is created as:

- `/proc/myfw_stats`

The proc file has two roles:

- read: dumps aggregated values
- write: resets counters

The module exposes the current traffic totals to user space without requiring a custom kernel API for basic telemetry.

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

## 5. Key Kernel Interfaces Used

The implementation relies on Linux kernel APIs and subsystems including:

- `netfilter` and `nf_hook_ops`
- `struct sk_buff`
- `ip_hdr()` / `tcp_hdr()` / `udp_hdr()`
- `rcu_read_lock()` / `rcu_read_unlock()`
- `hash_for_each_possible_rcu()` / `hash_add_rcu()` / `hash_del_rcu()`
- `alloc_percpu()` / `free_percpu()`
- `proc_create()` / `remove_proc_entry()`
- `genetlink` registration and message parsing

---

## 6. Build Instructions

### 6.1 Prerequisites

- Linux kernel source tree for ARM64
- cross-compiler for ARM64, e.g. `aarch64-linux-gnu-gcc`
- kernel configuration with Netfilter enabled
- root privileges for module loading on the target system

### 6.2 Build the module

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

### 6.3 Deploy to target

On a Raspberry Pi or ARM64 Linux target, copy the generated module file and load it:

```bash
sudo insmod myfw.ko
```

To unload:

```bash
sudo rmmod myfw
```

Check kernel logs:

```bash
dmesg | tail -n 50
```

---

## 7. Runtime Behavior

Once the module is loaded:

1. It initializes the hash table.
2. Allocates per-CPU stats.
3. Registers the Generic Netlink family.
4. Creates `/proc/myfw_stats`.
5. Registers the `NF_INET_PRE_ROUTING` hook.

At runtime, every incoming IPv4 packet is inspected. The module calculates the packet 5-tuple and checks whether it matches a configured rule. If a rule matches with a drop action, the packet is dropped; otherwise it is accepted.

---

## 8. Rule Model

The rule structure is defined in `myfw_hashtable.h` as:

```c
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
```

Matching logic compares the packet tuple against stored rule fields and supports wildcard-like behavior when rule fields are zero.

---

## 9. Generic Netlink API Summary

The family is named `MYFW_GENL` and supports the following commands:

| Command | Purpose |
| --- | --- |
| `MYFW_CMD_ADD_RULE` | Add a new drop/accept rule |
| `MYFW_CMD_DEL_RULE` | Remove a rule |
| `MYFW_CMD_GET_RULES` | Dump current rules |
| `MYFW_CMD_GET_STATS` | Read statistics |
| `MYFW_CMD_RESET_STATS` | Reset counters |

Attributes used by the protocol include:

- `MYFW_ATTR_SRC_IP`
- `MYFW_ATTR_DST_IP`
- `MYFW_ATTR_SRC_PORT`
- `MYFW_ATTR_DST_PORT`
- `MYFW_ATTR_PROTO`
- `MYFW_ATTR_ACTION`
- `MYFW_ATTR_RX_PACKETS`
- `MYFW_ATTR_RX_BYTES`
- `MYFW_ATTR_DROPPED_PACKETS`

This interface is the kernel side of a Linux-standard control plane intended to connect to a user-space daemon or CLI tool.

---

## 10. Procfs Statistics Format

Reading `/proc/myfw_stats` produces output in the following form:

```text
Rx Packets   : 12345
Rx Bytes     : 987654
Dropped Pkts : 421
```

Writing to the proc file triggers a reset of the counters.

---

## 11. Safety and Implementation Notes

This project is a kernel extension and must be handled carefully:

- validate all packet headers before parsing
- avoid dereferencing memory beyond bounds
- use `pskb_may_pull()` for safe header access
- keep the fast path minimal and deterministic
- ensure synchronization is correct for RCU and per-CPU counters

The current implementation performs basic validation and handles IPv4 TCP/UDP traffic explicitly. It also defines a clear separation between configuration, rules, stats, and IPC logic.

---

## 12. Limitations

This repository is a prototype and experimental module, not yet a production-grade firewall stack. Current limitations include:

- IPv4 focus only; IPv6 is not implemented in the main hook
- no full user-space daemon or CLI tool included in this repo
- no advanced policy engine or rule priority system
- no automated benchmark harness or long-running stress validation code
- no full integration with nftables/iptables-compatible user-space tools

---

## 13. Benchmarking and Validation

This section documents the measured test results collected during runtime validation of the kernel module. The objective was to evaluate throughput stability, resilience under SYN flood conditions, and memory behavior with the firewall extension loaded.

### 13.1 Throughput Impact

Test target: Raspberry Pi / ARM64 environment with a 1 Gbps link.

Tooling: `iperf3`

#### 13.1.1 Before module insertion

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

#### 13.1.2 After module insertion

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

#### 13.1.3 After module insertion and continuous rule add/remove loop

The module was exercised with the script `myfw-loop-test.sh`, which continuously inserted and removed rules in a repeated 20-second cycle.

##### TCP

Observed output:

```text
[  5]   0.00-10.00  sec  1.10 GBytes   943 Mbits/sec    0            sender
[  5]   0.00-10.01  sec  1.10 GBytes   940 Mbits/sec                  receiver
```

Result: the measured throughput stayed within the same range as the no-load case, indicating that rapid rule churn did not induce a major throughput penalty.

##### UDP

Observed output:

```text
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.000 ms  0/906 (0%)  sender
[  5]   0.00-10.00  sec  1.25 MBytes  1.05 Mbits/sec  0.385 ms  0/906 (0%)  receiver
```

Result: UDP performance remained stable with zero packet loss and minimal jitter increase.

Conclusion for throughput impact:

- The module did not significantly reduce network throughput under the tested conditions.
- Drift between baseline and loaded module remained within a narrow range, consistent with normal measurement variation.
- No major throughput degradation was observed while the module was active, even under repeated rule updates.

---

### 13.2 SYN Flood / Stress Test

#### 13.2.1 Rule setup

Observed configuration:

```bash
# myfw-ctl -fw rule show
Rule -> Src:    192.168.10.5:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: DROP
Rule -> Src:    192.168.10.4:0     | Dst:    192.168.10.1:4000  | Proto:   6 | Action: ACCEPT
```

#### 13.2.2 SYN flood with hping3

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

- Under the SYN flood test, the target host saw no successful replies to the flood traffic.
- The firewall module maintained stability without crashing the kernel or stalling the system.
- The counter output confirms the module processed a very large number of packets while not reporting drops in the custom stat view.

#### 13.2.3 Stress test with iperf3

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

### 13.3 Memory Safety Validation

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
- Memory usage remained stable and did not show a sustained upward trend after repeated traffic and rule churn.
- The module appears to be in a stable state for the tested workload.

---

### 13.4 Result Summary

The measured validation results indicate that the module is stable enough for a prototype/educational Linux firewall implementation under the tested conditions:

- Throughput remained stable before and after module insertion.
- No material degradation was observed during repeated rule add/remove operations.
- The system stayed responsive under SYN flood pressure.
- No obvious kernel memory leak was detected in the validation window.

These results are suitable for a prototype-level performance baseline, but they should still be expanded with broader benchmarking on more diverse hardware and traffic patterns.

---

## 14. Future Work

Planned enhancements for a more complete production-grade version include:

- adding IPv6 support
- implementing a richer rule priority model
- extending Generic Netlink notifications for asynchronous alerts
- adding a robust user-space daemon and CLI utility
- integrating with `libnl` / `libnl-genl` for easier user-space control
- extending benchmark automation and regression test coverage

---

## 15. Summary

This kernel module is a compact, educational, and practical network filtering prototype. It brings together the essential components of a Linux kernel firewall extension:

- packet capture at the Netfilter pre-routing layer
- dynamic rules stored in an RCU-aware hash table
- per-CPU statistics accounting
- kernel telemetry via procfs
- user-kernel communication via Generic Netlink

It is a solid foundation for extending into a more complete traffic monitoring and firewall solution for ARM64 and Linux 6.x environments.
