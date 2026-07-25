# Walkthrough: HE channel widths

This example compares matched 20, 40, 80, and 160 MHz HE single-user
configurations. Five-run application results measure capacity and delay in the
offered-load-limited topology, while run-0 radiotap captures establish the
transmitted bandwidth and airtime of equal-sized QoS Data frames.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why a wider channel supplies more HE subcarriers at fixed MCS;
- identify channel width in effective configuration and radiotap evidence;
- relate equal-sized frame duration to application goodput and delay; and
- reproduce the run and diagnostic queries.

Channel width is the occupied RF bandwidth, not an application bitrate.
Keeping modulation and coding scheme (MCS), guard interval, payload, topology,
and offered load matched isolates the modeled bandwidth effect. Wider channels
should shorten equal-sized PHY transmissions and raise service capacity, but
the relationship need not be linear.

## Scenario description

[HeChannelWidthsNetwork.ned](HeChannelWidthsNetwork.ned) contains one wired
server, one AP, and four stationary wireless hosts. The server warms each flow
at `0.2 s`, then offers downlink UDP traffic from `0.3 s`; the analyzed window
is `0.3–0.43 s`. [omnetpp.ini](omnetpp.ini) changes channel number, band name,
receiver bandwidth, sensitivity, and fixed bitrate as a consistent width
bundle. There is no mobility or external interferer.

```text
server -- AP ~~ {host[0], host[1], host[2], host[3]}
```

## Standards and INET model boundary

IEEE Std 802.11-2024 describes HE channel-width capabilities in Table 9-376
and limits transmission width to the BSS channel width in Clause 26.17.1
(corpus chunks `80211ax-2024:chunk:03627`, `09952`, and `09956`). The standard permits
these widths; it does not guarantee the throughput values below.

INET's configured radio bandwidth, sensitivity, rate, scheduler, traffic, and
packet-level error model define this experiment. Radiotap is direct capture
evidence only where the HE presence/known bits support a value.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Each treatment transmits at its named width | `PASS` | radiotap HE QoS Data rows | packet run/seed `0` | five frames per width |
| Equal payload duration decreases with width | `PASS` | decoded QoS Data durations | packet run/seed `0` | direct packet observation |
| Goodput increases and p95 delay decreases | `PASS` | application vectors | runs/seeds `0–4` | `0.3–0.43 s`, this load/topology |
| Wider width improves coverage | `NOT RUN` | no distance/sensitivity sweep | none | scenario does not test coverage |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `Width20MHz` | Control | 20 MHz | matched saturated downlink | `0–4` | longest frame duration |
| `Width40MHz` | Treatment | 40 MHz | matched | `0–4` | more capacity than 20 MHz |
| `Width80MHz` | Treatment | 80 MHz | matched | `0–4` | more capacity than 40 MHz |
| `Width160MHz` | Treatment | 160 MHz | matched | `0–4` | shortest duration/highest capacity |

The width-specific config blocks supply consistent `bandName`, channel,
receiver bandwidth, sensitivity, and fixed HE MCS-1 bitrate. Those coordinated
changes are necessary for a valid radio configuration but mean the result is a
width bundle rather than a single raw parameter toggle.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| HE QoS Data width equals config | AP PCAP radiotap HE field | unknown/wrong width | radio/PCAP encoding | inspect presence/known bits and effective radio bandwidth |
| Duration falls at equal size/MCS | packet statistics | non-monotonic duration | mode construction | compare MCS, GI, NSS, coding, and bytes |
| Goodput rises, p95 delay falls | sink vectors | reversed or missing metric | load/MAC/application | verify offered load and per-run window |

## Reproduction

Run from the repository root. The command is illustrative and was **NOT RUN**
during this rewrite:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/he_channel_widths/omnetpp.ini \
  -c Width20MHz -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/he_channel_widths/results/manual/Width20MHz
```

Campaign regeneration:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py width -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py width
```

## Scalar and vector analysis

Inputs are the `.sca` and `.vec` files in each configuration directory under
`results/scalar-vector/20260725T120411Z/`. The sidecar
[channel-width-dashboard.png.json](../analysis/figures/width/channel-width-dashboard.png.json)
binds all hashes, filters, runs, and the `0.3–0.43 s` window.

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND module =~ "**.app[*]" AND (name =~ "packetReceived:vector(packetBytes)" OR name =~ "endToEndDelay:vector")' \
  examples/ieee80211ax/he_channel_widths/results/scalar-vector/20260725T120411Z/*/*.vec
