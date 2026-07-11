# 802.11ax HE Extended Range Single User (ER SU) Simulation

This example illustrates the Extended Range Single User (ER SU) transmission format introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how HE ER SU PPDUs and robust PHY/MAC settings (such as Dual Carrier Modulation and MCS restrictions) maintain connectivity and data delivery at the outer boundaries (cell edge) of a wireless coverage area.

## Background: HE ER SU & Dual Carrier Modulation (DCM)

In dense or outdoor deployments, stations (STAs) at the cell edge suffer from low Signal-to-Noise Ratio (SNR) due to path loss, leading to packet reception failures. To extend range without increasing transmission power, 802.11ax introduces **HE ER SU**:
1. **Preamble Duplication**: The preamble of an HE ER SU PPDU repeats the HE-SIG-A field, increasing its duration by 8 µs (from 36 µs to 44 µs in total). This repetition allows receivers to combine the energy of the two fields, significantly improving HE-SIG-A decoding robustness.
2. **Resource Unit Limitation**: HE ER SU is restricted to a single spatial stream (NSS = 1) and robust MCS indices (MCS 0, 1, or 2).
3. **Dual Carrier Modulation (DCM)**: DCM modulates the same information block over two widely separated subcarriers in the frequency domain. This introduces frequency diversity, making the transmission resilient against frequency-selective fading and noise, at the cost of halving the net data rate.

---

## Network Topology

The network [HeErSuNetwork.ned](HeErSuNetwork.ned) consists of:
- **`ap`**: An Access Point located at `(390, 180)`.
- **`edge`**: A stationary wireless host located at `(70, 180)` (representing a cell-edge client).
- **Distance**: The AP and client are separated by **320 meters**. At this distance, the Free Space Path Loss (FSPL) at 5 GHz is approximately 96.5 dB, resulting in very weak signal reception (around -86.5 dBm at 10mW transmission power).
- **`server`**: A wired server connected to the AP.
- **Traffic**: Downlink UDP traffic is sent from the `server` to the `edge` host via the AP (300B packets sent every 1ms).

```
        [server]
           | (wired)
           v
        [ ap ] <--------------- 320m ---------------> [edge]
      (AP at 390m)                                (STA at 70m)
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines three scenarios:

### 1. `HeSu` (Baseline)
- The AP transmits standard HE SU PPDUs.
- It uses a fixed bitrate configuration of `7.3125 Mbps` (corresponding to MCS 0 with 20 MHz bandwidth, NSS = 1, and no DCM).
- Preamble format is the standard HE SU PPDU (36 µs).

### 2. `HeErSu` (Extended Range SU)
- The AP enables Minstrel rate control with ER SU capability:
  - `**.ap.wlan[*].mac.hcf.rateControl.typename = "HeMinstrelRateControl"`
  - `**.ap.wlan[*].mac.hcf.rateControl.enableExtendedRangeSu = true`
  - `**.ap.wlan[*].mac.hcf.rateControl.preferDcm = true`
  - `**.ap.wlan[*].mac.hcf.rateControl.maxMcs = 2`
- **Result**: The AP rate controller restricts MCS selection to robust levels (0, 1, 2), activates DCM to combat frequency selective fading, and formats the PPDUs as HE ER SU with the repeated HE-SIG-A preamble field (44 µs total duration).

### 3. `ErBss` (ER BSS management behavior)

- Extends `HeErSu`, enables full beaconing/association instead of installing
  association state at initialization, and sets the HE ER BSS capability.
- Management-frame bitrate overrides are removed so HE rate control can choose
  ER SU for the relevant management and group-addressed transmissions.
- Block Ack is disabled to keep the trace focused on ER-BSS management and
  single-MPDU behavior; this is a scope choice, not an ER-BSS requirement.

---

## Running the Simulation

Ensure your environment is set up, then run the simulations.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c HeErSu examples/ieee80211ax/he_er_su/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
# Run HeSu Baseline
bin/inet -u Cmdenv -c HeSu examples/ieee80211ax/he_er_su/omnetpp.ini

# Run HeErSu Config
bin/inet -u Cmdenv -c HeErSu examples/ieee80211ax/he_er_su/omnetpp.ini

# Run ER-BSS beaconing and association
bin/inet -u Cmdenv -c ErBss examples/ieee80211ax/he_er_su/omnetpp.ini
```

---

## Verifying Results

After running the simulations, analyze the packets received at the UDP application layer on the `edge` host and MAC layer packet drops due to incorrect reception.

```sh
# Query packets received at the UDP sink on the edge host
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.edge.app*"' examples/ieee80211ax/he_er_su/results/*.sca

# Query packet drops due to corruption/incorrect reception at the edge MAC layer
opp_scavetool query -l -f 'name =~ "packetDropIncorrectlyReceived:count" and module =~ "*.edge.wlan[0].mac"' examples/ieee80211ax/he_er_su/results/*.sca
```

### Expected Output Summary

```
HeSu-#0.sca:
scalar  HeErSuNetwork.edge.app[0]         packetReceived:count                 1800
scalar  HeErSuNetwork.edge.wlan[0].mac    packetDropIncorrectlyReceived:count  42

HeErSu-#0.sca:
scalar  HeErSuNetwork.edge.app[0]         packetReceived:count                 1800
scalar  HeErSuNetwork.edge.wlan[0].mac    packetDropIncorrectlyReceived:count  42
```

### Interpretation of Results

1. **Successful Delivery**:
   - Both scenarios successfully deliver **1800 packets** to the `edge` host (out of 1801 packets sent by the server).
   - *Why?* Under the clean, deterministic Free Space Path Loss model without dynamic shadowing or multipath fading, the signal at 320 meters (-86.55 dBm) remains slightly above the receiver sensitivity threshold of -88 dBm. This allows MCS 0 to decode successfully in both cases, experiencing only minor packet drops (42 packets) during initial link establishment/coordination.

2. **Under-the-Hood Preamble Difference**:
   - In `HeSu`, transmissions use standard HE SU PPDUs with a **36 µs** preamble.
   - In `HeErSu`, transmissions employ HE ER SU PPDUs. The preamble includes the repeated HE-SIG-A field, increasing the preamble duration to **44 µs**. This extra 8 µs of repeated header symbol provides the essential energy duplication required to decode preambles under real-world multipath fading and noise, which would otherwise drop the link entirely.
   - The configuration demonstrates selection and timing of the robust format.
     It does not prove a range gain in this deterministic free-space channel;
     such a claim requires a fading/noise experiment and a delivery/PER sweep.
