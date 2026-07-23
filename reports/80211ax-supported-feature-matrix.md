# 802.11ax supported-feature and modeling matrix

## Fidelity contract

INET implements IEEE 802.11ax at packet-level PHY fidelity. HE SU, HE ER SU,
HE MU, and HE TB use a validated immutable TXVECTOR, an immutable calculated
PPDU layout, format-specific represented PLCP fields, and a recipient-specific
RXVECTOR. The model calculates RU geometry, coding parameters, symbol counts,
padding, packet extension, field durations, interference, SNIR, PER, and
per-MPDU outcomes. It does not generate scrambled, BCC/LDPC-coded, interleaved,
modulated, pilot, sample, or waveform data.

The logical HE signaling codecs and registered chunk serializers are normative
for the fields they represent. They are not a complete over-the-air PPDU
bitstream. Scheduler plans, packet ranges, Trigger correlation, queue handles,
calculated PHY annotations, and channel/error outcomes are model-only and have
no serializer registration.

The status terms below mean:

- **supported**: implemented, validated, and covered by focused tests;
- **supported when advertised**: optional behavior gated by directional
  local-TX/peer-RX or local-RX/peer-TX capabilities;
- **deliberately unsupported**: rejected explicitly rather than approximated;
- **model-only**: an analytical or simulation contract with no claimed IEEE
  wire representation.

## PPDU, channel, and PHY matrix

