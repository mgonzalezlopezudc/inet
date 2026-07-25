# HE Rate-Adaptation Walkthrough

This walkthrough is limited to the selected-rate, probability, outcome, and
retry telemetry recorded for the HE Minstrel configurations in
[omnetpp.ini](omnetpp.ini). The topology is defined in
[HeRateAdaptationNetwork.ned](HeRateAdaptationNetwork.ned).

## Five-run telemetry

The scalar/vector session
[`20260725T120411Z`](results/scalar-vector/20260725T120411Z) contains
`HeMinstrelMobile` runs `0` through `4`. Its aligned AP rate-control vectors are:

| Vector | Evidence represented |
|---|---|
| `heRateSelectedMcs:vector` | Selected MCS; observed range across the five runs is MCS `0` through `9`. |
| `heRateSelectedNss:vector` | Selected spatial-stream count; every recorded value is `1` in all five runs. |
| `heRateSuccessProbability:vector` | Controller success-probability telemetry for the selected candidate; recorded values span `0.0158203` through `0.98` across the five runs. |
| `heRateTxSuccess:vector` | Per-attempt outcome; its five-run success fraction is `0.998445 ± 0.001980` (95% CI). |
| `heRateRetryCount:vector` | Controller retry telemetry; every recorded value is `0` in all five runs. |
| `packetSentToPeerWithRetry:vector(packetBytes)` and `packetDropRetryLimitReached:vector(packetBytes)` | MAC retry and retry-limit telemetry; inspect matching AP HCF modules and timestamps rather than deriving retries from control-frame counts. |
| `packetReceived:vector(packetBytes)` | Application delivery; five-run aggregate goodput is `13.338 ± 1.053 Mbps` (95% CI). |

The MCS range and outcome fraction demonstrate that multiple selections were
made while nearly all recorded attempts succeeded. They do not establish a
performance advantage because this session contains no matched comparison
configuration.

## Reproduce and inspect

```sh
bin/inet -u Cmdenv -c HeMinstrelMobile -r 0 examples/ieee80211ax/he_rate_adaptation/omnetpp.ini --result-dir=examples/ieee80211ax/he_rate_adaptation/results/manual
```

```sh
opp_scavetool query -l \
  -f 'name =~ "heRateSelectedMcs:vector" or name =~ "heRateSelectedNss:vector" or name =~ "heRateSuccessProbability:vector" or name =~ "heRateTxSuccess:vector" or name =~ "heRateRetryCount:vector" or name =~ "packetSentToPeerWithRetry:vector(packetBytes)" or name =~ "packetDropRetryLimitReached:vector(packetBytes)"' \
  examples/ieee80211ax/he_rate_adaptation/results/manual/*.sca \
  examples/ieee80211ax/he_rate_adaptation/results/manual/*.vec
```

The retained packet-statistics session
[`20260724T175025Z`](results/packet-statistics/20260724T175025Z) contains AP and
station captures plus `.sca`/`.vec` files for `FixedMcs`, `HeMinstrel`, and
`HeMinstrelMobile`. Its exact tables below establish protocol-visible frame
populations. The table's evidence check correctly treats selected MCS/NSS,
probability, outcomes, and retries as vector evidence.

```sh
tshark -n -r 'examples/ieee80211ax/he_rate_adaptation/results/packet-statistics/20260724T175025Z/HeMinstrelMobile/HeMinstrelMobile-#0HeRateAdaptationNetwork.ap.wlan[0].pcap' -c 20
```

<!-- REWRITE-PREFIX-END -->


<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260724T175025Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FixedMcs produced protocol-visible wireless observations | 1624 AP/global transmission observations |
| **PASS** | HeMinstrel produced protocol-visible wireless observations | 4460 AP/global transmission observations |
| **PASS** | HeMinstrelMobile produced protocol-visible wireless observations | 4800 AP/global transmission observations |
| **INCONCLUSIVE** | Selected MCS/NSS, EWMA outcome and retries | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### Configuration: `FixedMcs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1624**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eb314" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 1435 | 88.36% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 99.63% | 78.41% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 84 | 5.17% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.15% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 84 | 5.17% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.16% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.25% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.49% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.55% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -75.0 dBm | 13.0 dBm | 0.04% | 0.03% |

### Configuration: `HeMinstrel`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4460**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eb314" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 353 | 7.91% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 36.01% | 19.29% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 811 | 18.18% | 54.2 B | 2.6 B | 38.1 us | 0.9 us | 5010 MHz | - | 13.0 dBm | 2.88% | 1.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 50 | 1.12% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.13% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 50 | 1.12% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -75.9 dBm | - | 0.14% | 0.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 247 | 5.54% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -77.5 dBm | - | 2.50% | 1.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 1339 | 30.02% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz | -77.2 dBm | - | 42.89% | 22.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 778 | 17.44% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz, 5017 MHz | -74.0 dBm | - | 13.77% | 7.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.09% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.20% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -75.0 dBm | 13.0 dBm | 0.06% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 811 | 18.18% | 3.0 B | 0.0 B | 21.0 us | 0.0 us | - | 3278.2 dBm | - | 1.59% | 0.85% |

### Configuration: `HeMinstrelMobile`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4800**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#26ba2b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.04% | 966.0 B | 0.0 B | 1074.8 us | 18.0 us | 5010 MHz | - | 13.0 dBm | 0.22% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eb314" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 177 | 3.69% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 19.53% | 9.67% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 916 | 19.08% | 54.7 B | 3.2 B | 38.2 us | 1.1 us | 5010 MHz | - | 13.0 dBm | 3.54% | 1.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 28 | 0.58% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.08% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 28 | 0.58% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -75.2 dBm | - | 0.09% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 293 | 6.10% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -77.7 dBm | - | 3.20% | 1.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 1610 | 33.54% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz | -77.3 dBm | - | 55.80% | 27.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 810 | 16.88% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz, 5017 MHz | -74.3 dBm | - | 15.51% | 7.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.17% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.06% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 916 | 19.08% | 3.0 B | 0.0 B | 21.0 us | 0.0 us | - | 3349.7 dBm | - | 1.94% | 0.96% |

<!-- END GENERATED: ieee80211ax-pcap-statistics -->
