# 802.11ax HE Target Wake Time (TWT) Simulation

This example illustrates the Target Wake Time (TWT) power-saving mechanism introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how TWT agreements are negotiated between an Access Point (AP) and stations (STAs), and how individual and broadcast TWT configurations allow stations to sleep, reducing power consumption.

## Background: HE Target Wake Time (TWT)

In legacy 802.11 power-saving modes (like PS-Poll or APSD), stations wake up periodically to receive Beacons and check if the AP has buffered downlink traffic. In high-density networks, this causes all sleeping stations to wake up simultaneously, leading to high collision rates and increased energy waste.

802.11ax addresses this with **Target Wake Time (TWT)**:
1. **Scheduled Wakeup**: Instead of waking up for every Beacon, the AP and each STA negotiate specific, customized time windows called **TWT sessions** (defined by `wakeInterval` and `wakeDuration`). The STA remains in a deep sleep state outside of these sessions.
2. **Individual TWT**:
   - Negotiated dynamically via TWT Setup frames.
   - **Unannounced TWT**: The STA is not required to announce that it is awake
     before exchanging frames during the service period.
   - **Announced TWT**: The STA indicates that it is awake before the peer sends
     it frames. “Announced” describes presence signaling, not a general rule
     that the STA must remain silent until an AP poll.
3. **Broadcast TWT**:
   - The AP defines and broadcasts a shared TWT schedule (in Beacons or Association responses).
   - Multiple stations join this shared broadcast schedule, allowing the AP to coordinate groups of STAs together (highly useful for Downlink/Uplink MU-OFDMA).

---

## Network Topology

The network [TwtRegression.ned](TwtRegression.ned) consists of:
- **`ap`**: An Access Point located at `(300, 180)`.
- **`sta[0..1]`**: Two wireless stations located at `(250, 150)` and `(250, 210)` on strong links to the AP.
- **`server`**: A wired server connected to the AP.
- **Traffic**: Each station generates 200-byte uplink UDP packets for the wired server every 2 s from 10–90 s. The remaining 10 s drains queued traffic.

