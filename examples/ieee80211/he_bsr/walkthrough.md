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
- The queue report freshness threshold is set to a very low value: `**.ap.wlan[*].mac.hcf.ulCoordinator.reportMaxAge = 1ms`.
- Since queue statuses expire in 1ms, the AP's scheduler frequently finds the records stale and is forced to send additional BSRP Trigger frames to re-verify backlogs before scheduling.
- **Result**: Higher explicit BSRP polling overhead (373 BSRP Triggers vs. 3 in baseline) due to continuous expiration.

### 3. `ImplicitBsr`
- The AP's UL trigger check interval is set to a larger value: `**.ap.wlan[*].mac.hcf.ulTriggerCheckInterval = 0.5s`.
- This allows STAs to first transmit data frames using single-user (SU) EDCA channel access. The buffer status (BSR) is implicitly set in the HE-variant HT Control field of these SU data frames.
- When the AP receives these SU QoS Data frames, it extracts the BSR implicitly, updating its backlog database. It then triggers UL MU-OFDMA transmissions only when necessary, without sending explicit BSRP poll frames.
- **Result**: Zero explicit BSRP triggers, minimal Basic trigger overhead, and significantly higher aggregate throughput since STAs utilize efficient SU EDCA transmissions.

---

## Running the Simulation

Ensure your environment is set up and compiled, then run the simulations.

