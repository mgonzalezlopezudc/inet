# 802.11ax supported-feature and modeling matrix

## Conformance profile and deterministic policies

The standards-oriented `opMode = "ax"` profile covers infrastructure AP and
non-AP STA operation. Support is band-aware for 2.4, 5, and 6 GHz and
capability-aware for channel width, NSS, DCM, LDPC, ER SU, puncturing, OFDMA,
and MU-MIMO. Optional behavior is required when both endpoints advertise the
corresponding capability; it is not silently enabled otherwise.

The profile supports 20, 40, 80, contiguous 160, and distinct 80+80 MHz
topologies. A flat 160 MHz allocation is not a representation of 80+80 MHz.

Where IEEE 802.11 permits an implementation choice, fixtures use these
deterministic model policies:

- choose the minimum legal HE-LTF count for locally constructed HE SU, ER SU,
  and DL MU PPDUs;
- preserve and validate Trigger-supplied GI/LTF type and HE-LTF count for HE TB;
- use zero HE-SIG-B padding, a fixed nonzero scrambler seed when a represented
  field needs one, and a deterministic AP content-channel split;
- treat analytical FEC/DCM/scrambling values as timing and error-model inputs,
  not generated coded bit streams.

The normative profile basis is IEEE Std 802.11-2024 Clauses 4.3.16 and 27.1.1.
In the status column, **supported** denotes required target behavior even when
the current implementation gap column records unfinished work.

## Matrix

