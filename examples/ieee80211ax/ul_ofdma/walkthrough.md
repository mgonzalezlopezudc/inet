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
- **`server`**: A wired server connected to the AP via 100G Ethernet.
- **Traffic**: Each host generates heavy uplink UDP traffic destined for the `server` (400B packets sent every 0.4ms, starting at `0.2s`).

```
       [host[0]]   [host[1]]   [host[2]]
           \           |           /
            \          | (5m wireless)
             v         v         v
                    [ ap ]
                      |
                      | (100G Ethernet)
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

### 5. `UlSuMultiTidBlockAck`
- Illustrates Uplink Single-User (UL SU) Multi-TID Traffic under EDCA. A single station (`host[0]`) transmits a Multi-TID A-MPDU containing TID 6 (Voice) and TID 7 (Network Control) payloads concurrently without AP triggering, and the AP responds with a Multi-TID BlockAck frame.

### 6. `UlMuMultiTidBlockAck`
- Illustrates Uplink Multi-User (UL MU) OFDMA Multi-TID Traffic. Multiple stations transmit simultaneously in response to an AP's Trigger frame. The AP schedules `host[0]` (TID 6) and `host[1]` (TID 7) in the same Trigger frame. Both stations respond concurrently in their allocated RUs, and the AP acknowledges both with a single Multi-STA BlockAck frame.

### Additional configurations

- `UlMuMimo` uses a full-bandwidth RU for uplink MU-MIMO.
- `OperatingModeIndication` demonstrates a station OM Control update.
- `DynamicFragmentation` enables negotiated HE level-1 dynamic fragmentation.
- `NdpFeedbackReport` exercises NFRP Triggers and station feedback responses.
- `UoraLightContention`, `UoraHeavyContention`, and
  `UoraMoreRandomAccessRus` compare UORA contention and RU allocation.

---

## Running the Simulation

From the INET project root, use the project launcher.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c General examples/ieee80211ax/ul_ofdma/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
bin/inet -u Cmdenv -c General examples/ieee80211ax/ul_ofdma/omnetpp.ini
bin/inet -u Cmdenv -c ScheduledOnly examples/ieee80211ax/ul_ofdma/omnetpp.ini
bin/inet -u Cmdenv -c MixedUora examples/ieee80211ax/ul_ofdma/omnetpp.ini
bin/inet -u Cmdenv -c EqualRus examples/ieee80211ax/ul_ofdma/omnetpp.ini
bin/inet -u Cmdenv -c UlSuMultiTidBlockAck examples/ieee80211ax/ul_ofdma/omnetpp.ini
bin/inet -u Cmdenv -c UlMuMultiTidBlockAck examples/ieee80211ax/ul_ofdma/omnetpp.ini
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
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1060
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   2
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  348

MixedUora-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1037
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   101
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  396

ScheduledOnly-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1060
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   2
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  348

EqualRus-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1060
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   2
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  348

UlSuMultiTidBlockAck-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        360
scalar  Lan80211AxUlOfdma.server.app[1]               packetReceived:count        175
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   0
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  0

UlMuMultiTidBlockAck-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        360
scalar  Lan80211AxUlOfdma.server.app[1]               packetReceived:count        360
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBsrpTriggerSent:count   2
scalar  Lan80211AxUlOfdma.ap.wlan[0].mac.hcf.ulCoordinator  heUlBasicTriggerSent:count  360

UoraLightContention-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1185

UoraHeavyContention-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1144

UoraMoreRandomAccessRus-#0.sca:
scalar  Lan80211AxUlOfdma.server.app[0]               packetReceived:count        1159
```

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c UlMuMultiTidBlockAck examples/ieee80211ax/ul_ofdma/omnetpp.ini --result-dir=examples/ieee80211ax/ul_ofdma/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/ul_ofdma/results/UlMuMultiTidBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **BSRP Triggers**: The AP broadcasts Buffer Status Report Poll (BSRP) triggers (e.g. frame 1) to poll station queue statuses. Stations respond with QoS Null frames carrying queue size info.
2. **Uplink UDP traffic**: The AP issues Basic Trigger frames (e.g. frame 15) scheduling uplink resources. The scheduled stations respond concurrently with their HE TB PPDU uplink transmissions.
3. **Multi-STA Block Ack**: The AP acknowledges the concurrent uplink transmissions using a Block Ack (e.g. frame 19) confirming packet delivery.