| Area | Status | Implemented boundary | IEEE 802.11-2024 provenance |
|---|---|---|---|
| HE SU | supported | Native HE protocol/preamble/header identity, SU HE-SIG-A, analytical Data/PE, RXVECTOR, radiotap HE | 27.3.4, 27.3.11, Tables 27-20 and 27-32 |
| HE ER SU | supported when advertised | Separate 20 MHz ER SU format, 242-tone or upper 106-tone mode, repeated HE-SIG-A, DCM/capability validation | 27.3.4, Tables 27-20 and 27-32 |
| HE MU | supported when advertised | DL OFDMA and full-bandwidth MU-MIMO, HE-SIG-A/B, per-user RU/stream layout and reception | 27.3.4, 27.3.11.8, Tables 27-21 and 27-25 through 27-28 |
| HE TB | supported when advertised | Basic/BSRP Trigger-authoritative UL Length, bandwidth, GI/LTF, LTF count, LDPC-extra/pre-FEC state, four Spatial Reuse fields, per-recipient context; TRS-originated UL Data Symbols are deliberately unsupported | 9.3.1.22, Tables 9-49 and 9-50; 27.3.4 and 27.3.11 |
| HE sounding NDP | supported | Explicit HE SU NDP with no Data field; zero-length non-NDP PSDU remains data | 27.3.4 and sounding procedures in Clause 26 |
| HE TB NDP feedback | supported | Explicit Trigger-derived NDP Feedback Report with recipient status/RU/STS context | 9.3.1.22 and 26.5.7 |
| 20/40/80/160 MHz | supported when advertised | Scalar contiguous channel widths, full RU catalogs, format-specific bandwidth signaling | 27.3.2, Tables 27-7 through 27-10 |
| 80+80 MHz | deliberately unsupported | A scalar `Hz` bandwidth cannot preserve two noncontiguous segment centers; canonical construction rejects it instead of treating it as 160 MHz | 27.3.2 and format-specific bandwidth tables |
| 2.4 GHz | supported | HE plus applicable DSSS/ERP/HT compatibility; 6 us signal-extension policy where required | 4.3.16 and Clause 27 band rules |
| 5 GHz | supported | OFDM/HT/VHT compatibility plus HE | 4.3.16 and Clause 27 band rules |
| 6 GHz | supported | HE profile without injecting inapplicable legacy PHY identity into HE packets | 4.3.16 and 6 GHz HE operation rules |
| 26/52/106/242/484/996/2x996-tone RUs | supported | Exact tone offsets, data/pilot counts, center frequency/bandwidth, and legal placement | Tables 27-7 through 27-10 and 27-15 |
| Center 26-tone and mixed RU layouts | supported | Width-specific allocation-tree geometry, all Table 27-27 semantic code ranges, center-RU handling, and reserved-code rejection | Tables 27-26 and 27-27 |
| MU-MIMO users per RU | supported when advertised | Up to eight users/space-time streams on one physical RU with disjoint stream ranges | 27.3.11.8, 27.3.11.10, Table 21-13 |
| Preamble puncturing | supported when advertised | The model-supported legal 80/160 MHz pattern sets, puncture-aware allocation, distinct HE signaling, and parser validation; 20/40 MHz puncturing is rejected | HE-SIG-A puncturing rules and Table 27-27 |
| GI/LTF and HE-LTF count | supported | Format-specific Table 27-32 combinations and legal counts 1, 2, 4, 6, 8; minimum DL count and Trigger-preserved TB count | Tables 27-13 and 27-32; 27.3.11.10 |
| MCS 0-7 | supported | Mandatory baseline modes constrained by width/NSS capability maps | HE MCS/NSS maps and Table 27-118 |
| MCS 8-11 | supported when advertised | Optional higher MCS modes; 1024-QAM, LDPC, RU/NSS, and DCM restrictions validated | 27.3.12.5 and Table 27-118 |
| NSS/NSTS | supported when advertised | One through eight streams, per-user and per-physical-RU validation | 27.3.11.8, 27.3.11.10, Table 21-13 |
| DCM | supported when advertised | Legal MCS/RU/NSS/format combinations; analytical rate and error-model input | 27.3.12 and Table 27-118 |
| BCC/LDPC | supported | Analytical encoder/codeword, shortening, puncturing, repetition, tail, padding, and LDPC-extra decisions | 27.3.12.5 |
| Packet extension and TXOP Duration | supported | PE and pre-FEC padding are in PPDU timing; TXOP supports UNSPECIFIED and IEEE quantization through 8448 us | 27.3.12 and format-specific HE-SIG-A tables |
| Doppler/midambles | deliberately unsupported | Trigger/HE-SIG codec fields parse and preserve their values, but TXVECTOR construction returns `UNSUPPORTED_DOPPLER_TIMING` because midamble insertion is not in the PPDU timeline | 27.3.10 and format-specific HE-SIG-A tables |
| Maximum PPDU duration | supported | Public validation rejects values beyond 5.484 ms without a usable TXVECTOR | Table 9-34 and Clause 27 |

## Representation and observation matrix

| Concern | Status | Contract |
|---|---|---|
| L-STF/L-LTF/L-SIG/RL-SIG/HE-SIG-A/B/HE-STF/HE-LTF/Data/PE ordering | supported | `Ieee80211HePpduLayout` provides immutable half-open time spans and exact total duration. |
| Logical L-SIG/RL-SIG and HE-SIG-A/B fields | supported | Independent golden bits/octets cover formats, widths, reserved values, CRC/tail, TXOP, Spatial Reuse, SIG-B compression, allocation, and user blocks. |
| Complete coded PHY bitstream or waveform | model-only | No FEC encoder/decoder, scrambler, interleaver, constellation, pilot, RF impairment, synchronization, CFO, or channel-estimation samples are produced. |
| PPDU container and recipient packet ranges | model-only | `Ieee80211HePpduLayout` describes concatenated packet-model ranges; it is immutable and is never serialized as an IEEE field. |
| TX handoff | model-only | `Ieee80211HeTxVectorReq` carries one canonical immutable TXVECTOR/layout pair from radio encapsulation to transmitter. |
| RX indication | model-only | `Ieee80211HeRxVectorInd` carries only format-appropriate received facts. HE TB Trigger-derived RU/MCS/NSS/FEC facts remain in `Ieee80211HeTbRecipientContextInd`. |
| Trigger correlation | model-only | `Ieee80211HeTriggerCorrelationTag` prevents a late or foreign response from owning another Trigger transaction; it is not a MAC field. |
| Per-RU propagation/interference/SNIR/PER | model-only | Scalar and dimensional media use the selected RU bandwidth/geometry. Packet and A-MPDU outcomes are explicit; no FEC decoding is claimed. |
| Received power/RSSI and SNIR | model-only measurement | Generic `SignalPowerInd`, `SnirInd`, and `SignalTimeInd` are attached to received packets. HE does not define a duplicate private RSSI tag. Trigger target RSSI remains a normative Trigger field. |
| Packet printer/dissector | supported | HE SU/ER SU/MU/TB retain `ieee80211HePhy` identity and are never reported as HT/VHT merely to reuse a shared packet-level implementation. |
| PCAP/radiotap | supported | Radiotap HE is populated from canonical TXVECTOR or RXVECTOR. TB user fields are supplemented only from the separate recipient context. HE-MU radiotap is omitted when the model lacks the required HE-SIG-B allocation codes. |
| Legacy/HT/VHT receive compatibility | supported | The `ax` profile admits applicable earlier modes by band; HE packets themselves retain HE identity. |

