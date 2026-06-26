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

The [omnetpp.ini](omnetpp.ini) file defines two test scenarios:

### 1. `FullBsrAccounting` (Default/Baseline)
- The BSR freshness timer (`reportMaxAge`) uses the default (retained longer).
- The AP relies on fresh queue reports delivered implicitly or from previous polls to schedule UL MU-OFDMA data transmissions using `HeUlSchedulerBacklogBased`.
- **Result**: Minimizes explicit BSRP polling overhead, saving channel airtime for actual payload delivery.

### 2. `StaleBsr`
- The queue report freshness threshold is set to a very low value: `**.ap.wlan[*].mac.hcf.ulCoordinator.reportMaxAge = 1ms`.
- Since queue statuses expire in 1ms, the AP's scheduler frequently finds the records stale and is forced to send additional BSRP Trigger frames to re-verify backlogs.
- **Result**: Higher control overhead (more BSRP Triggers) and slightly lower packet throughput.

---

## Running the Simulation

Ensure your environment is set up and compiled, then run the simulations.

### Running with Qtenv (GUI)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv
opp_run -u Qtenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c FullBsrAccounting examples/ieee80211/he_bsr/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv

# Run Full BSR Accounting
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c FullBsrAccounting examples/ieee80211/he_bsr/omnetpp.ini

# Run Stale BSR Scenario
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c StaleBsr examples/ieee80211/he_bsr/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to analyze how many BSRP polling triggers were sent by the AP and the total packets received at the server.

```sh
# Query the number of BSRP Trigger frames sent by the AP
opp_scavetool query -l -f 'name =~ "heUlBsrpTriggerSent:count" and module =~ "*.ap.*"' examples/ieee80211/he_bsr/results/*.sca

# Query the total packets received at the UDP sink on the server
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.server.*"' examples/ieee80211/he_bsr/results/*.sca
```

### Expected Output Summary

```
FullBsrAccounting-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count  373
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count       1183

StaleBsr-#0.sca:
scalar  HeBsrNetwork.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count  457
scalar  HeBsrNetwork.server.app[0]                     packetReceived:count       1159
```

### Interpretation of Results

1. **BSRP Trigger Count**:
   - In `FullBsrAccounting`, the AP sends **373 BSRP Triggers**.
   - In `StaleBsr`, this count increases by about **22.5% to 457 BSRP Triggers**.
   - *Why?* A `reportMaxAge` of 1ms forces the AP to continuously doubt its queue status knowledge and repeatedly query the STAs, generating significant control frame overhead.

2. **Server Packet Delivery**:
   - `FullBsrAccounting` successfully delivers **1183 packets** to the server.
   - `StaleBsr` delivers **1159 packets** (a small but clear degradation).
   - *Why?* The extra airtime consumed by the additional BSRP triggers leaves less time for UL data frames, thereby reducing the aggregate throughput.