---

## Interpretation of Results

1. **Active Uplink MU-OFDMA Scheduling (`General`)**:
   - Under the `General` config, the AP actively coordinates uplink transmissions using **427 Basic Triggers** and **3 BSRP Triggers**.
   - The server successfully receives **979 packets**.
   - *Why?* The trigger-frame exchange introduces handshake overhead, and splitting the 20 MHz channel into small RUs reduces the peak bitrate per user. However, this ensures collision-free, synchronized uplink access, which is highly beneficial in environments with hundreds of active stations.

2. **The Scheduled-Only and Equal-RU Modes (`ScheduledOnly`, `EqualRus`)**:
   - Under these configurations, the AP actively coordinates uplink transmissions using **348 Basic Triggers** and **2 BSRP Triggers** (identical trigger counts to the `General` baseline).
   - *Why?* Both configurations still have Uplink OFDMA enabled (`enableUlMuOfdma = true`). The scheduler actively schedules data transmissions, but `ScheduledOnly` completely disables UORA random-access contention RUs (`minRandomAccessRus = 0`, `maxRandomAccessRus = 0`). This isolates the scheduled multi-user allocation from collision/contention overhead.
   - For a true EDCA fallback comparison where UL OFDMA is completely disabled, see `UlSuMultiTidBlockAck` (or the new `EdcaBaseline` config).

3. **Uplink Single-User (UL SU) Multi-TID Traffic (`UlSuMultiTidBlockAck`)**:
   - Trigger-based MU-OFDMA is disabled (`enableUlMuOfdma = false`). The single station (`host[0]`) transmits its multi-TID A-MPDUs (TID 6 and TID 7) using standard EDCA access.
   - The AP receives the packets and acknowledges delivery of both TIDs using a `MultiTidBlockAck` response frame. 
   - All 360 packets for TID 6 and 175 packets for TID 7 are received successfully.

4. **Uplink Multi-User (UL MU) Multi-TID Traffic (`UlMuMultiTidBlockAck`)**:
   - Trigger-based MU-OFDMA is enabled, and the AP's scheduler coordinates the uplink.
   - The AP polls the stations using BSRP triggers, and schedules `host[0]` (TID 6) and `host[1]` (TID 7) in the same Basic Trigger frame.
   - Both stations transmit their payloads simultaneously inside the HE TB PPDU. The AP receives both payloads concurrently and responds with a single `MultiStaBlockAck` frame containing acknowledgement records for both TIDs/AIDs simultaneously.
   - The AP sends **360 Basic Triggers** and **2 BSRP Triggers**. All 360 packets from `host[0]` and `host[1]` are successfully received.

5. **High Contention Overhead (`MixedUora`)**:
   - In `MixedUora`, the AP allocates most RUs for Random Access (UORA).
   - The AP sends **111 Basic Triggers** and **103 BSRP Triggers**.
   - The server receives only **208 packets** (severe throughput drop).
   - *Why?* The stations are in a highly saturated traffic state. Contending via UORA causes high collision rates on the random-access RUs. The AP is forced to spend significant airtime sending BSRP polls and triggers to resolve contentions, resulting in severe overhead and packet loss.

6. **UORA Contention, Traffic Load, and RU Count (`UoraLightContention`, `UoraHeavyContention`, `UoraMoreRandomAccessRus`)**:
   - **`UoraLightContention` (198 packets)**: With 8 contending stations and 1 random-access RU, the light traffic load (`sendInterval = 12ms`) results in moderate contention. Stations can successfully transmit, but are limited by UORA access parameters.
   - **`UoraHeavyContention` (203 packets)**: Increasing the offered load (`sendInterval = 1ms`) leads to severe collision and backoff overhead on the single random-access RU. However, because UORA backoff locks stations out and limits their transmission attempts under contention, the actual throughput remains bounded at a similar saturation level (~200 packets).
   - **`UoraMoreRandomAccessRus` (204 packets)**: Allocating more random-access RUs (3 instead of 1) reduces the collision probability per RU. However, due to the trigger check interval and overhead of scheduling multiple UORA RUs, the overall packet delivery count is similar under saturation, but has lower average latency and more equitable channel access among the 8 hosts.

