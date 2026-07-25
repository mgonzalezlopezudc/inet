# Walkthrough: HE extended-range single-user

This walkthrough compares matched cell-boundary HE single-user (HE-SU) and HE
extended-range single-user (HE-ER-SU) transmissions. Run-0 radiotap fields
distinguish the PPDU formats; five-run application vectors measure the bounded
delivery outcome under the configured error model.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish HE-SU from HE-ER-SU physical layer protocol data units (PPDUs);
- explain why repeated HE-SIG-A signaling can improve robustness;
- separate transmitted-format evidence from receiver/application evidence;
  and
- reproduce the representative run and first diagnostics.

HE-ER-SU carries one user's payload and repeats the HE-SIG-A signaling field.
The extra signaling time trades efficiency for robustness. A format label in
a sender-side capture proves what was transmitted, not that the receiver
decoded it or that range improved.

## Scenario description

[HeErSuNetwork.ned](HeErSuNetwork.ned) extends the common single-BSS network
with one AP and one station; a wired server sends downlink UDP. In the boundary
pair the AP and host are 340 m apart, background noise is `-89 dBm`,
sensitivity is `-100 dBm`, generic SNIR threshold is `0 dB`, payload is
100 B, interval is 600 µs, and Block Ack is disabled. Traffic starts at
`0.3 s`; there is no separate warm-up interval in the boundary pair, and
results use `0.3–2.0 s`. [omnetpp.ini](omnetpp.ini) fixes MCS 0 in
both cases so the primary delta is the PPDU format.

```text
server -- AP  ~~~~~~~~~ 340 m ~~~~~~~~~  host[0]
```

`ErBss` is a separate management/rate-control example and is not part of the
cell-boundary outcome comparison.

## Standards and INET model boundary

IEEE Std 802.11-2024 Clauses 27.1.4 and 27.3.4 define HE-SU and HE-ER-SU PPDU
formats; Table 27-13 gives the longer HE-SIG-A-R duration for HE-ER-SU. These
were verified in corpus chunks `80211ax-2024:chunk:09989`, `10075`, and
`10124`.

INET's `enableExtendedRangeSu` rate-control choice, error model, path loss,
noise, and thresholds are modeling choices. The experiment does not decode
HE-SIG-A success separately and does not certify a real-world range gain.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Treatment transmits HE-ER-SU at MCS 0 | `PASS` | radiotap HE profile | packet run/seed `0` | 4513 QoS Data observations |
| Control transmits HE-SU at MCS 0 | `PASS` | radiotap HE profile | packet run/seed `0` | 4615 QoS Data observations |
| HE-ER-SU goodput is higher in boundary setup | `PASS` | application vectors | runs/seeds `0–4` | `0.3–2.0 s` |
| Repeated HE-SIG-A caused each delivery | `INCONCLUSIVE` | no per-frame signaling decode/outcome correlation | none | configuration-level inference |
| General range extension | `NOT RUN` | no distance curve | none | one boundary point only |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `CellBoundaryHeSu` | Control | fixed HE-SU MCS 0 | 340 m, matched UDP/noise | `0–4` | HE-SU frames |
| `CellBoundaryHeErSu` | Treatment | extends control; ER-SU enabled, max MCS 0 | matched | `0–4` | HE-ER-SU and improved boundary delivery |
| `ErBss` | Feature example | ER-BSS and full management exchange | lighter traffic | packet run `0` | HE-ER-SU management/data formats |

The treatment extends the control, then installs `HeMinstrelRateControl`,
enables extended-range SU, clears the fixed data bitrate with `-1bps`, and
caps MCS at 0. Those more specific assignments win over the general interface
bitrate.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| PPDU format differs while MCS matches | AP PCAP HE fields | same/unknown format or MCS | rate control/PHY encoder | inspect known bits and selected mode |
| ER frame duration is longer at equal payload | packet statistics | no signaling overhead | duration calculator | compare GI, NSS, coding, and size |
| ER boundary goodput is higher | sink byte vectors | overlap/reversal | receiver error model | correlate reception/drop telemetry per seed |

## Reproduction

Run from the repository root. This command is illustrative and was **NOT RUN**
during this rewrite:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/he_er_su/omnetpp.ini \
  -c CellBoundaryHeErSu -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/he_er_su/results/manual/CellBoundaryHeErSu
```

Campaign regeneration:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py er -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py er
```

## Scalar and vector analysis

Inputs are the `.sca`/`.vec` pairs in each configuration directory under
`results/scalar-vector/20260725T120411Z/`. The sidecar
[he-er-su-boundary.png.json](../analysis/figures/er/he-er-su-boundary.png.json)
records all hashes, filters, seeds, and the `0.3–2.0 s` window.

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND ((module =~ "**.app[*]" AND name =~ "packetReceived:vector(packetBytes)") OR (module =~ "**.mac" AND name =~ "packetDropIncorrectlyReceived:vector(packetBytes)"))' \
  examples/ieee80211ax/he_er_su/results/scalar-vector/20260725T120411Z/*/*.vec