```

| Configuration | Aggregate goodput | p95 end-to-end delay |
|---|---:|---:|
| `Width20MHz` | 26.892 ± 0.000 Mbps | 97.253 ± 0.057 ms |
| `Width40MHz` | 50.708 ± 0.000 Mbps | 63.809 ± 0.077 ms |
| `Width80MHz` | 81.428 ± 0.335 Mbps | 40.060 ± 0.075 ms |
| `Width160MHz` | 118.646 ± 0.000 Mbps | 9.406 ± 0.075 ms |

These values are a **derived measurement** from the named application vectors.
Each run is aggregated before computing the mean and two-sided 95% Student-t
CI over five independent seeds. The p95 is computed within each run; vector
samples are not repetitions. No warm-up samples enter the `0.3–0.43 s`
window.

## PCAP statistics

Capture session `results/packet-statistics/20260724T175025Z` records run/seed 0
MAC observations at each `wlan[0]` in legacy PCAP. TShark 4.6.4 decodes it.

```sh
tshark -n -r 'examples/ieee80211ax/he_channel_widths/results/packet-statistics/20260724T175025Z/Width20MHz/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap' \
  -q -z io,stat,0,'wlan.fc.type_subtype==0x28'
```

| Configuration | All AP observations | QoS Data evidence (5 frames, 1066 B, MCS 1, GI 3.2 µs, LDPC) |
|---|---:|---|
| `Width20MHz` | 781 | 20 MHz, mean 619.1 µs |
| `Width40MHz` | 734 | 40 MHz, mean 327.6 µs |
| `Width80MHz` | 791 | 80 MHz, mean 175.2 µs |
| `Width160MHz` | 865 | 160 MHz, mean 105.6 µs |

Counts are capture observations, not de-duplicated application packets.

## Frame exchange analysis

```sh
tshark -n -r 'examples/ieee80211ax/he_channel_widths/results/packet-statistics/20260724T175025Z/Width20MHz/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap' \
  -Y 'frame.number <= 2' -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.fc.type_subtype -e radiotap.he.data_1.ppdu_format \
  -e radiotap.he.data_3.data_mcs -e radiotap.he.data_5.data_bw_ru_allocation
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200644 s | AP → host[0] | QoS Data / HE-SU | subtype `0x28`, PPDU `0`, MCS 1, width code 0 (20 MHz) | warm-up data |
| 2 | 0.200692 s | host[0] → AP | Ack | subtype `0x1d` | successful MAC response |

The packet-statistics decoder, which checks radiotap known bits, supplies the
human-readable width labels used above.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| Width bundle took effect | `PASS` | four width configs | none required | decoded 20/40/80/160 MHz | n/a |
| Wider channels serve this load faster | `PASS` | matched traffic | app vectors | shorter equal-size frames | monotonic goodput/delay |
| Wider channels increase range | `NOT RUN` | no distance control | none | none | none |

The bounded verdict is `PASS`: the configured widths are directly visible in
the capture and the five-run outcomes move monotonically in this scenario.

## Limitations and inconclusive claims

- The result and PCAP sessions are separate, so their relationship is
  configuration-level rather than event-level.
- Sensitivity differs consistently by width; no coverage conclusion follows.
- Only one topology, MCS, payload family, and offered-load regime are tested.
- Packet totals are not a capacity estimator.

## Further experiments

- Sweep offered load to locate saturation for each width.
- Repeat at matched receive power and multiple MCS values; predict that frame
  duration remains ordered but the capacity ratios change.

## Implementation plan

No implementation work is proposed; the retained invariants pass. Additional
coverage is experimental, not a demonstrated model gap.

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z` | four configs, runs/seeds `0–4` | sidecar filters; `0.3–0.43 s` | SHA-256 per input |
| PCAP | `results/packet-statistics/20260724T175025Z` | four configs, run/seed `0` | TShark 4.6.4, MAC observation | separate session |
| Figure | `../analysis/figures/width/channel-width-dashboard.png` | four configs | per-run CI; run-0 ECDF | provenance sidecar |
