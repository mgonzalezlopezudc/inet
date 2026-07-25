# HE Extended-Range Single-User Comparison

This example compares `CellBoundaryHeSu` and `CellBoundaryHeErSu` in the
[HeErSuNetwork](HeErSuNetwork.ned) topology. The parameters are defined in
[omnetpp.ini](omnetpp.ini). Application vectors measure delivery in the
configured reception-boundary experiment, while radiotap fields establish the
PPDU format, MCS, RU, and spatial-stream metadata.

## Run the example

```sh
bin/inet -u Cmdenv -c CellBoundaryHeSu \
  examples/ieee80211ax/he_er_su/omnetpp.ini -r 0
bin/inet -u Cmdenv -c CellBoundaryHeErSu \
  examples/ieee80211ax/he_er_su/omnetpp.ini -r 0
```

Generate runs `0–4` and rebuild the comparison figure:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py er -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py er
```

The campaign is stored under
[`results/scalar-vector/20260725T120411Z`](results/scalar-vector/20260725T120411Z).
The figure provenance,
[`he-er-su-boundary.png.json`](../analysis/figures/er/he-er-su-boundary.png.json),
lists all `.sca` and `.vec` inputs with SHA-256 digests, runs `0–4`, the
`0.3–2.0 s` window, and the exact result filters.

## Scalar/vector evidence

The figure computes application goodput from
`packetReceived:vector(packetBytes)` at `**.app[*]`. It also names
`packetDropIncorrectlyReceived:vector(packetBytes)` at `**.mac` as an optional
result when its value is zero.

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND ((module =~ "**.app[*]" AND name =~ "packetReceived:vector(packetBytes)") OR (module =~ "**.mac" AND name =~ "packetDropIncorrectlyReceived:vector(packetBytes)"))' \
  examples/ieee80211ax/he_er_su/results/scalar-vector/20260725T120411Z/*/*.vec
```

![HE-SU and HE-ER-SU boundary comparison](../analysis/figures/er/he-er-su-boundary.png)

| Configuration | Application goodput |
|---|---:|
| `CellBoundaryHeSu` | 0.3870 ± 0.0059 Mbit/s |
| `CellBoundaryHeErSu` | 0.4435 ± 0.0098 Mbit/s |

Values are means ± 95% Student-t confidence intervals over runs `0–4` in the
provenance-defined window. The delivery vectors therefore show higher
application goodput for `CellBoundaryHeErSu` in this configured experiment.
They do not establish a general range guarantee.

## Packet evidence

The retained run-0 packet session is
[`results/packet-statistics/20260724T175025Z`](results/packet-statistics/20260724T175025Z).
Its evidence checks and exact frame rows report:

| Configuration | AP/global observations | QoS Data evidence |
|---|---:|---|
| `CellBoundaryHeSu` | 7007 | 4615 HE-SU, MCS 0, 20 MHz observations; 0 decoded as HE-ER-SU |
| `CellBoundaryHeErSu` | 8669 | 4513 HE-ER-SU, MCS 0, 242-tone RU, NSTS 1 observations |
| `ErBss` | 240 | 120/120 QoS Data observations decoded as HE-ER-SU |

For `CellBoundaryHeSu`, the QoS Data row reports mean duration `217.6 µs`,
mean size `166.0 B`, and transmit power `10.0 dBm`. For
`CellBoundaryHeErSu`, the matching row reports `225.6 µs`, `166.0 B`, and
`10.0 dBm`. These radiotap-decoded rows establish equal MCS 0 and different
HE-SU/HE-ER-SU formats; the application-delivery difference is measured by
`packetReceived:vector(packetBytes)`, not by the transmitted-frame counts.

The `ErBss` packet table further breaks its 120 HE-ER-SU QoS Data observations
into 3 at MCS 0, 5 at MCS 1, and 112 at MCS 2; all use a 242-tone RU and one
spatial stream according to the packet-statistics evidence check.

Inspect the AP-side HE-ER-SU capture:

```sh
tshark -n -r \
  'examples/ieee80211ax/he_er_su/results/packet-statistics/20260724T175025Z/CellBoundaryHeErSu/CellBoundaryHeErSu-#0HeErSuNetwork.ap.wlan[0].pcap' \
  -c 20
```

Regenerate the packet artifacts with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/analyze_pcap_types.py \
  --generate --subdir he_er_su
```

See `packet_statistics.png` for the complete QoS Data, Ack, duration, RU,
frequency, signal, power, and estimated-airtime tables. The capture identifies
transmitted formats; it does not separately decode HE-SIG-A reliability.
