## Verdict

This is a substantial and useful **packet-level 802.11ax/HE simulation model**, with real DL/UL OFDMA scheduling, MU-MIMO scaffolding, TWT, BSS coloring, and solid test coverage. It is **not fully IEEE 802.11-2024 compliant** in the interoperability/certification sense. Several paths intentionally use custom model metadata instead of standard on-air encodings; a few PHY/FEC constraints are materially wrong.

The clearest positioning would be: “HE feature model for system-level simulation,” not “fully standard-compliant 802.11ax implementation.”

## 1. Standard compliance

I checked against the supplied IEEE 802.11-2024 text, principally Clauses 26, 27, 9.3.1.22, and 9.4.2.247.

| Area | Assessment | Notes |
|---|---|---|
| HE MCS/GI/RU basics | Mostly good | MCS 0–11, 0.8/1.6/3.2 µs GIs, 26–1992 tone RUs, RU placement validation, common-duration MU PPDUs, 5.484 ms limit. |
| DL MU-OFDMA | Good simulation support | Per-STA queues, schedulers, A-MPDU/BA gating, common PPDU duration, fallback to SU. |
| UL OFDMA/UORA | Good but simplified | Basic/BSRP triggers, scheduled/RA RUs, OBO/OCW behavior, Multi-STA BA model. |
| DL MU-MIMO | Partial | CSI/sounding and leakage model exist, but this is a packet-level abstraction rather than beamforming/PHY processing. |
| HE management IEs | Partial | Own serializer handles selected capability/operation fields, but not the complete HE Capability/Operation semantics. |
| TWT | Partial | Individual, announced/unannounced, and broadcast scheduling are modeled; coverage is not the complete TWT state machine. |
| BSS coloring / OBSS-PD | Simplified | Color propagation and threshold-based ignore behavior work; the normative SRG/non-SRG/PSR/NAV/power constraints do not. |
| HE on-air formats | Not compliant | HE PHY header and Trigger frame are custom INET encodings, not standard HE-SIG or Trigger-frame bit encodings. |
| HE SU / HE ER SU / 6 GHz | Missing or largely absent | The explicit PPDU-format model only contains HE MU DL and HE trigger-based UL. |

### High-priority compliance defects

1. **FEC restrictions are not enforced.**  
   Clause 27.3.12.5 requires LDPC for 484-, 996-, and 2×996-tone RUs and for HE-MCS 10/11. The implementation accepts BCC for any valid coding enum and MCS/RU combination. It also defaults to MCS 11, 40/80/160 MHz support, and `ldpc = false`, which is internally inconsistent with the standard.

   See [Ieee80211HeCapabilities.h](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h:47) and [Ieee80211HePhyCalculator.h](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h:392).

2. **BCC timing is wrong for higher-rate configurations.**  
   The calculator derives multiple BCC encoders from `NDBPS` and adds six tail bits per encoder. Clause 27.3.12.5.1 says HE BCC always uses one encoder. This affects PPDU duration and throughput results.

   See [Ieee80211HePhyCalculator.h](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h:413).

3. **Custom wire formats prevent true protocol conformance.**  
   The code explicitly labels the HE PHY header as an INET model representation rather than bit-exact HE-SIG encoding. The Trigger serializer writes a custom trigger ID, duration, RU index/offset, TID, RSSI, and internal extension flag; these are not the standard Trigger Common/User Info field format from 9.3.1.22.

   See [Ieee80211PhyHeader.msg](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg:184) and [Ieee80211MacHeaderSerializer.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc:326).

4. **Preamble-puncturing validation is too permissive.**  
   The parser accepts any 80/160 MHz bit mask provided the primary 20 MHz is active and not every subchannel is punctured. Table 27-21 allows only particular patterns; for example, 160 MHz permits constrained adjacent patterns. Invalid masks can therefore enter the model.

   See [HeHcf.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:92).

5. **Spatial reuse is only a threshold rule.**  
   The receiver compares a differing BSS color with one configurable OBSS-PD threshold. Clause 26.10 also requires inter-BSS classification rules, non-SRG/SRG policy, PPDU exclusions, TX-power coupling, spatial-reuse fields, NAV effects, and PSR interactions. The model is useful for an illustrative OBSS-PD experiment, but not for standards-level SR studies.

