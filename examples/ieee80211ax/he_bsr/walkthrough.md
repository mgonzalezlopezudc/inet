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
- **`server`**: A wired server connected to the AP via a 100 Mbps Ethernet link (`ap.ethg++ <--> Eth100M <--> server.ethg++`).
- **Traffic**: Each host runs a `UdpBasicApp` that generates uplink traffic destined for the `server` with 700B messages every 0.35ms.

```
       [host[0]]  [host[1]]  [host[2]]
           \          |          /
            \         | (wireless)
             v        v        v
                   [ ap ]
                     |
                     | (100M Ethernet)
                     v
                 [server]
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines three test scenarios:

### 1. `FullBsrAccounting` (Default/Baseline)
- The BSR freshness timer (`reportMaxAge`) uses the default (retained longer).
- The AP relies on fresh queue reports delivered implicitly or from previous polls to schedule UL MU-OFDMA data transmissions using `HeUlSchedulerBacklogBased`.
- **Result**: AP triggers frequent UL transmissions with minimal BSRP polling overhead, but continuous Trigger-based PPDU scheduling introduces frame aggregation limits and SIFS overhead under high backlog.

### 2. `StaleBsr`
- The queue report freshness threshold is set to `**.ap.wlan[*].mac.hcf.ulCoordinator.reportMaxAge = 10ms`.
- Since queue statuses expire after 10 ms, the AP's scheduler frequently finds records stale and sends additional BSRP Trigger frames before backlog-based scheduling. The value is deliberately long enough for one Trigger exchange to finish deterministically; it is an INET experiment parameter, not an IEEE timer value.
- **Result**: Higher explicit BSRP polling overhead (373 BSRP Triggers vs. 3 in baseline) due to continuous expiration.

### 3. `ImplicitBsr`
- The AP's UL trigger check interval is set to a larger value: `**.ap.wlan[*].mac.hcf.ulTriggerCheckInterval = 0.5s`.
- This allows STAs to first transmit data frames using single-user (SU) EDCA channel access. The buffer status (BSR) is implicitly set in the HE-variant HT Control field of these SU data frames.
- When the AP receives these SU QoS Data frames, it extracts the BSR implicitly, updating its backlog database. It then triggers UL MU-OFDMA transmissions only when necessary, without sending explicit BSRP poll frames.
- **Result**: Zero explicit BSRP triggers, minimal Basic trigger overhead, and significantly higher aggregate throughput since STAs utilize efficient SU EDCA transmissions.

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

### Expected Output Summary

```
FullBsrAccounting-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   3
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  541
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count        1034

StaleBsr-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   28
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  500
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count        1012

ImplicitBsr-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   0
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  4
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count        2077
```

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

1. **Explicit BSRP Polling Overhead**:
   - In `ImplicitBsr`, the AP sends **0 BSRP Triggers**. Because the STAs transmit BSRs implicitly inside uplink SU data frames during their EDCA transmit opportunities, the AP's coordinator receives regular backlog updates without having to explicitly query the STAs.
   - In `FullBsrAccounting`, the AP sends **3 BSRP Triggers**.
   - In `StaleBsr`, the BSRP trigger count rises to **28 BSRP Triggers**. Because queue status records expire after 10 ms, the scheduler periodically has to refresh them.

2. **Trigger-Based Scheduling vs. EDCA Throughput**:
   - `ImplicitBsr` delivers **2077 packets** to the server, which is the highest throughput among the three configurations. By scheduling trigger checks at 0.5s intervals, STAs make heavy use of standard SU EDCA channel access, avoiding the overhead of trigger frame sequences, Multi-STA BlockAck responses, and padding. Because the AP receives these implicit reports during normal EDCA transmissions, it requires **0 explicit BSRP trigger frames** and **4 Basic Triggers** to maintain fresh queue records.
   - `FullBsrAccounting` delivers **1034 packets** and sends **541 Basic Triggers**. The constant polling and triggering overhead at 1ms check intervals limits the aggregate channel capacity, resulting in lower throughput than SU EDCA.
   - `StaleBsr` delivers **1012 packets**. Because it spends extra overhead in BSRP polling cycles, it sends slightly fewer Basic Triggers (500) and suffers a slight throughput reduction compared to the full baseline.
