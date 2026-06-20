# Walkthrough - 802.11ax MU OFDMA Simulation Examples

This document walks through the design, implementation, and verification of five distinct simulation scenarios illustrating the most prominent features of 802.11ax Multi-User Orthogonal Frequency Division Multiple Access (MU OFDMA) in the INET Framework.

---

## 1. Implemented Simulation Configurations

All configurations are defined in [omnetpp.ini](examples/ieee80211/ofdma/omnetpp.ini) within the `ofdma` example folder.

### Scenario A: Bandwidth Optimization (`EqualSizedRUs_fBW`)
- **Objective**: Maximize bandwidth occupancy (spectral efficiency) by choosing larger Resource Units (RUs).
- **Mechanism**: Uses the `HeDlSchedulerEqualSizedRUs` scheduler with `schedulingFunction = "fBW"`. Under a 20 MHz channel, it allocates two 106-tone RUs (which covers 87.6% of the tones), serving 2 out of the 3 active users concurrently. The third user is buffered for subsequent frames.

### Scenario B: Latency/STA Concurrency Optimization (`EqualSizedRUs_fHoL`)
- **Objective**: Minimize Head-of-Line (HoL) packet delay and maximize user concurrency.
- **Mechanism**: Uses the `HeDlSchedulerEqualSizedRUs` scheduler with `schedulingFunction = "fHoL"`. Under a 20 MHz channel, it allocates three 52-tone RUs, serving all 3 active users concurrently. Under high packet arrival rates, this concurrency gain outweighs the slight reduction in spectral efficiency.

### Scenario C: Queue-Aware Dynamic RU Allocation (`BacklogBased`)
- **Objective**: Dynamically allocate mixed RU sizes (e.g., larger RU for heavy backlog, smaller RU for low backlog) in a multi-user frame.
- **Mechanism**: Uses `HeDlSchedulerBacklogBased` with asymmetric traffic profiles (heavy load on `host[0]`, medium on `host[1]`, light on `host[2]`).

### Scenario D: Latency-Centric Scheduling Baseline (`HoLMinDelay`)
- **Objective**: Provide a comparison baseline for `BacklogBased` by scheduling RUs based strictly on the sizes and delays of Head-of-Line packets.
- **Mechanism**: Uses `HeDlSchedulerHoLMinDelay` under the same asymmetric traffic profile as Scenario C.

### Scenario E: Wide-Bandwidth High-Capacity OFDMA (`WideBandwidth80MHz`)
- **Objective**: Demonstrate 802.11ax OFDMA scalability to a wide 80 MHz channel supporting 8 active users concurrently.
- **Mechanism**: Configures an 80 MHz band, center frequency `5.2 GHz`, channel number `2`, and scales the topology to 8 wireless hosts. The scheduler allocates eight 106-tone RUs concurrently.
- **Key Realization**: To allow correct physical reception of all subchannels by the hosts, the receiver bandwidth is explicitly set to `80MHz` (`**.wlan[*].radio.receiver.bandwidth = 80MHz`), and the transmitter power is configured to `100mW` to compensate for the 10 dB noise integration penalty over the wider bandwidth.

---

## 2. Verification and Quantitative Results

All simulations were run to completion (time limit of `2.0s`). Below is the comparative results table containing the number of successfully received UDP packets at the application layer:

### Summary of Simulation Results

| Scenario / Config | Description | Scheduler / Function | Channel / Bandwidth | Packets Received by Clients (UDP App) |
|---|---|---|---|---|
| **`EqualSizedRUs_fBW`** | Bandwidth Optimization | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (20 MHz) | `host[0]`: 34 <br> `host[1]`: 25 <br> `host[2]`: 26 <br> **Total: 85** |
| **`EqualSizedRUs_fHoL`** | Concurrency Optimization | `HeDlSchedulerEqualSizedRUs` (fHoL) | 5 GHz (20 MHz) | `host[0]`: 39 <br> `host[1]`: 31 <br> `host[2]`: 32 <br> **Total: 102** |
| **`BacklogBased`** | Queue-Aware Sizing | `HeDlSchedulerBacklogBased` | 5 GHz (20 MHz) | `host[0]`: 101 <br> `host[1]`: 69 <br> `host[2]`: 16 <br> **Total: 186** |
| **`HoLMinDelay`** | Baseline Latency | `HeDlSchedulerHoLMinDelay` | 5 GHz (20 MHz) | `host[0]`: 101 <br> `host[1]`: 44 <br> `host[2]`: 6 <br> **Total: 151** |
| **`WideBandwidth80MHz`**| 80 MHz Scalability (8 users) | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (80 MHz) | `host[0]`: 534, `host[1]`: 523, `host[2]`: 518, `host[3]`: 514, <br> `host[4]`: 509, `host[5]`: 505, `host[6]`: 495, `host[7]`: 546 <br> **Total: 4144** |

---

## 3. Analysis and Insights

### Concurrency vs. Spectral Efficiency (fBW vs. fHoL)
- By scheduling all 3 stations concurrently using three 52-tone RUs, `EqualSizedRUs_fHoL` reduces the average queuing and Head-of-Line delays. This concurrency advantage yields a **20% throughput improvement** (102 vs. 85 received packets) over the bandwidth-optimizing `fBW` scheduler, which leaves one client starved in each transmission slot by allocating larger 106-tone RUs.

### Queue-Aware Multi-User Sizing (BacklogBased vs. HoLMinDelay)
- Under asymmetric traffic loads, the `BacklogBased` scheduler dynamically scales the size of the RUs according to the backlog levels. Active stations with lower load (`host[1]` and `host[2]`) are still assigned smaller RUs instead of being starved.
- Compared to `HoLMinDelay`, `BacklogBased` increases throughput for medium-load `host[1]` from **44 to 69 packets** and for low-load `host[2]` from **6 to 16 packets**, showing a **23% overall throughput boost** (186 vs. 151 packets).

### Wide Bandwidth High Capacity (80 MHz Scaling)
- Scaling the channel to 80 MHz enables all 8 users to be scheduled concurrently using separate 106-tone RUs.
- This scenario demonstrates the full multi-user capability of 802.11ax OFDMA: overall throughput scales by **~40 times** (from ~100 to 4,144 packets), and traffic is distributed extremely evenly across all 8 clients.