6. **Multi-TID Block Ack is mostly representation, not operational behavior.**  
   The feature has frame classes, serializers, capability flags, and a round-trip test. There is no MAC-side generation/scheduling use beyond serialization/capability storage. The `MultiTidBlockAck` example configurations therefore advertise support but do not demonstrate a real multi-TID exchange.

## 2. Example-simulation quality

All five sampled scenarios completed successfully:

- DL OFDMA `EqualSizedRUs_fBW`
- UL OFDMA `MixedUora`
- HE features `CombinedHeFeatures`
- BSS coloring enabled
- TWT broadcast

That is a real strength: the examples are executable, not merely configuration sketches.

### Strengths

- DL OFDMA has several useful scheduler comparisons and a 20/80 MHz scale-up case.
- UL OFDMA exposes scheduled-only, mixed UORA, and equal-RU variants.
- HE-features isolates LDPC, PE, puncturing, mixed-capability, and combined configurations.
- BSS coloring offers an intuitive two-BSS topology.
- TWT keeps traffic/seed identical across its four configurations.
- The examples use static placement and simple UDP loads, which is appropriate for explaining one mechanism at a time.

### Problems

- The validation scripts are stale. Both [ofdma_example_validation.sh](/home/user/omnetpp_ws/inet/tests/validation/ieee80211/ofdma_example_validation.sh:1) and `he_mu_command_contract.sh` refer to a deleted `examples/ieee80211ax/ofdma` directory and `/home/user/omnetpp-6.4.0`, while the project now uses `dl_ofdma`/`ul_ofdma` and 6.4.0aipre2. They cannot validate the current examples.
- The DL walkthrough and compliance report still point at `examples/ieee80211ax/ofdma`, claim 2-second runs while the current DL example runs 0.6 s, and report stale numerical results.
- The BSS-coloring walkthrough’s claimed totals (345 disabled, 517 enabled) do not match current runs. My current runs delivered 313 versus 327 packets respectively; moreover the scenario has no fixed seed or repetitions, so one-run performance claims are not defensible.
- BSS coloring uses `sameTransmissionStartTimeCheck = "ignore"` and an aggressive receiver threshold override. That may be suitable as a model demonstration, but it must be described as a deliberate experimental simplification.
- DL/UL/HE-feature/BSS examples have no controlled repetitions, confidence intervals, offered-load sweep, or fairness/delay metrics. Packet count alone is too thin for scheduler claims.
- The HE-features walkthrough says `"0100"` becomes `0x4`; the parser treats the second character as subchannel index 1, hence the constructed bitmask is `0x2`. Documentation and implementation disagree.
- The TWT broadcast run completes with undisposed-packet warnings, so its lifecycle is not clean yet.
- UL OFDMA lacks a walkthrough/report equivalent to the DL material.

## 3. Code quality

### Strong points

- The architecture is mostly well-factored: PHY calculator, RU utilities, schedulers, HCF orchestration, frame sequences, capability negotiation, and TWT manager are separated.
- The HE comments are unusually good: many name the relevant standard clause and explain model choices.
- Debuggability is strong. `EV_INFO`, `EV_DEBUG`, `EV_WARN`, invariants, detailed rejection reasons, and scheduler/trigger diagnostics are pervasive.
- The new non-throwing PPDU calculation result (`valid`, `error`, parameters) is a particularly good design for simulation diagnostics.
- There are many focused tests: RU layout, timing, serializer round trips, per-user error behavior, block-ack gates, UORA, spatial reuse, MU-MIMO eligibility, and TWT frames.

### Weak points

- The main orchestration files are too large: `HeHcf.cc` is 1,358 lines and `HeDlMuTxOpFs.cc` is 1,119 lines. They mix policy, packet construction, capability checks, recovery, and logging; this raises regression risk.
- Model-only fields are interwoven with protocol-looking serializers. The distinction should be explicit in type names or module documentation to prevent accidental claims of wire compatibility.
- The same OBSS-PD decision appears twice in the receiver path; it should be a shared helper.
- Some comments overstate conformance despite simplified mechanics. The code’s own “packet-level model” caveat is more accurate than several example documents.
- Tests are mostly self-consistency/round-trip tests. There are no standard-derived golden octet vectors for HE Capability IEs, Trigger frames, HE-SIG fields, or TWT exchanges.
- `HeUlScheduler_1.test` currently fails the test harness’s `%contains` check despite all its internal assertions appearing to complete. The HE-focused test run is therefore **24 pass / 1 unexpected fail**.
- The codebase remains clean after the review; generated simulation results are ignored.