```

| Configuration | Application goodput |
|---|---:|
| `CellBoundaryHeSu` | 0.3870 ± 0.0059 Mbit/s |
| `CellBoundaryHeErSu` | 0.4435 ± 0.0098 Mbit/s |

Values are per-run means ± two-sided 95% Student-t CIs over five independent
seeds. Bytes received in `0.3–2.0 s` are aggregated within each run before
the across-run CI. The optional incorrect-reception vector may be absent when
zero. This supports a bounded delivery comparison, not a general range claim.

## PCAP statistics

Session `results/packet-statistics/20260724T175025Z` contains run/seed 0 legacy
PCAPs at AP and host MAC observation points. TShark 4.6.4 decodes them.

```sh
tshark -n -r 'examples/ieee80211ax/he_er_su/results/packet-statistics/20260724T175025Z/CellBoundaryHeErSu/CellBoundaryHeErSu-#0HeErSuNetwork.ap.wlan[0].pcap' \
  -q -z io,stat,0,'wlan.fc.type_subtype==0x28'
```

| Configuration | AP/global observations | QoS Data evidence |
|---|---:|---|
| `CellBoundaryHeSu` | 7007 | 4615 HE-SU, MCS 0, 20 MHz; mean 217.6 µs, 166 B |
| `CellBoundaryHeErSu` | 8669 | 4513 HE-ER-SU, MCS 0, 242-tone RU, NSTS 1; mean 225.6 µs, 166 B |
| `ErBss` | 240 | 120 HE-ER-SU QoS Data; MCS 0/1/2 counts 3/5/112 |

Both boundary rows use 10 dBm transmit power. Counts are capture observations,
not delivered application packets.

## Frame exchange analysis

```sh
tshark -n -r 'examples/ieee80211ax/he_er_su/results/packet-statistics/20260724T175025Z/CellBoundaryHeErSu/CellBoundaryHeErSu-#0HeErSuNetwork.ap.wlan[0].pcap' \
  -Y 'frame.number <= 2' -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.fc.type_subtype -e radiotap.he.data_1.ppdu_format \
  -e radiotap.he.data_3.data_mcs -e radiotap.he.data_5.data_bw_ru_allocation \
  -e radiotap.he.data_6.nsts
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.300252 s | AP → host[0] | QoS Data / HE-ER-SU | PPDU `1`, MCS 0, BW/RU code 7, NSTS 1 | treatment data |
| 2 | 0.300314 s | host[0] → AP | Ack | subtype `0x1d` | receiver MAC response |

The typed analyzer maps code 7 to the authoritative 242-tone HE-ER-SU
observation after checking radiotap presence and known bits.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| ER treatment changes PPDU format | `PASS` | ER rate control enabled | selected-mode path implied | HE-ER-SU vs HE-SU decoded | n/a |
| ER treatment improves this boundary outcome | `PASS` | matched MCS/load/channel | application receive vectors | equal-MCS format distinction | higher five-run goodput |
| repeated SIG-A is per-frame cause | `INCONCLUSIVE` | requested format | no signaling decision vector | no SIG-A-success field | separate-session aggregate |

The bounded verdict is `PASS` for PPDU-format selection and higher application
goodput at this configured boundary, with per-packet causal attribution
`INCONCLUSIVE`.

## Limitations and inconclusive claims

- Scalar/vector and packet evidence come from separate sessions.
- The capture identifies transmitted format but not HE-SIG-A decode success.
- One distance, path-loss realization, MCS, and noise point do not define a
  range curve.
- Resolve causality by co-recording selected mode, signaling/header error
  outcome, receiver decision, and AP/host captures in one seed.

## Further experiments

- Sweep distance or noise around the boundary and compare matched delivery
  curves across several seeds.
- Add a negative control with ER disabled but dynamic rate control retained.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | no direct HE-SIG-A success/failure observation |
| Intended behavior | expose signaling-field outcome without altering reception |
| Smallest change surface | first assess existing PHY error-model signals/results; add telemetry only if absent |
| Observability | selected PPDU format plus preamble/header/data error stage |
| Validation | co-record control/treatment, boundary points, seeds, PCAP and receiver telemetry |
| Compatibility and risks | telemetry must not perturb RNG or packet processing |
| Architecture and sealing | apply architecture/sealing review before any `src/inet` edit |
| Next handoff | HE PHY/error-model maintainer |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z` | boundary pair, runs/seeds `0–4` | sidecar; `0.3–2.0 s` | SHA-256 per input |
| PCAP | `results/packet-statistics/20260724T175025Z` | three configs, run/seed `0` | TShark 4.6.4; MAC | separate session |
| Figure | `../analysis/figures/er/he-er-su-boundary.png` | boundary pair | per-run CI | provenance sidecar |
