# 802.11be Multi-Link Operation (MLO) Example

This example demonstrates the marquee feature of Wi-Fi 7: Multi-Link Operation (MLO). The network leverages the `Ieee80211MldMac` to seamlessly manage and distribute traffic across multiple independent wireless links simultaneously.

## Topology

The topology features a multi-link access point (`MldAp`) and a multi-link station (`MldHost`), connected to a wired backend server.
Instead of standard single-radio interfaces, these custom nodes employ an `MldInterface` compound module. This interface integrates a single upper MLD MAC (`Ieee80211MldMac`) and two lower independent 802.11 MAC/PHY instances (referred to as `link[0]` and `link[1]`).

## Configurations

The `omnetpp.ini` establishes a 5 GHz link (160 MHz width) and a 6 GHz link (320 MHz width) between the AP and the STA. A single, high-bandwidth UDP stream is sent from the server to the STA. The MLD MAC handles queueing the incoming packets and dynamically steering them to the lower links as transmission opportunities arise.

There are two primary configurations to evaluate different MLO paradigms:

1. **`Str` (Simultaneous Transmit and Receive):** In this mode, both the 5 GHz and 6 GHz links operate entirely independently. The device can transmit on the 6 GHz link while simultaneously receiving on the 5 GHz link without causing internal interference. This maximizes aggregated throughput.
2. **`Nstr` (Non-Simultaneous Transmit and Receive):** In this mode, the device's radios lack the isolation required for STR. Consequently, transmissions and receptions across the links must be carefully synchronized by the MLD MAC to prevent self-interference, leading to different channel access patterns and slightly constrained throughput compared to STR.

## Simulation Results and Analysis (with Multi-Link Scheduler)

A round-robin packet scheduler was implemented in `Ieee80211MldMac::handleUpperMessage` to utilize both links. Additionally, the queues of the links were decoupled by setting `**.link[*].mac.hcf.edca.edcaf[*].pendingQueueModule = ""` in `omnetpp.ini` to avoid cross-link destination address mismatches.

Both configurations were executed to completion (1.0s simulation time) and yielded the following metrics:

### Key Metrics

| Metric | `Str` Configuration | `Nstr` Configuration |
|---|---|---|
| **UDP Packets Sent (Server)** | 90,001 | 90,001 |
| **UDP Packets Received (Host)** | 18,251 | 18,251 |
| **Packet Delivery Ratio** | ~20.28% | ~20.28% |
| **Link 0 (5 GHz) Transmissions** | 97 frame sequences | 97 frame sequences |
| **Link 1 (6 GHz) Transmissions** | 190 frame sequences | 190 frame sequences |

### Analysis & Observations

1. **Successful Multi-Link Utilization**:
   - The round-robin scheduler successfully alternates packet transmissions across both links. 
   - Decoupling the pending queues ensures that each lower MAC interface manages its own frame transmissions, eliminating packet drops due to cross-link destination address mismatch.
   - Total host reception increased significantly from **6,166 packets** (single-link baseline) to **18,251 packets** (multi-link operation).

2. **Identical STR vs. NSTR Behavior**:
   - The results for the `Str` and `Nstr` configurations remain identical.
   - Analysis of the C++ code reveals that while the `ehtStr` and `ehtNstr` flags are parsed and exchanged during capability negotiation, INET does not currently implement runtime scheduling/interference constraints or self-interference simulation for NSTR.

3. **Required Fixes for Execution**:
   - The AP compound module links were modified to disable the station agent (`mldWlan.link[*].agent.typename = ""`) to prevent initialization crashes.


