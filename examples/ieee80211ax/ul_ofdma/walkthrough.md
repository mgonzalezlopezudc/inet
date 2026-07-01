# 802.11ax Uplink OFDMA (UL OFDMA) Simulation

This example illustrates the Uplink Multi-User Orthogonal Frequency Division Multiple Access (UL MU-OFDMA) and Uplink OFDMA Random Access (UORA) mechanisms introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how the Access Point (AP) coordinates concurrent uplink transmissions from multiple stations (STAs) using Trigger frames, and compares scheduled access against random-access contention.

## Background: UL MU-OFDMA & UORA

In legacy 802.11 standards, all uplink traffic is sent using single-user CSMA/CA (EDCA), where stations contend individually for the medium. In high-density environments, this leads to frequent collisions and high overhead.

802.11ax introduces **Uplink MU-OFDMA** to allow multiple stations to transmit concurrently to the AP on partitioned subchannels called **Resource Units (RUs)**:
1. **Trigger Frames**: Uplink OFDMA is AP-controlled. The AP transmits a **Basic Trigger frame** to specify which stations can transmit, which RUs they should use, and their transmission parameters (power, MCS, duration).
2. **Uplink Trigger-Based (TB) PPDU**: The targeted stations receive the Trigger frame and transmit their uplink data frames simultaneously as **HE TB PPDUs**, aligning their starts exactly in time.
3. **Scheduled vs. Random Access**:
   - **Scheduled RUs**: Assigned directly to a specific station (AID) by the AP's Uplink Scheduler (e.g., `HeUlSchedulerBacklogBased`).
   - **Random Access RUs (UORA)**: Assigned to AID 0. Any associated station can contend for these RUs using a special **OFDMA Backoff (OBO)** counter, which decrements with each UORA RU received. UORA allows stations with new traffic to quickly transmit a Buffer Status Report (BSR) or small data packets to the AP.
4. **Multi-TID Block Ack**: Enables the AP to acknowledge data frames belonging to multiple Traffic Identifiers (TIDs) in a single response block.

---

## Network Topology

The network [Lan80211AxUlOfdma.ned](Lan80211AxUlOfdma.ned) consists of:
- **`ap`**: An Access Point located at `(25, 25)` on a 50m x 50m area.
- **`host[0..2]`**: Three wireless stations situated around the AP at close range (5 meters).
- **`server`**: A wired server connected to the AP via 100M Ethernet.
- **Traffic**: Each host generates heavy uplink UDP traffic destined for the `server` (400B packets sent every 0.4ms, starting at `0.2s`).

```
       [host[0]]   [host[1]]   [host[2]]
           \           |           /
            \          | (5m wireless)
             v         v         v
                    [ ap ]
                      |
                      | (100M Ethernet)
                      v
                  [server]
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines several scenarios to show different scheduler behaviors:

### 1. `General` (Default)
- Uplink OFDMA is enabled (`enableUlMuOfdma = true`).
- The AP uses `HeUlSchedulerBacklogBased`.
- UORA is enabled with 1 to 3 random-access RUs (`minRandomAccessRus = 1`, `maxRandomAccessRus = 3`).

### 2. `ScheduledOnly`
- Random access (UORA) is disabled (`minRandomAccessRus = 0`, `maxRandomAccessRus = 0`).
- The AP only schedules explicit RUs for stations with known backlog reports.

### 3. `MixedUora`
- Activates UORA (1 to 3 RUs) and limits the scheduled multi-user stations to 2 (`maxMuStations = 2`).

### 4. `EqualRus`
- Changes the scheduler type to `HeUlSchedulerEqualSizedRUs` which partitions the channel into equal-sized subchannels.

### 5. `MultiTidBlockAck`
- Enables negotiation of Multi-TID Aggregation support (`heMultiTidAggregationRx = true`, `heMultiTidAggregationTx = true`).

---

## Running the Simulation

Ensure your environment is set up, then run the simulations.

### Running with Qtenv (GUI)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv
opp_run -u Qtenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c General examples/ieee80211ax/ul_ofdma/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv

opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c General examples/ieee80211ax/ul_ofdma/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c ScheduledOnly examples/ieee80211ax/ul_ofdma/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c MixedUora examples/ieee80211ax/ul_ofdma/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c EqualRus examples/ieee80211ax/ul_ofdma/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c MultiTidBlockAck examples/ieee80211ax/ul_ofdma/omnetpp.ini
```

---

## Verifying Results

After running the simulations, extract the total packets received at the server and the count of Trigger frames sent by the AP.

```sh
# Query total received packets at the server
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.server.app*"' examples/ieee80211ax/ul_ofdma/results/*.sca

# Query Trigger frames (BSRP and Basic) sent by the AP
opp_scavetool query -l -f 'name =~ "heUlBsrpTriggerSent:count" or name =~ "heUlBasicTriggerSent:count"' examples/ieee80211ax/ul_ofdma/results/*.sca
```

### Expected Output Summary

```
General-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        979
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   3
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  427

MixedUora-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        126
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   103
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  111

ScheduledOnly-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        2085
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   64
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  0

EqualRus-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        2085
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   64
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  0

MultiTidBlockAck-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        2085
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   64
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  0
```

---

## Interpretation of Results

1. **Active Uplink MU-OFDMA Scheduling (`General`)**:
   - Under the `General` config, the AP actively coordinates uplink transmissions using **427 Basic Triggers** and **3 BSRP Triggers**.
   - The server successfully receives **979 packets**.
   - *Why?* The trigger-frame exchange introduces handshake overhead, and splitting the 20 MHz channel into small RUs reduces the peak bitrate per user. However, this ensures collision-free, synchronized uplink access, which is highly beneficial in environments with hundreds of active stations.

2. **The EDCA Fallback (`ScheduledOnly`, `EqualRus`, `MultiTidBlockAck`)**:
   - In these configurations, the AP sends **0 Basic Triggers** (no actual Uplink OFDMA scheduling occurs), although it sends **64 BSRP Triggers** to poll for queue status.
   - Consequently, the stations fallback to standard **CSMA/CA (EDCA)** for their data transmissions.
   - The server receives **2085 packets** (a much higher throughput).
   - *Why?* With only 3 stations at close range (5m) and zero channel interference, CSMA/CA operates at near-perfect efficiency without collisions. Bypassing the Trigger frame coordination overhead allows the stations to transmit at full single-user speeds (14.6 Mbps), maximizing throughput.

3. **High Contention Overhead (`MixedUora`)**:
   - In `MixedUora`, the AP allocates most RUs for Random Access (UORA).
   - The AP sends **111 Basic Triggers** and **103 BSRP Triggers**.
   - The server receives only **126 packets** (severe throughput drop).
   - *Why?* The stations are in a highly saturated traffic state (sending every 0.4ms). Contending via UORA causes high collision rates on the random-access RUs. The AP is forced to spend significant airtime sending BSRP polls and triggers to resolve contentions, resulting in severe overhead and packet loss.
