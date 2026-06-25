# Walkthrough - 802.11ax MU OFDMA Simulation Examples

This document walks through the design, implementation, and verification of five distinct simulation scenarios illustrating the most prominent features of 802.11ax Multi-User Orthogonal Frequency Division Multiple Access (MU OFDMA) in the INET Framework.

These examples are standards-aware packet-level simulations, not bit-level interoperability tests. The HE PHY header and Trigger frame serializers still carry INET model metadata rather than exact HE-SIG/Trigger on-air encodings. The `MultiTidBlockAck` configuration negotiates Multi-TID capability and exercises the BAR/BlockAck plumbing, but the current DL path creates one Block Ack record per request, so it should not be read as proof of aggregated multi-TID operation. FEC legality is checked strictly in the PHY calculator, while mixed LDPC/non-LDPC scheduling is still coarse because a schedule chooses one coding mode across the selected peers.

---

## 1. Implemented Simulation Configurations

All configurations are defined in [omnetpp.ini](omnetpp.ini) within the `dl_ofdma` example folder.

### Scenario A: Bandwidth Optimization (`EqualSizedRUs_fBW`)
- **Objective**: Maximize bandwidth occupancy (spectral efficiency) by choosing larger Resource Units (RUs).
- **Mechanism**: Uses the `HeDlSchedulerEqualSizedRUs` scheduler with `schedulingFunction = "fBW"`. Under a 20 MHz channel, it allocates two 106-tone RUs (which covers 87.6% of the tones), serving 2 out of the 3 active users concurrently. The third user is buffered for subsequent frames.

### Scenario B: Latency/STA Concurrency Optimization (`EqualSizedRUs_fHoL`)
- **Objective**: Minimize Head-of-Line (HoL) packet delay and maximize user concurrency.
- **Mechanism**: Uses the `HeDlSchedulerEqualSizedRUs` scheduler with `schedulingFunction = "fHoL"`. Under a 20 MHz channel, it allocates three 52-tone RUs, serving all 3 active users concurrently. This improves short-run per-station fairness compared with the bandwidth-oriented scheduler, at the cost of smaller RUs.

### Scenario C: Queue-Aware Dynamic RU Allocation (`BacklogBased`)
- **Objective**: Dynamically allocate mixed RU sizes (e.g., larger RU for heavy backlog, smaller RU for low backlog) in a multi-user frame.
- **Mechanism**: Uses `HeDlSchedulerBacklogBased` with asymmetric traffic profiles (heavy load on `host[0]`, medium on `host[1]`, light on `host[2]`).

### Scenario D: Latency-Centric Scheduling Baseline (`HoLMinDelay`)
- **Objective**: Provide a comparison baseline for `BacklogBased` by scheduling RUs based strictly on the sizes and delays of Head-of-Line packets.
- **Mechanism**: Uses `HeDlSchedulerHoLMinDelay` under the same asymmetric traffic profile as Scenario C.

### Scenario E: Wide-Bandwidth 80 MHz Configuration (`WideBandwidth80MHz`)
- **Objective**: Exercise the 80 MHz channel setup with 8 configured users and 8 scheduled RUs.
- **Mechanism**: Configures an 80 MHz band, center frequency `5.2 GHz`, channel number `2`, and scales the topology to 8 wireless hosts. The scheduler allocates eight 106-tone RUs concurrently.
- **Key Realization**: To allow correct physical reception of all subchannels by the hosts, the receiver bandwidth is explicitly set to `80MHz` (`**.wlan[*].radio.receiver.bandwidth = 80MHz`), and the transmitter power is configured to `100mW` to compensate for the 10 dB noise integration penalty over the wider bandwidth.

---

## 2. Verification and Quantitative Results

All simulations were run to completion with the current `sim-time-limit = 0.6s`. Below is the comparative results table containing the number of successfully received UDP packets at the application layer:

### Summary of Simulation Results

| Scenario / Config | Description | Scheduler / Function | Channel / Bandwidth | Packets Received by Clients (UDP App) |
|---|---|---|---|---|
| **`EqualSizedRUs_fBW`** | Bandwidth Optimization | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (20 MHz) | `host[0]`: 43 <br> `host[1]`: 43 <br> `host[2]`: 0 <br> **Total: 86** |
| **`EqualSizedRUs_fHoL`** | Concurrency Optimization | `HeDlSchedulerEqualSizedRUs` (fHoL) | 5 GHz (20 MHz) | `host[0]`: 22 <br> `host[1]`: 22 <br> `host[2]`: 22 <br> **Total: 66** |
| **`BacklogBased`** | Queue-Aware Sizing | `HeDlSchedulerBacklogBased` | 5 GHz (20 MHz) | `host[0]`: 101 <br> `host[1]`: 138 <br> `host[2]`: 56 <br> **Total: 295** |
| **`HoLMinDelay`** | Baseline Latency | `HeDlSchedulerHoLMinDelay` | 5 GHz (20 MHz) | `host[0]`: 101 <br> `host[1]`: 113 <br> `host[2]`: 60 <br> **Total: 274** |
| **`WideBandwidth80MHz`**| 80 MHz configuration smoke test (8 users) | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (80 MHz) | `host[0]`: 6, `host[1]`: 6, `host[2]`: 0, `host[3]`: 0, <br> `host[4]`: 0, `host[5]`: 0, `host[6]`: 0, `host[7]`: 0 <br> **Total: 12** |

---

## 3. Analysis and Insights

### Concurrency vs. Spectral Efficiency (fBW vs. fHoL)
- By scheduling all 3 stations concurrently using three 52-tone RUs, `EqualSizedRUs_fHoL` demonstrates equal per-station service in the current short run (`22/22/22`). The bandwidth-optimizing `fBW` scheduler delivers more packets overall (`86` vs. `66`) by using larger 106-tone RUs, but it leaves `host[2]` without delivered packets in this result set.

### Queue-Aware Multi-User Sizing (BacklogBased vs. HoLMinDelay)
- Under asymmetric traffic loads, the `BacklogBased` scheduler dynamically scales the size of the RUs according to the backlog levels. Active stations with lower load (`host[1]` and `host[2]`) are still assigned RUs instead of being starved.
- Compared to `HoLMinDelay`, `BacklogBased` keeps heavy-load `host[0]` unchanged at **101 packets**, increases medium-load `host[1]` from **113 to 138 packets**, and decreases low-load `host[2]` from **60 to 56 packets**. The aggregate result is about an **8% throughput increase** (295 vs. 274 packets), with a different fairness/queueing tradeoff.

### Wide Bandwidth 80 MHz Configuration
- Scaling the channel to 80 MHz configures an 8-host, 8-RU scheduling case using separate 106-tone RUs.
- The current `0.6s` result file should be treated as a configuration smoke test rather than a capacity benchmark: only `host[0]` and `host[1]` have delivered application packets. Longer seeded runs and broader metrics are needed before making throughput-scaling claims for this scenario.
