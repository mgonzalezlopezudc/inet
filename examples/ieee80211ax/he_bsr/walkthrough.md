# 802.11ax HE Buffer Status Report (BSR) Simulation

This example illustrates the Buffer Status Report (BSR) and Buffer Status Report Poll (BSRP) mechanisms introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how BSRs assist the AP in dynamically scheduling uplink MU-OFDMA transmissions, and highlights the impact of BSR report freshness on overall network overhead and throughput.

## Background: HE BSR & Uplink OFDMA Scheduling

In 802.11ax, Uplink Multi-User Orthogonal Frequency Division Multiple Access (UL MU-OFDMA) allows multiple stations (STAs) to transmit concurrently to the Access Point (AP) on partitioned frequency channels called **Resource Units (RUs)**.

To allocate RUs efficiently, the AP's Uplink Scheduler needs to know the backlog (buffer size) of each STA's traffic queues. The **BSR** mechanism provides this:
1. **Implicit BSR**: STAs include buffer status details in the *BSR control subfield* of the HE variant HT Control field in uplink MAC data frames.
2. **Explicit BSR / BSRP**: When the AP needs fresh buffer status information and has no pending uplink data from a STA, it transmits a **Buffer Status Report Poll (BSRP)** Trigger frame. The target STAs reply with their current queue status.
3. **Queue Accounting Freshness**: The AP's UL Coordinator maintains an active registry of STA queue statuses. If a status record is older than `reportMaxAge`, it is marked as stale, and the AP must re-poll using a BSRP Trigger frame before scheduling.

---

## Network Topology

The network [HeBsrNetwork.ned](HeBsrNetwork.ned) consists of:
- **`ap`**: An Access Point located at `(300, 210)`.
- **`host[0..2]`**: Three wireless stations located around the AP at distances of 60m.
- **`server`**: A wired server connected to the AP via a 100 Gbps Ethernet link (`ap.ethg++ <--> Eth100G <--> server.ethg++`).
- **Traffic**: Each host runs a `UdpBasicApp` that generates uplink traffic destined for the `server` with 700B messages every 0.35ms. The controlled phase uses a `0.2–0.25s` warm-up and normal traffic from `0.3s`; the bursty comparison starts its first burst at `0.3s`.

```
       [host[0]]  [host[1]]  [host[2]]
           \          |          /
            \         | (wireless)
             v        v        v
                   [ ap ]
                     |
                     | (100G Ethernet)
                     v
                 [server]
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines three test scenarios:

### 1. `FullBsrAccounting` (fresh-report reference)
- The BSR freshness timer (`reportMaxAge`) uses the default (retained longer).
- The AP relies on fresh queue reports delivered implicitly or from previous polls to schedule UL MU-OFDMA data transmissions using `HeUlSchedulerBacklogBased`.
- Fresh reports let the AP allocate RUs from recent queue information without
  first spending another exchange on BSRP polling.

### 2. `StaleBsr`
- The queue report freshness threshold is set to `**.ap.wlan[*].mac.hcf.ulCoordinator.reportMaxAge = 10ms`.
- Since queue statuses expire after 10 ms, the AP's scheduler frequently finds records stale and sends additional BSRP Trigger frames before backlog-based scheduling. The value is deliberately long enough for one Trigger exchange to finish deterministically; it is an INET experiment parameter, not an IEEE timer value.
- The intentionally short age limit makes the cost of stale state visible:
  reports expire on the timescale of the offered traffic, so the AP must
  refresh its view before it can make a useful backlog-based allocation.

### 3. `ImplicitBsr`
- The AP's UL trigger check interval is set to a larger value: `**.ap.wlan[*].mac.hcf.ulTriggerCheckInterval = 0.5s`.
- This allows STAs to first transmit data frames using single-user (SU) EDCA channel access. The buffer status (BSR) is implicitly set in the HE-variant HT Control field of these SU data frames.
- When the AP receives these SU QoS Data frames, it extracts the BSR implicitly, updating its backlog database. It then triggers UL MU-OFDMA transmissions only when necessary, without sending explicit BSRP poll frames.
- This configuration demonstrates how an uplink data frame can refresh the
  AP's scheduling state without a dedicated poll. It is not a pure throughput
  comparison with scheduled OFDMA because it intentionally allows SU EDCA.

---

## Running the Simulation

From the INET project root, use the project launcher.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c ImplicitBsr examples/ieee80211ax/he_bsr/omnetpp.ini
```

