# 802.11ax HE Rate Adaptation Simulation

This example illustrates the High Efficiency (HE) Rate Adaptation mechanisms in the IEEE 802.11ax (Wi-Fi 6) standard. It showcases the difference between a static SNR-based MCS selection fallback and the dynamic, feedback-driven **HE Minstrel** rate adaptation algorithm in a multi-user downlink scheduling scenario.

## Background: HE Rate Selection & Minstrel Rate Control

Selecting the optimal Modulation and Coding Scheme (MCS) is critical in wireless networks. A rate too high causes packet corruption, while a rate too low wastes channel airtime. In 802.11ax, this choice is further integrated with the downlink scheduler:

1. **Static SNR-Based Fallback (SNR Mapping)**:
   - When no dynamic rate control is active, the HCF Downlink Scheduler uses a static mapping.
   - It estimates the path loss to the destination, calculates the expected SNR, and matches it against a configured table of thresholds (`heMcsSnrThresholds` parameter, default: `"4 7 10 13 16 19 21 24 27 30 33 36"` dB for MCS 0..11).

2. **HE Minstrel Rate Control (`HeMinstrelRateControl`)**:
   - Inspired by the classic Minstrel algorithm for legacy 802.11, this is a feedback-driven rate controller.
   - It maintains an **Exponentially Weighted Moving Average (EWMA)** of frame transmission success probabilities for each peer station and each possible MCS/NSS combination.
   - It periodically schedules **probe frames** (controlled by `lookaroundRatio`) to test other rates, dynamically adapting to actual link conditions (such as fading, shadowing, and collision levels) based on received ACKs and Block ACKs.

---

## Network Topology

The network [HeRateAdaptationNetwork.ned](HeRateAdaptationNetwork.ned) consists of:
- **`ap`**: An Access Point located at `(320, 210)`.
- **`host[0..3]`**: Four wireless stations placed at varying distances from the AP:
  - `host[0]` at 70m (`(250, 210)`) -> closest, high SNR.
  - `host[1]` at 130m (`(200, 160)`).
  - `host[2]` at 177m (`(150, 260)`).
  - `host[3]` at 230m (`(90, 210)`) -> furthest, low SNR.
- **`server`**: A wired server connected to the AP.
- **Traffic**: Downlink UDP traffic is sent from the `server` to each of the four hosts via the AP (900B packets sent every 0.35ms).

```
  [host[3]]       [host[2]]       [host[1]]       [host[0]]      [ap] <== (wired) ==> [server]
    230m            177m            130m            70m
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines three scenarios:

### 1. `FixedMcs` (Baseline)
- The AP's HCF Downlink Scheduler does not use a dynamic rate control module.
- Instead, it falls back to the static path-loss SNR mapping to choose the transmission MCS for each station.

### 2. `HeMinstrel`
- Dynamic HE Minstrel rate control is enabled on the AP:
  - `**.ap.wlan[*].mac.hcf.rateControl.typename = "HeMinstrelRateControl"`
  - `**.ap.wlan[*].mac.hcf.rateControl.minMcs = 0`
  - `**.ap.wlan[*].mac.hcf.rateControl.maxMcs = 11`
  - `**.ap.wlan[*].mac.hcf.dlScheduler.heRateControlModule = "^.rateControl"`
- **Result**: The Downlink Scheduler queries the `HeMinstrelRateControl` module to select the optimal MCS/NSS dynamically for each peer, updating its selection based on ACK success rates.

### 3. `HeMinstrelMobile`

- Extends `HeMinstrel` and changes only `host[3]` to `LinearMobility` at 8 m/s.
- This is the didactically useful adaptation case: the link budget changes
  during the run, so inspect the selected-rate vector over simulation time
  rather than comparing only its mean or the final packet count.

---

## Running the Simulation

From the INET project root, use the project launcher.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c HeMinstrel examples/ieee80211ax/he_rate_adaptation/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
# Run FixedMcs Baseline
bin/inet -u Cmdenv -c FixedMcs examples/ieee80211ax/he_rate_adaptation/omnetpp.ini

# Run HeMinstrel Config
bin/inet -u Cmdenv -c HeMinstrel examples/ieee80211ax/he_rate_adaptation/omnetpp.ini

# Run Minstrel with a moving edge station
bin/inet -u Cmdenv -c HeMinstrelMobile examples/ieee80211ax/he_rate_adaptation/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to analyze the received packets at the hosts and the selected transmission bitrates of the AP.

```sh
# Query the total packets received at the UDP applications on host[0..3]
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.host*app*"' examples/ieee80211ax/he_rate_adaptation/results/*.sca

