# Walkthrough - 802.11ax MU OFDMA Simulation Examples

This document walks through the design, implementation, and verification of the downlink 802.11ax multi-user scenarios in the INET Framework.

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

### Additional configurations

- `SuEdcaBaseline` runs the same workload through single-user QoS EDCA.
- `DlMuMimo` exercises full-bandwidth downlink MU-MIMO with three stations.
- `MultiTidBlockAck` negotiates and exercises HE Multi-TID Block Ack plumbing.
- `DlMuMimo80MHz` combines the 80 MHz topology with downlink MU-MIMO.

These mechanisms share the topology for controlled comparison even though
they are not OFDMA scheduler variants.

---

## 2. Verification and Quantitative Results

All simulations were run with Cmdenv in the release build, configuration run
`0`, seed set `0`, and the configured `sim-time-limit = 0.6s`. The table reports
the `packetReceived:count` scalar of each UDP sink. Repeated runs with the same
run number and seed set are deterministic.

### Summary of Simulation Results

| Scenario / Config | Description | Scheduler / Function | Channel / Bandwidth | Packets Received by Clients (UDP App) |
|---|---|---|---|---|
| **`EqualSizedRUs_fBW`** | Bandwidth Optimization | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (20 MHz) | `host[0]`: 47 <br> `host[1]`: 47 <br> `host[2]`: 47 <br> **Total: 141** |
| **`EqualSizedRUs_fHoL`** | Concurrency Optimization | `HeDlSchedulerEqualSizedRUs` (fHoL) | 5 GHz (20 MHz) | `host[0]`: 43 <br> `host[1]`: 43 <br> `host[2]`: 43 <br> **Total: 129** |
| **`BacklogBased`** | Queue-Aware Sizing | `HeDlSchedulerBacklogBased` | 5 GHz (20 MHz) | `host[0]`: 37 <br> `host[1]`: 37 <br> `host[2]`: 58 <br> **Total: 132** |
| **`HoLMinDelay`** | Baseline Latency | `HeDlSchedulerHoLMinDelay` | 5 GHz (20 MHz) | `host[0]`: 33 <br> `host[1]`: 33 <br> `host[2]`: 65 <br> **Total: 131** |
| **`WideBandwidth80MHz`**| 80 MHz, 8-user scaling | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (80 MHz) | `host[0..7]`: 86 each <br> **Total: 688** |
| **`MultiTidBlockAck`** | Downlink Multi-TID Block Ack | `HeDlSchedulerEqualSizedRUs` (fBW) | 5 GHz (20 MHz) | `host[0]`: 115 (80 VO, 35 NC) <br> `host[1]`: 115 (80 VO, 35 NC) <br> `host[2]`: 0 <br> **Total: 230** |

---

## 3. Analysis and Insights

### Concurrency vs. Spectral Efficiency (fBW vs. fHoL)
- `EqualSizedRUs_fHoL` schedules all three stations in every MU opportunity using three 52-tone RUs. The run contains 42 three-user HE MU PPDUs and delivers `43/43/43` packets, including the single-user Block Ack bootstrap packet for each STA.
- `EqualSizedRUs_fBW` selects two 106-tone RUs per MU PPDU. It rotates service across the three backlogged STAs rather than permanently excluding the third one: 70 two-user HE MU PPDUs deliver `47/47/47` packets.

### Queue-Aware Multi-User Sizing (BacklogBased vs. HoLMinDelay)
- Both asymmetric scenarios schedule all three active stations in every MU opportunity. `BacklogBased` creates 12 mixed-RU PPDUs; `HoLMinDelay` creates 32 three-user PPDUs.
- Packet counts alone are not a throughput comparison because the application payloads are 1000, 400, and 100 bytes for hosts 0, 1, and 2. In delivered application bytes, `BacklogBased` carries 57,600 bytes versus 52,700 bytes for `HoLMinDelay`, about 9.3% more in this deterministic short run. `HoLMinDelay` gives the light, small-packet flow more transmission opportunities.
- The central 26-tone RU used by the light flow is evaluated with an RU-bandwidth-adjusted receive threshold. This keeps the PHY sensitivity test consistent with the medium's per-RU receive-power scaling.

### Wide Bandwidth 80 MHz Configuration
- Scaling the channel to 80 MHz configures an 8-host, 8-RU scheduling case using separate 106-tone RUs.
- The run creates 85 eight-user HE MU PPDUs and delivers 86 packets to every host (one single-user Block Ack bootstrap packet plus 85 MU packets). This verifies 8-user RU allocation and reception, but a single short deterministic run is still not a capacity benchmark.

### Downlink Multi-TID Block Ack (`MultiTidBlockAck`)
- In this configuration, Multi-TID Aggregation is negotiated between the AP and the client stations. 
- The Server sends two concurrent streams per host (TID 6 / Voice and TID 7 / Network Control). The AP accumulates this multi-TID downlink data, packs them into HE MU PPDUs using `sequentialBar` acknowledgement method, and issues Multi-TID BAR frames to confirm multi-TID delivery. 
- A total of 115 packets are successfully received by both `host[0]` and `host[1]`.

## 4. Qtenv WATCH Inspection

Run any configuration with `-u Qtenv`, select the AP's `wlan[0].mac.hcf` module, and inspect `heHcfSummary`, `pendingUlTriggerName`, `stationQueueBanks`, `triggeredUlExchangeCount`, and `csiManager.csiTable`.

For the AP scheduler submodule, inspect `lastScheduleSummary`, `lastCandidates`, and `lastRuAllocations`. These watches show whether the scheduler selected ordinary OFDMA or, in `DlMuMimo`, a full-channel MU-MIMO group with per-user NSS/MCS/SNR.

For PHY details, inspect the AP radio transmitter's `lastHeTransmissionSummary` and `lastHeUserPhyParameters`. These expose the HE PPDU format, user count, RU layout, coding, packet extension, puncturing mask, LDPC accounting, and resolved PPDU duration.