### Running with Qtenv (GUI)
```sh
source /home/user/omnetpp-6.4.0aipre2/setenv && source /home/user/omnetpp_ws/inet/setenv
opp_run -u Qtenv --ned-path=/home/user/omnetpp_ws/inet/src:/home/user/omnetpp_ws/inet/examples -l /home/user/omnetpp_ws/inet/src/libINET.so -c ImplicitBsr examples/ieee80211/he_bsr/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
source /home/user/omnetpp-6.4.0aipre2/setenv && source /home/user/omnetpp_ws/inet/setenv

# Run Full BSR Accounting
opp_run -u Cmdenv --ned-path=/home/user/omnetpp_ws/inet/src:/home/user/omnetpp_ws/inet/examples -l /home/user/omnetpp_ws/inet/src/libINET.so -c FullBsrAccounting examples/ieee80211/he_bsr/omnetpp.ini

# Run Stale BSR Scenario
opp_run -u Cmdenv --ned-path=/home/user/omnetpp_ws/inet/src:/home/user/omnetpp_ws/inet/examples -l /home/user/omnetpp_ws/inet/src/libINET.so -c StaleBsr examples/ieee80211/he_bsr/omnetpp.ini

# Run Implicit BSR Scenario
opp_run -u Cmdenv --ned-path=/home/user/omnetpp_ws/inet/src:/home/user/omnetpp_ws/inet/examples -l /home/user/omnetpp_ws/inet/src/libINET.so -c ImplicitBsr examples/ieee80211/he_bsr/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to analyze how many BSRP and Basic triggers were sent by the AP and the total packets received at the server.

```sh
# Query the number of BSRP and Basic Trigger frames sent by the AP
source /home/user/omnetpp-6.4.0aipre2/setenv
opp_scavetool query -l -f "*Trigger*" examples/ieee80211/he_bsr/results/*.sca

# Query the total packets received at the UDP sink on the server
opp_scavetool query -l -f "*packetReceived:count*" examples/ieee80211/he_bsr/results/*.sca
```

### Expected Output Summary

```
FullBsrAccounting-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   3
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  520
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count        743

StaleBsr-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   373
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  103
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count        1029

ImplicitBsr-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   0
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  3
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count        1419
```

### Interpretation of Results

1. **Explicit BSRP Polling Overhead**:
   - In `ImplicitBsr`, the AP sends **0 BSRP Triggers**. Because the STAs transmit BSRs implicitly inside uplink SU data frames during their EDCA transmit opportunities, the AP's coordinator receives regular backlog updates without having to explicitly query the STAs.
   - In `FullBsrAccounting`, the AP sends **3 BSRP Triggers**.
   - In `StaleBsr`, the count dramatically rises to **373 BSRP Triggers**. Because the queue status records expire in just 1 ms, the AP's scheduler is constantly forced to send BSRP trigger polls to check the STAs' queue status.

2. **Trigger-Based Scheduling vs. EDCA Throughput**:
   - `ImplicitBsr` delivers **1419 packets** to the server, which is the highest throughput among the three configurations. By scheduling trigger checks at 0.5s intervals, STAs make heavy use of standard SU EDCA channel access, avoiding the overhead of trigger frame sequences, Multi-STA BlockAck responses, and padding.
   - `FullBsrAccounting` delivers **743 packets** and sends **520 Basic Triggers**. The constant polling and triggering overhead at 1ms check intervals limits the aggregate channel capacity, resulting in lower throughput than SU EDCA.
   - `StaleBsr` delivers **1029 packets**. Because it spends a significant amount of time in BSRP/EDCA cycles due to the fast expiration of BSR reports, it sends fewer Basic Triggers (103) and relies more on EDCA, producing intermediate throughput.

---

## Detailed Explanation of Control Frame and Block Ack Handling

During simulation runs under high traffic demand (specifically in scenarios like `StaleBsr`), the stations (STAs) and the Access Point (AP) interact using advanced 802.11ax control frame exchanges (such as Multi-User Block Ack Requests and Multi-STA Block Acks). The base class implementation (`Hcf`) was originally designed for older standards (802.11e/n/ac) and lacked support for these ax-specific control frame sequences, causing simulation crashes. 

The two overridden methods in `HeHcf` resolve these issues:

### 1. Bypassing Transmitted Block Ack Responses (`HeHcf::originatorProcessTransmittedControlFrame`)
* **The Context**: When the AP sends a Multi-User Block Ack Request (MU-BAR, a type 2 Trigger frame) to a STA, the STA is standard-required to respond with a Block Ack frame (specifically a `Ieee80211BasicBlockAck` frame) within an HE Trigger-Based PPDU response.
* **The Problem**: When the STA transmits this Block Ack response, the TX completion handler calls `Hcf::originatorProcessTransmittedFrame()`. Because it is a control frame, HCF routes it to `Hcf::originatorProcessTransmittedControlFrame()`. However, the base HCF only expects to route control frames that *request* a response (such as `Ieee80211BlockAckReq` or `Ieee80211RtsFrame`) so it can set up wait states or retry recovery. Since the Block Ack is a final response, HCF did not have a matching condition and threw `cRuntimeError("Unknown control frame")`.
* **The Solution**: We overrode `HeHcf::originatorProcessTransmittedControlFrame` to catch `Ieee80211BlockAck` frames and return immediately. This safely bypasses HCF's request-recovery tracking, as a Block Ack response is a terminal frame and does not require further SIFS acknowledgments.

### 2. Translating Received Multi-STA Block Acks (`HeHcf::originatorProcessReceivedFrame`)
* **The Context**: In 802.11ax, the AP frequently aggregates acknowledgments for multiple uplink stations into a single `Ieee80211MultiStaBlockAck` frame, even for single-user uplink data transmitted via EDCA.
* **The Problem**: When a STA wins EDCA contention, transmits a block of QoS Data frames, and receives a `Multi-STA Block Ack` in response, the STA processes the response frame via `Hcf::originatorProcessReceivedFrame()`. This method routes received control frames to `Hcf::originatorProcessReceivedControlFrame()`. Since the base HCF only understands traditional `Ieee80211BasicBlockAck` frames, receiving the new `Ieee80211MultiStaBlockAck` frame caused it to throw `cRuntimeError("Unknown control frame")`.
* **The Solution**: We overrode `HeHcf::originatorProcessReceivedFrame` to intercept incoming `Ieee80211MultiStaBlockAck` frames. The override implements a surgical translation layer:
  1. It loops through the records inside the Multi-STA Block Ack to locate the record that matches the STA's Association ID (`myAid`).
  2. If a matching record is found and has a successful response status, it dynamically instantiates a dummy standard `Ieee80211BasicBlockAck` frame.
  3. It populates this dummy frame with the AP's transmitter address, the STA's receiver address, the target TID, and the record's starting sequence number.
  4. It maps the 64-bit block acknowledgment bitmap from the Multi-STA record onto the dummy Basic Block Ack's bitmap structure.
  5. It passes this dummy basic Block Ack to HCF's existing `originatorProcessReceivedControlFrame` method. This allows the baseline `QosAckHandler` and `OriginatorBlockAckAgreementHandler` to cleanly process the bitmap, mark the outstanding QoS data packets as acknowledged, and advance their sequence windows without requiring any invasive refactoring of the base classes.