# Query the selected datarate statistics for transmissions by the AP HCF
opp_scavetool query -l -f 'name =~ "datarateSelected:vector"' examples/ieee80211ax/he_rate_adaptation/results/*.vec
```

### Expected Output Summary

```
FixedMcs-#0.sca:
scalar  HeRateAdaptationNetwork.host[0].app[0]  packetReceived:count  358
scalar  HeRateAdaptationNetwork.host[1].app[0]  packetReceived:count  358
scalar  HeRateAdaptationNetwork.host[2].app[0]  packetReceived:count  358
scalar  HeRateAdaptationNetwork.host[3].app[0]  packetReceived:count  358

HeMinstrel-#0.sca:
scalar  HeRateAdaptationNetwork.host[0].app[0]  packetReceived:count  1095
scalar  HeRateAdaptationNetwork.host[1].app[0]  packetReceived:count  1094
scalar  HeRateAdaptationNetwork.host[2].app[0]  packetReceived:count  161
scalar  HeRateAdaptationNetwork.host[3].app[0]  packetReceived:count  160
```

AP datarate vector summary:
- `FixedMcs`: count=1566, mean=17.04 Mbps, min=7.31 Mbps, max=121.87 Mbps
- `HeMinstrel`: count=2589, mean=53.85 Mbps, min=7.31 Mbps, max=121.87 Mbps

Individual Host HCF wlan[0] datarates:
- `FixedMcs` hosts mean datarate: `host[0]`: 118.30 Mbps, `host[1]`: 118.60 Mbps, `host[2]`: 118.94 Mbps, `host[3]`: 7.31 Mbps
- `HeMinstrel` hosts mean datarate: `host[0]`: 111.46 Mbps, `host[1]`: 108.19 Mbps, `host[2]`: 118.40 Mbps, `host[3]`: 17.39 Mbps

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c HeMinstrel examples/ieee80211ax/he_rate_adaptation/omnetpp.ini --result-dir=examples/ieee80211ax/he_rate_adaptation/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/he_rate_adaptation/results/HeMinstrel-#0HeRateAdaptationNetwork.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **Downlink UDP Packets**: The AP transmits UDP data packets (e.g. frames 1, 7, 13) to client hosts at distance-adapted MCS rates.
2. **Block Ack Negotiation**: Block ACK negotiation Action frames (e.g. frames 3, 5, 9, 11, 15) are exchanged between the AP and the client hosts to establish session block acknowledgments.
3. **Minstrel Dynamic Adaptation (HeMinstrelMobile)**: Under mobility (in `HeMinstrelMobile`), you can observe the AP dynamically reducing the transmission MCS and datarates for `host[3]` as it moves further away and its SNR decreases, maintaining high packet delivery success rates.

---

## Interpretation of Results

1. **Different Results under High Traffic Saturation**:
   - With the 100G Ethernet upgrade and dynamic queue clearing at the AP, the packet delivery numbers and selected bitrates differ between the static `FixedMcs` and dynamic `HeMinstrel` runs.
   - *Why?* While the channel remains static, the high offered traffic load saturates the queues. `HeMinstrel`'s lookaround probing and success estimation interact dynamically with the DL HCF scheduler, adapting the rates to clear queues of closer stations faster, which changes scheduling priority and results in higher aggregate throughput (2510 total packets vs 1432 for FixedMcs) at the expense of fairness for more distant stations.

2. **Datarate vs. Distance**:
   - The AP dynamically transmits to closer stations (like `host[0]`) at much higher bitrates (up to 121.87 Mbps, using high MCS) because of the strong signal.
   - For the cell-edge `host[3]` at 230m, the AP is forced to transmit at the robust base rate of 7.31 Mbps (MCS 0) to ensure the signal is decodable.

3. **Advantages of Minstrel in Dynamic Channels**:
   - While both algorithms perform identically in this static sandbox, in real-world scenarios with client mobility, blockages, or multi-user interference, a static SNR mapping is highly inaccurate or unavailable. In those conditions, Minstrel's adaptive feedback loop is essential to track link quality changes dynamically using transmission success history.