| Area | Profile decision | Status | IEEE 802.11-2024 provenance | Current implementation gap |
|---|---|---|---|---|
| HE SU | Construct, serialize represented PLCP fields, calculate timing, and receive as HE SU | supported | 27.3.4, 27.3.11, Tables 27-20 and 27-32 | Ordinary HE SU still reuses HT/VHT identity on parts of the path |
| HE ER SU | Separate format with ER-specific legality and signaling | supported when advertised | 27.3.4, Table 27-20, Table 27-32 | Format exists but legality/profile coverage is incomplete |
| HE MU | DL OFDMA and MU-MIMO, HE-SIG-B, per-user PSDU/RU reception | supported when advertised | 27.3.4, 27.3.11.8, Tables 27-21 and 27-25 through 27-28 | Signaling and payload boundaries are partly custom/model-only |
| HE TB | Trigger-authoritative UL OFDMA/MU-MIMO, per-user reception | supported when advertised | 9.3.1.22, Tables 9-49 and 9-50; 27.3.4 and 27.3.11 | Calculator currently recomputes parameters that must come from Trigger state |
| 20 MHz | Full RU catalog and signaling | supported | Tables 27-7 and 27-8 | Catalog needs independent signed-subcarrier fixtures |
| 40 MHz | Full RU catalog, two HE-SIG-B content channels | supported when advertised | Tables 27-7, 27-9, and 27-26 | Independent placement/content-channel vectors incomplete |
| 80 MHz | Full RU catalog, center 26-tone signaling, puncturing | supported when advertised | Tables 27-7, 27-10, 27-26, and 27-27 | Center-RU and puncturing encoding are incomplete |
| Contiguous 160 MHz | 996/2x996 placement and two content channels | supported when advertised | 27.3.2, 27.3.11.8, RU placement text following Tables 27-8 through 27-10 | Geometry and validation are incomplete |
| 80+80 MHz | Preserve distinct segment topology and puncturing | supported when advertised | 27.3.2 and format-specific bandwidth signaling tables | Current scalar bandwidth type cannot distinguish it from contiguous 160 MHz |
| 2.4 GHz | HE plus applicable DSSS/ERP/HT compatibility | supported | 4.3.16 and band/mode rules in Clause 27 | Current `ax` mode set is not fully band-aware |
| 5 GHz | HE plus applicable OFDM/HT/VHT compatibility | supported | 4.3.16 and Clause 27 | Current `ax` profile admits an incomplete earlier-mode set |
| 6 GHz | HE-only band rules; do not inject inapplicable legacy PHY identity | supported | 4.3.16 and 6 GHz HE operation requirements | Current profile is not sufficiently band-aware |
| 26/52/106/242/484/996/2x996-tone RU | Exact legal placement, data/pilot counts, and allocation identity | supported | Tables 27-7 through 27-10 and 27-15 | Some validation uses compact offsets/indices without segment identity |
| Center 26-tone RU | Width-specific center-RU presence and content-channel bit | supported | Tables 27-26 and 27-27 | Partial implementation; needs positive and reserved-pattern vectors |
| Mixed RU layouts | All legal Table 27-27 partitions and reserved-code rejection | supported | Table 27-27 | Codec coverage is incomplete and puncturing input is ignored on one path |
| MU-MIMO users per RU | Up to eight users and eight total space-time streams per physical RU | supported when advertised | 27.3.11.8, 27.3.11.10, and Table 21-13 | Calculator grouping now uses segment center, tone size, and tone offset; the remaining scheduler/header contracts still carry parallel RU identities |
| Preamble puncturing | Validate legal patterns and encode/decode represented fields | supported when advertised | HE-SIG-A bandwidth/puncturing rules and Table 27-27 | Input is not consistently consumed by encoding |
| HE-LTF counts | Legal counts 1, 2, 4, 6, 8; DL uses minimum legal count, TB preserves Trigger value | supported | 27.3.11.10 and Table 21-13 | Calculator now derives the deterministic minimum from maximum per-physical-RU N_STS; TB Trigger authority is not represented in the PHY header |
| GI/LTF combinations | Validate the Table 27-32 subset for each PPDU format; generic durations exist for 1x+0.8, 1x+1.6, 2x+0.8, 2x+1.6, 4x+0.8, and 4x+3.2 microseconds | supported | Tables 27-13 and 27-32 | Calculator includes GI in LTF duration; format-specific validation is part of the current checkpoint, while the distributed request/header contract still lacks an explicit LTF type |
| MCS 0-7 | Mandatory baseline HE MCS set subject to NSS/bandwidth capability maps | supported | HE MCS/NSS capability maps and Table 27-118 | Current mode/profile classification is incomplete |
| MCS 8-9 | Capability-gated optional modes | supported when advertised | HE MCS/NSS capability maps and Table 27-118 | Negotiated directional capability handling is incomplete |
| MCS 10-11 | Capability-gated 1024-QAM; LDPC-only and DCM constraints enforced | supported when advertised | 27.3.12.5 and Table 27-118 | Some paths permit invalid coding/profile combinations |
| NSS/NSTS | Validate per-user and per-RU limits through eight streams | supported when advertised | 27.3.11.8, 27.3.11.10, Table 21-13 | Calculator validates per-physical-RU stream ranges; parallel scheduler, request, and header fields can still disagree |
| DCM | Enforce MCS/NSS/RU/format restrictions; feed timing/error model | supported when advertised | HE Data-field rules and Table 27-118 | Legality and calibration coverage are incomplete |
| BCC/LDPC | Enforce BCC/LDPC rules, including LDPC-only RU/MCS cases | supported | 27.3.12.5 | Rate-control and scheduler paths can fail to construct legal LDPC variants |
| Coded FEC bit stream | Do not emit or decode BCC/LDPC/interleaved bits | model-only | Packet-level abstraction decision; analytical values still follow 27.3.12 | Must remain clearly separated from serialized IEEE fields |
| Packet extension | Legal PE values and duration incorporated in common PPDU timing | supported | 27.3.12 and PE-related signaling tables | Boundary and golden duration vectors incomplete |
| Maximum PPDU duration | Non-throwing rejection at the standard maximum boundary | supported | HE PPDU duration limits in Clause 27 | Boundary coverage incomplete |
| L-STF/L-LTF/L-SIG/RL-SIG ordering | Correct represented field ordering and lengths for HE formats | supported | 27.3.4 and HE PPDU format figures | Current format identity/legacy-header reuse is incorrect on SU paths |
| HE-SIG-A | Format-dependent logical fields, widths, reserved values, CRC/tail where represented | supported | Tables 27-20 and 27-21 | Several fields have incorrect meanings/assignments |
| HE-SIG-B | HE MU only; allocation, common/user blocks, CRC/tail, padding, content-channel equalization | supported | 27.3.11.8, Tables 27-25 through 27-28 | Constrained MCS-0 estimator now counts CRC/tail per User Block and covers the 9-user/20 MHz boundary; MCS, DCM, compression, and actual RU-to-channel split remain absent from its API |
| HE-STF/HE-LTF/Data/PE timing | Exact analytical field and PPDU durations | supported | Tables 27-13 and 27-32; 27.3.12 | GI-dependent HE-LTF duration is corrected in the calculator; Trigger-derived LTF selection and complete format field timing remain open |
| Scrambling/interleaving/modulation samples | Do not generate physical coded/sample streams | model-only | Packet-level abstraction decision | Metadata must not be exposed as a claimed complete PPDU bitstream |
| Scheduler plans, queue handles, Trigger correlation, per-user SNIR/PER annotations | Non-serializable transaction/model metadata | model-only | No IEEE wire representation | Current custom HE RU payload header crosses this boundary |
| Legacy/HT/VHT receive modes | Admit only earlier modes applicable to the selected band and advertised profile | supported | 4.3.16 and Clause 27 compatibility requirements | Compact `ax` mode set is incomplete and HE SU can carry wrong legacy identity |

## Independent Gate 0 fixture provenance

The first fixture set is derived independently from these standard tables and
clauses:

- 20 MHz 26-tone RU signed-subcarrier and pilot catalog: Table 27-8.
- RU data/pilot totals: Table 27-15.
- HE-SIG-B allocation-code semantic boundaries: Table 27-27.
- HE-LTF count mapping: Clause 27.3.11.10 and Table 21-13.
- GI/LTF legality and durations: Tables 27-13 and 27-32.
- HE-SIG-B structure and accounting: Clause 27.3.11.8 and Tables 27-25,
  27-26, 27-27, 27-28, and 27-118.
- HE TB Trigger authority: Tables 9-49 and 9-50.
- Informative byte oracle: Annex Z, example 2 content-channel octets.

These fixtures must store both INET allocation-tree offsets and IEEE signed
subcarrier ranges; the two coordinate systems are not interchangeable.