## 4. New components introduced

Relative to the pre-HE baseline, this work adds approximately **22,920 lines across 222 changed files**.

New component families include:

- HE mode/MCS definitions and HE-capable mode-set integration.
- HE RU catalog/layout/allocation, HE PHY calculator, HE-SIG-B RU codec, and HE packet metadata.
- Per-user MU transmission/reception, RU-aware attenuation/noise/error-model support.
- `HeHcf`, HE frame-sequence handler, DL MU TXOP, UL MU TXOP, and sounding frame sequence.
- Per-STA queue banks and manager.
- DL schedulers: equal-sized, backlog-based, HoL-min-delay.
- UL schedulers: equal-sized and backlog-based, plus UL coordinator and trigger policy.
- HE capability/operation model, HE management elements, and association negotiation.
- Trigger, Multi-STA Block Ack, Multi-TID Block Ack, and TWT frame representations.
- TWT manager plus example-only TWT agent/AP management support.
- New examples: DL OFDMA, UL OFDMA, HE features, BSS coloring, and TWT.
- 25+ HE/TWT-specific unit tests and two validation scripts.

## 5. Existing components modified

The implementation also changes core, pre-existing behavior in these areas:

- `Ieee80211Mac`, `Hcf`, `Dcf`, EDCA/contention, Tx/Rx contracts, frame-sequence plumbing, queues, retry/recovery, aggregation, and Block Ack handling.
- MAC frame definitions and serializers; management frames, AP/STA association, and MIB configuration.
- Rate selection/control and several MAC data services.
- PHY mode sets plus HT/VHT code paths to accommodate HE/LDPC-related behavior.
- Radio, transmitter, receiver, medium, transmission representation, protocol printer/dissector, tags, and PHY-header serialization.
- NIST/Yans/base error-model interfaces.
- Interface/NED assembly and visualizer support.

That scope is powerful, but it means HE regressions can affect legacy 802.11 behavior. Cross-mode regression testing should be a release gate.

## 6. SWOT

| Strengths | Weaknesses |
|---|---|
| Broad HE feature coverage for a simulator | Not bit-/wire-level interoperable |
| Good OFDMA architecture and scheduling extensibility | FEC legality/timing defects |
| Strong logging, assertions, and unit-test breadth | Very large orchestration classes |
| Executable, visually useful examples | Stale validation/docs and weak experiment design |
| Capability negotiation and per-user PHY abstraction | Several advertised features are only partial |

| Opportunities | Threats |
|---|---|
| Become a credible system-level Wi-Fi 6 research platform | Users may publish results as standard-accurate when they are model-dependent |
| Add a strict “standard constraints” validation layer | Incorrect BCC/LDPC timing can bias throughput/latency conclusions |
| Add standard vectors and external trace comparison | Stale scripts can silently remove regression protection |
| Separate model metadata from serialized protocol fields | Broad changes to shared MAC/PHY code can regress non-HE modes |
| Build reproducible benchmark suites with seed sets/statistics | TWT object-lifecycle warnings can grow into memory/lifetime defects |

## Recommended order of work

1. Enforce Clause 27.3.12.5 FEC rules and correct the one-encoder BCC model.
2. Validate puncturing against the permitted standard patterns.
3. Clearly label custom HE PHY/Trigger encodings as simulation-only, or implement real field encoding with golden vectors.
4. Fix the failing UL scheduler test and repair the stale validation scripts.
5. Update all walkthrough paths, environment commands, runtime claims, and numerical results.
6. Add seeded multi-run benchmark experiments with throughput, delay, fairness, collisions, PPDU duration, and energy metrics.
7. Either implement operational Multi-TID behavior or remove it from examples as a demonstrated feature.