```
       [sta[0]]
          |
          | (58m wireless)
          v
       [ ap ] <==== (100G Ethernet) ====> [server]
          ^
          | (58m wireless)
          |
       [sta[1]]
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines four TWT scenarios:

### 1. `Baseline`
- TWT is disabled: `**.wlan[*].twt.enabled = false`.
- Stations do not sleep and remain awake for the entire duration of the simulation.

### 2. `IndividualUnannounced`
- TWT is enabled: `**.wlan[*].twt.enabled = true`.
- Stations negotiate an **Individual, Unannounced** TWT schedule:
  - `*.sta[*].wlan[*].agent.requestBroadcast = false`
  - `*.sta[*].wlan[*].agent.announced = false`
  - Wake interval is set to `100 ms` and wake duration to `90 ms`.
- **Result**: Stations enter a low-power sleep state outside their wake windows.

### 3. `IndividualAnnounced`
- Extends the `IndividualUnannounced` configuration but sets `announced = true`.
- Announced mode changes presence signaling; it is not a general requirement
  to wait for an AP poll before every exchange.

### 4. `Broadcast`
- Stations join a **Broadcast** TWT schedule created by the AP:
  - `*.ap.wlan[*].mgmt.createBroadcastSchedule = true` and `*.ap.wlan[*].mgmt.broadcastId = 1`
  - `*.sta[*].wlan[*].agent.requestBroadcast = true` and `*.sta[*].wlan[*].agent.broadcastId = 1`

---

## Running the Simulation

From the INET project root, use the project launcher.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c IndividualUnannounced examples/ieee80211ax/twt/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
bin/inet -u Cmdenv -c Baseline examples/ieee80211ax/twt/omnetpp.ini
bin/inet -u Cmdenv -c IndividualUnannounced examples/ieee80211ax/twt/omnetpp.ini
bin/inet -u Cmdenv -c IndividualAnnounced examples/ieee80211ax/twt/omnetpp.ini
bin/inet -u Cmdenv -c Broadcast examples/ieee80211ax/twt/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to extract TWT agreement counts and sleep time statistics for each station.

```sh
# Query TWT agreements, schedules, awake time, and sleep time
opp_scavetool query -l -f 'name =~ "twtAgreementCount" or name =~ "twtBroadcastScheduleCount" or name =~ "twtAwakeTime" or name =~ "twtSleepTime"' examples/ieee80211ax/twt/results/*.sca
```

### Expected Output Summary

```
Baseline-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          0
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               0

IndividualUnannounced-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          2
scalar  TwtRegression.sta[0].wlan[0].twt  twtAgreementCount          1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               89.3788 s
scalar  TwtRegression.sta[0].wlan[0].twt  twtAwakeTime               10.6212 s

IndividualAnnounced-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          2
scalar  TwtRegression.sta[0].wlan[0].twt  twtAgreementCount          1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               89.3788 s
scalar  TwtRegression.sta[0].wlan[0].twt  twtAwakeTime               10.6212 s

Broadcast-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtBroadcastScheduleCount  1
scalar  TwtRegression.sta[1].wlan[0].twt  twtBroadcastScheduleCount  1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               78.8487 s
scalar  TwtRegression.sta[0].wlan[0].twt  twtAwakeTime               21.1513 s
```

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c IndividualUnannounced examples/ieee80211ax/twt/omnetpp.ini --result-dir=examples/ieee80211ax/twt/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/twt/results/IndividualUnannounced-#0TwtRegression.ap.wlan[0].pcap -c 26
```

The decoded output timeline shows:
1. **Network Entry**: The client stations send Probe Requests, Authenticate, and Associate with the AP (e.g. frames 2, 3, 11, 19).
2. **TWT Negotiation**: After association, client stations send 802.11 Action frames carrying TWT Setup Requests (e.g. frame 23), and the AP responds with Action frames containing TWT Setup Responses (Accept) (e.g. frame 25) confirming the wake schedule.
3. **Power Saving sleep state**: Once negotiated, the client stations remain in deep sleep outside their wake service periods.

---

## Interpretation of Results

1. **Agreement Counts**:
   - In `Baseline`, no TWT agreements are formed.
   - In `IndividualUnannounced` and `IndividualAnnounced`, the AP negotiates **2 agreements** (1 with each STA), and both STAs report `twtAgreementCount = 1`.
   - In `Broadcast`, the AP and STAs register **1 Broadcast Schedule** (`twtBroadcastScheduleCount = 1`).

2. **Sleep Duration**:
   - In `Baseline`, the stations sleep for **0 seconds** because TWT is disabled.
   - The configured individual schedule requests a 90 ms service period every 100 ms. Exact awake and sleep durations are result metrics, not fixed expectations; random seeds and management synchronization can change them.
   - *Why use a high duty cycle?* The regression requires every treatment seed to preserve at least 95% of paired-baseline delivery. This schedule retains measurable sleep intervals while prioritizing reliable delivery of the periodic uplink workload.
   - Broadcast and individual schedules can have different awake time because beacon synchronization and schedule maintenance add overhead.
   - Awake/sleep time demonstrates scheduled radio availability. The configured radio energy consumer and battery make the corresponding energy cost measurable; compare consumed energy together with delivery and latency. Sleep fraction alone is not an energy result, and this workload is not a general estimate of real-device battery lifetime.

3. **Energy advantage with delivery preserved**:
   - Across five paired seeds, TWT delivers `16,000 B` versus
     `15,960 ± 111 B` for the baseline, satisfying the 95% delivery gate.
     Energy per delivered bit falls from `2.85e-6 J/bit` to
     `1.56e-6 J/bit`, about a 45% reduction.
   - The `90 ms` service period is deliberately conservative. It leaves only a
     modest nominal sleep opportunity, but keeps the periodic uplink workload
     reliable. A shorter service period could save more energy while increasing
     queueing delay or loss; that is the trade-off TWT exposes.

## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from the Access Point's wireless interface (`ap.wlan[0]`), which captures all uplink, downlink, and management traffic in the BSS without duplication.

> **HE capture metadata caveat:** The current INET `PcapRecorder` uses a repository-specific packing for HE radiotap metadata. TShark can consequently decode SU transmissions as HE ER SU and downlink HE MU transmissions as HE TB. Frame type, subtype, count, and size remain useful, but the HE PPDU-format, MCS, bandwidth, GI, NSS, and coding suffixes—and the airtime estimates derived from them—are diagnostic only and are not standards-conformance evidence.

Two airtime occupancy percentages are provided:
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Configuration: `Baseline`
Total over-the-air packets captured (Global BSS/AP): **2377**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c019" /></svg> | Data: QoS Data [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 1303 | 54.82% | 266.9 B | 1.7 B | 141.5 us | 0.1 us | 5010 MHz | -69.0 dBm | - | 55.60% | 0.18% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#be6237" /></svg> | Control: Block Ack Request (BAR) [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 34 | 1.43% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.29% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#308ef3" /></svg> | Control: Ack [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 20 | 0.84% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.15% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#930117" /></svg> | Management: Beacon [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 1000 | 42.07% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 43.42% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f97443" /></svg> | Management: Probe Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.08% | 68.0 B | 0.0 B | 110.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.07% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fb9874" /></svg> | Management: Probe Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.08% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.09% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f218ca" /></svg> | Management: Association Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.08% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.07% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d235ed" /></svg> | Management: Association Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.08% | 81.0 B | 0.0 B | 128.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.08% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9439b" /></svg> | Management: Authentication [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.34% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.16% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e90b07" /></svg> | Management: Action [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 4 | 0.17% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.08% | 0.00% |

### Configuration: `Broadcast`
Total over-the-air packets captured (Global BSS/AP): **1174**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c019" /></svg> | Data: QoS Data [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 93 | 7.92% | 269.7 B | 1.1 B | 141.7 us | 0.1 us | 5010 MHz | -69.0 dBm | - | 7.13% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#be6237" /></svg> | Control: Block Ack Request (BAR) [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 17 | 1.45% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.26% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#12268c" /></svg> | Control: Block Ack (BA) [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 16 | 1.36% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.27% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#308ef3" /></svg> | Control: Ack [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 24 | 2.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.32% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#930117" /></svg> | Management: Beacon [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 1000 | 85.18% | 111.0 B | 0.0 B | 168.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 90.87% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f97443" /></svg> | Management: Probe Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 68.0 B | 0.0 B | 110.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.12% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fb9874" /></svg> | Management: Probe Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.16% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f218ca" /></svg> | Management: Association Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.13% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d235ed" /></svg> | Management: Association Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 81.0 B | 0.0 B | 128.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.14% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9439b" /></svg> | Management: Authentication [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.68% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.28% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e90b07" /></svg> | Management: Action [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.68% | 42.5 B | 5.5 B | 76.7 us | 7.3 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.33% | 0.00% |

### Configuration: `IndividualAnnounced`
Total over-the-air packets captured (Global BSS/AP): **4572**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c019" /></svg> | Data: QoS Data [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 1532 | 33.51% | 266.9 B | 1.7 B | 141.5 us | 0.1 us | 5010 MHz | -69.0 dBm | - | 52.03% | 0.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#6ebced" /></svg> | Control: PS-Poll [HE-ER-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 1992 | 43.57% | 20.0 B | 0.0 B | 26.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 12.75% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#308ef3" /></svg> | Control: Ack [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 24 | 0.52% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.14% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#930117" /></svg> | Management: Beacon [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 1000 | 21.87% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 34.56% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f97443" /></svg> | Management: Probe Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.04% | 68.0 B | 0.0 B | 110.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.05% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fb9874" /></svg> | Management: Probe Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.04% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.07% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f218ca" /></svg> | Management: Association Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.04% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.06% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d235ed" /></svg> | Management: Association Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.04% | 81.0 B | 0.0 B | 128.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.06% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9439b" /></svg> | Management: Authentication [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.17% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.13% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e90b07" /></svg> | Management: Action [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.17% | 42.5 B | 5.5 B | 76.7 us | 7.3 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.15% | 0.00% |

### Configuration: `IndividualUnannounced`
Total over-the-air packets captured (Global BSS/AP): **1156**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c019" /></svg> | Data: QoS Data [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 80 | 6.92% | 270.0 B | 0.0 B | 141.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 7.14% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#be6237" /></svg> | Control: Block Ack Request (BAR) [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 14 | 1.21% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.25% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#12268c" /></svg> | Control: Block Ack (BA) [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 14 | 1.21% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.27% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#308ef3" /></svg> | Control: Ack [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 24 | 2.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.37% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#930117" /></svg> | Management: Beacon [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 1000 | 86.51% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 90.63% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f97443" /></svg> | Management: Probe Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 68.0 B | 0.0 B | 110.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.14% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fb9874" /></svg> | Management: Probe Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 93.0 B | 0.0 B | 144.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.18% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f218ca" /></svg> | Management: Association Request [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.15% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d235ed" /></svg> | Management: Association Response [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 2 | 0.17% | 81.0 B | 0.0 B | 128.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.16% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9439b" /></svg> | Management: Authentication [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.69% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.33% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e90b07" /></svg> | Management: Action [HE-ER-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 8 | 0.69% | 42.5 B | 5.5 B | 76.7 us | 7.3 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.39% | 0.00% |

### Analysis of Packet Distribution
Only `IndividualAnnounced` contains the large **PS-Poll** population. This is consistent with the announced-TWT procedure: the requester signals that it is awake with PS-Poll or an APSD trigger before the responder sends a non-Trigger frame (IEEE Std 802.11-2024, Table 9-347 and Clause 10.46). Unannounced TWT does not require that presence signal. The QoS Data totals are transmitted MPDU observations, not delivered application-packet counts; aggregation and repeated sequence numbers can make them much larger than the workload. Validate TWT delivery with sink scalars and energy with the recorded radio-power vectors.
