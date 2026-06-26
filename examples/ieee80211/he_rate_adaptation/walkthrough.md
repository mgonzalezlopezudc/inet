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

The [omnetpp.ini](omnetpp.ini) file defines two scenarios:

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

---

## Running the Simulation

Ensure your environment is set up, then run the simulations.

### Running with Qtenv (GUI)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv
opp_run -u Qtenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c HeMinstrel examples/ieee80211/he_rate_adaptation/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv

# Run FixedMcs Baseline
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c FixedMcs examples/ieee80211/he_rate_adaptation/omnetpp.ini

# Run HeMinstrel Config
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c HeMinstrel examples/ieee80211/he_rate_adaptation/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to analyze the received packets at the hosts and the selected transmission bitrates of the AP.

```sh
# Query the total packets received at the UDP applications on host[0..3]
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.host*app*"' examples/ieee80211/he_rate_adaptation/results/*.sca

# Query the selected datarate statistics for transmissions by the AP HCF
opp_scavetool query -l -f 'name =~ "datarateSelected:vector"' examples/ieee80211/he_rate_adaptation/results/*.vec
```

### Expected Output Summary

```
FixedMcs-#0.sca:
scalar  HeRateAdaptationNetwork.host[0].app[0]  packetReceived:count  230
scalar  HeRateAdaptationNetwork.host[1].app[0]  packetReceived:count  227
scalar  HeRateAdaptationNetwork.host[2].app[0]  packetReceived:count  195
scalar  HeRateAdaptationNetwork.host[3].app[0]  packetReceived:count  200

HeMinstrel-#0.sca:
scalar  HeRateAdaptationNetwork.host[0].app[0]  packetReceived:count  230
scalar  HeRateAdaptationNetwork.host[1].app[0]  packetReceived:count  227
scalar  HeRateAdaptationNetwork.host[2].app[0]  packetReceived:count  195
scalar  HeRateAdaptationNetwork.host[3].app[0]  packetReceived:count  200
```

AP Datarate vector summary (both configs):
`HeRateAdaptationNetwork.ap.wlan[0].mac.hcf datarateSelected:vector count=1144 mean=27.34 Mbps min=7.31 Mbps max=121.87 Mbps`

Individual Host uplink datarates (both configs):
- `host[0]` (closest): `mean=111.46 Mbps` (using high MCS)
- `host[1]`: `mean=114.23 Mbps`
- `host[2]`: `mean=120.40 Mbps`
- `host[3]` (furthest): `mean=7.31 Mbps` (uses fixed MCS 0 due to low SNR)

---

## Interpretation of Results

1. **Identical Results in Static Channel**:
   - The packet delivery numbers and selected bitrates are identical between the static `FixedMcs` and dynamic `HeMinstrel` runs.
   - *Why?* The simulation uses a deterministic, collision-free, static channel model (Free Space Path Loss). The path loss and SNR for each host remain completely constant over time.
   - Under these conditions:
     - `FixedMcs` maps the stable SNR to the same optimal MCS for each peer.
     - `HeMinstrel` probes the rates and dynamically converges to the same optimal MCS because the success rate of the supported MCS levels is 100%.

2. **Datarate vs. Distance**:
   - The AP dynamically transmits to closer stations (like `host[0]`) at much higher bitrates (up to 121.87 Mbps, using high MCS) because of the strong signal.
   - For the cell-edge `host[3]` at 230m, the AP is forced to transmit at the robust base rate of 7.31 Mbps (MCS 0) to ensure the signal is decodable.

3. **Advantages of Minstrel in Dynamic Channels**:
   - While both algorithms perform identically in this static sandbox, in real-world scenarios with client mobility, blockages, or multi-user interference, a static SNR mapping is highly inaccurate or unavailable. In those conditions, Minstrel's adaptive feedback loop is essential to track link quality changes dynamically using transmission success history.