While the simulation runs, inspect the AP `wlan[0].mac.hcf.ulCoordinator` module. The watches `bufferStatusSummary`, `freshReports`, `backloggedReports`, `bufferStatusByAid`, `ofdmaContentionWindow`, and `ofdmaBackoff` show how BSR information drives Trigger decisions. The AP `ulTriggerPolicy` watches `lastContext.*` and `lastSelectedTriggerName`, and the `ulScheduler` watches `lastScheduleSummary` and `lastRuAllocations`.

### Running with Cmdenv (Command Line)
```sh
# Run Full BSR Accounting
bin/inet -u Cmdenv -c FullBsrAccounting examples/ieee80211ax/he_bsr/omnetpp.ini

# Run Stale BSR Scenario
bin/inet -u Cmdenv -c StaleBsr examples/ieee80211ax/he_bsr/omnetpp.ini

# Run Implicit BSR Scenario
bin/inet -u Cmdenv -c ImplicitBsr examples/ieee80211ax/he_bsr/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to analyze how many BSRP and Basic triggers were sent by the AP and the total packets received at the server.

```sh
# Query the number of BSRP and Basic Trigger frames sent by the AP
opp_scavetool query -l -f "*Trigger*" examples/ieee80211ax/he_bsr/results/*.sca

# Query the total packets received at the UDP sink on the server
opp_scavetool query -l -f "*packetReceived:count*" examples/ieee80211ax/he_bsr/results/*.sca
```

### Vector summary

The five-seed campaign records AP backlog vectors for `BurstyTraffic`
and `StaleBsr` and measures `0.3–1.9s`. Mean reported/scheduled backlog is
`36,079/41,506 B` for the fresh bursty condition and `73,410/73,565 B` for
the stale condition. These are scheduling-state observations; the campaign
uses those vectors, rather than packet counts, to show what information the
scheduler actually had.

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c StaleBsr examples/ieee80211ax/he_bsr/omnetpp.ini --result-dir=examples/ieee80211ax/he_bsr/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/he_bsr/results/StaleBsr-#0HeBsrNetwork.ap.wlan[0].pcap -c 25
```

The decoded output timeline shows:
1. **BSRP Triggers**: The AP broadcasts Buffer Status Report Poll (BSRP) triggers (e.g. frame 1) to poll station queue statuses. Stations respond with QoS Null frames carrying queue status (e.g. frames 2, 3, 4).
2. **Periodic Polling**: Since the queue status expires rapidly in the `StaleBsr` configuration (10 ms age limit), the AP is forced to re-poll buffer status regularly (e.g. frames 6, 11, 16, 21), generating explicit BSRP overhead.

---

## Interpretation of Results

1. **Fresh information is the advantage**: BSR does not carry payload; it lets
   the AP replace blind polling or contention with informed RU allocation.
   Scheduled bytes following nonzero reports is therefore stronger evidence
   than application packet count alone.
2. **Why `10 ms` matters**: it is long enough for a report exchange to finish,
   but short relative to this saturated workload. The stale case consequently
   accumulates about twice the mean reported backlog of the bursty fresh case.
   That is a scheduling-state penalty, not a claim that every shorter report
   lifetime halves throughput.
3. **Why bursty traffic matters**: filling, draining, and refilling the queues
   makes report freshness observable. A permanently empty or permanently
   saturated queue would reveal much less about whether the AP's view tracks
   changes in demand.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260718T132413Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FullBsrAccounting produced protocol-visible wireless observations | 4465 AP/global transmission observations |
| **PASS** | ImplicitBsr produced protocol-visible wireless observations | 2425 AP/global transmission observations |
| **PASS** | StaleBsr produced protocol-visible wireless observations | 4261 AP/global transmission observations |
| **INCONCLUSIVE** | Reported backlog and scheduler-consumed backlog | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### Configuration: `FullBsrAccounting`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4465**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23bf18" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 275 | 6.16% | 833.1 B | 112.8 B | 463.4 us | 63.6 us | 5010 MHz | -72.0 dBm | - | 9.33% | 6.37% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 964 | 21.59% | 1319.7 B | 309.6 B | 757.9 us | 169.4 us | 5010 MHz | -72.0 dBm | - | 53.51% | 36.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35cc24" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC] | 498 | 11.15% | 770.0 B | 0.0 B | 680.2 us | 0.0 us | 5005 MHz, 5015 MHz | -72.0 dBm | - | 24.81% | 16.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 6 | 0.13% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -72.0 dBm | - | 0.18% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a4d13" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC] | 12 | 0.27% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5010 MHz | -72.0 dBm | - | 0.14% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#117811" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC] | 965 | 21.61% | 34.0 B | 0.0 B | 96.4 us | 0.0 us | 5003 MHz, 5013 MHz, 5017 MHz | -72.0 dBm | - | 6.82% | 4.65% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 517 | 11.58% | 46.1 B | 1.8 B | 39.0 us | 0.1 us | 5010 MHz | - | 10.0 dBm | 1.48% | 1.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 88 | 1.97% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.24% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 517 | 11.58% | 57.8 B | 1.9 B | 39.8 us | 0.1 us | 5010 MHz | - | 10.0 dBm | 1.51% | 1.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 64 | 1.43% | 152.0 B | 0.0 B | 46.0 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.22% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 547 | 12.25% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.75% | 1.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 6 | 0.13% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 6 | 0.13% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |

### Configuration: `ImplicitBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2425**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23bf18" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 100 | 4.12% | 841.9 B | 76.0 B | 468.8 us | 45.3 us | 5010 MHz | -72.0 dBm | - | 4.43% | 2.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 1298 | 53.53% | 1292.4 B | 314.2 B | 743.0 us | 171.9 us | 5010 MHz | -72.0 dBm | - | 91.16% | 48.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35cc24" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC] | 3 | 0.12% | 770.0 B | 0.0 B | 680.2 us | 0.0 us | 5005 MHz | -72.0 dBm | - | 0.19% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#117811" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC] | 6 | 0.25% | 34.0 B | 0.0 B | 96.4 us | 0.0 us | 5013 MHz, 5017 MHz | -72.0 dBm | - | 0.05% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 3 | 0.12% | 46.0 B | 0.0 B | 39.0 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 92 | 3.79% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.33% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 3 | 0.12% | 58.0 B | 0.0 B | 39.8 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 86 | 3.55% | 152.0 B | 0.0 B | 46.0 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.37% | 0.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 822 | 33.90% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.39% | 1.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 6 | 0.25% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 6 | 0.25% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |

### Configuration: `StaleBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4261**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23bf18" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 240 | 5.63% | 848.0 B | 49.1 B | 471.8 us | 31.3 us | 5010 MHz | -72.0 dBm | - | 8.31% | 5.66% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 967 | 22.69% | 1332.5 B | 293.7 B | 764.9 us | 160.6 us | 5010 MHz | -72.0 dBm | - | 54.30% | 36.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35cc24" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC] | 470 | 11.03% | 770.0 B | 0.0 B | 680.2 us | 0.0 us | 5005 MHz, 5015 MHz | -72.0 dBm | - | 23.47% | 15.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 105 | 2.46% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -72.0 dBm | - | 3.07% | 2.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a4d13" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC] | 18 | 0.42% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz | -72.0 dBm | - | 0.21% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#117811" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC] | 797 | 18.70% | 34.0 B | 0.0 B | 96.4 us | 0.0 us | 5003 MHz, 5013 MHz, 5017 MHz | -72.0 dBm | - | 5.64% | 3.84% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 499 | 11.71% | 47.8 B | 6.9 B | 39.1 us | 0.5 us | 5010 MHz | - | 10.0 dBm | 1.43% | 0.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 89 | 2.09% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.25% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 499 | 11.71% | 57.2 B | 3.4 B | 39.8 us | 0.2 us | 5010 MHz | - | 10.0 dBm | 1.46% | 0.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 60 | 1.41% | 152.0 B | 0.0 B | 46.0 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.20% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 505 | 11.85% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.62% | 1.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 6 | 0.14% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 6 | 0.14% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |

### Analysis of Packet Distribution
The scheduled conditions contain the expected Trigger/response activity, but a BSR is an A-Control scheduling input rather than a frame subtype. IEEE Std 802.11-2024 Clause 26.5.5 requires the report contents and capability conditions; use the AP-reported and scheduled-backlog telemetry documented above. QoS Data counts are not evidence that a BSR was fresh or that the reported bytes were delivered.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->