## Canonical type migration

No compatibility shim is retained.

| Removed or superseded interface | Replacement |
|---|---|
| `Ieee80211HeMuReq`, `Ieee80211HeMuCommonReq`, `Ieee80211HeSuErTxVectorReq` | Caller inputs use `Ieee80211HeTxVectorRequest`; `Ieee80211HeTxVectorFactory::create()` returns a validated immutable TXVECTOR/layout pair. |
| `Ieee80211HeMuTxTag`, `Ieee80211HeMuTxAllocationInfo` | `Ieee80211HeTxVector`, `Ieee80211HePpduLayout`, and immutable transactional DL/UL plans; packet ownership stays in the frame-sequence transaction. |
| `Ieee80211HeMuRxTag`, `Ieee80211HeMuRxAllocationInfo` | `Ieee80211HeRxVectorInd`; HE TB Trigger-derived recipient data uses `Ieee80211HeTbRecipientContextInd`. |
| `Ieee80211HeMuLegacyPreambleInd` | Format-neutral `Ieee80211LegacyPreambleInd`. |
| Wire-like `Ieee80211HeMuRuPayloadHeader` | Removed. Recipient PSDU ranges are non-serializable `Ieee80211HePpduLayout` data. |
| Parallel mutable HE header/request fields | One canonical vector is projected into the concrete HE header and checked again when reconstructed at reception. |

There are no compatibility aliases and no HE NED/INI parameter or result-name
renames in this final migration checkpoint. Existing parameter names whose
semantics remain accurate are retained. C++/MSG users of the removed types
must migrate as shown above.

## PCAP, result, and fingerprint trajectory

- HE SU is exported as radiotap HE rather than inheriting an HT/VHT capture
  identity. HE MU/TB fields now come from the same canonical contract used by
  the transmitter and receiver.
- Canonical signaling can change capture bytes where the old metadata was
  incomplete or nonstandard. Analysis should key on HE radiotap/PHY identity
  and represented fields, not removed internal tags.
- Result signals were not renamed. Receiver power and SNIR continue to use the
  generic packet-level indications and existing recorders.
- Fingerprint changes are not accepted automatically. The five plan-selected
  fingerprints are compared after deterministic tests; every mismatch must be
  classified from its first changed event, and committed CSV expectations may
  be changed only with separate user approval.

## Independent fixture provenance

The independent source and literal provenance for RU layouts, GI/LTF, L-SIG,
HE-SIG-A/B, Trigger, BSR, TWT, and analytical coding vectors is recorded in
[`80211ax-golden-vector-provenance.md`](80211ax-golden-vector-provenance.md).
