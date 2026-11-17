# IEEE 802.15.4 — implementation and validation results

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [coverage.md](coverage.md), [applicability.md](applicability.md)

## Current M1/M2 result

The selected unsecured, nonbeacon legacy-frame O-QPSK implementation is complete
through the plan's steps 0–10. The [statement mapping](statement-results.md)
accounts for 85 base-standard statements and two imported address definitions.
PIB-8 remains conditional: this whole-value service has no hierarchical element
index operation. That is an explicit binding limit, not an invented successful
INVALID_INDEX test. This result does not cover the plan's later feature series.

All final commands run in `/home/miguel/workspace/inet-ieee802154-standard`, unless
the table names another directory. The durable evidence bundle is
`/home/miguel/workspace/.codex-artifacts/ieee802154-plan-20261115-root/`.
It contains exact commands, normalized schema-v1 records, runner JSON, raw outputs,
capture files, parent controls, source manifests and intermediate commit checks.
The continuation starts at `0336bad8b225397f50938dea19c0f6ef111f8fb7`; source identity
and publication commit correspondence are recorded in `source-manifest.json` and
`publication-note-rewrite.json`. Later release-note corrections change no compiled
input. The unrelated user edit to AGENTS.md is preserved and excluded.

Isolated source-bearing commit checks pass in dependency order. The wire/capture,
PHY and MAC units build and execute their selected fixtures independently. The
example initially failed because it selected the public alias before its cutover;
its owning commit now explicitly selects StandardInterface and its two runs plus
12 scalar checks pass. The repaired cutover commit also passes its four fixtures.
For those last two checks, matching production objects and cached generated
outputs are reused only after verifying 4132 compiled inputs and feature state.
The subsequent matching make/relink passes; this is recorded cache reuse, not
fresh independent compilation of every translation unit. The interrupted
redundant rebuild and failed intermediate example attempt remain retained.
The new series' commit/classification/include/NED/release gates pass; the six
branch-wide trailer/subject violations belong to inherited commits.

| Gate | Observed final result | Bundle evidence |
| --- | --- | --- |
| Fresh debug and release builds | PASS, exit 0 in both modes | `debug-reset-review-build.log`, `final-release-build.log` |
| Native protocol suite | **17 fixtures, 564 simulations, 564 explicit PASS, zero FAIL**, exit 0 | `final-protocol.log`, `final-protocol-results.json`, `final-protocol-raw/` |
| Native module suite | **14 fixtures PASS**, exit 0: 72 completed simulations plus one expected UWB initialization failure | `final-module.log`, `final-module-results.json`, `final-module-raw/` |
| Native unit suite | **8 fixtures PASS**, exit 0 | `final-unit.log`, `final-unit-results.json`, `final-unit-raw/` |
| Native serializer | PASS: four frames across two captures, no raw fallback | `final-serializer-raw/serializer_ieee802154/test.out` |
| Generic serializer controls | Two FAIL on both parent and feature; identical failure signatures and totals | `serializer-control-comparison.json`, `serializer-control/` |
| Independent Scapy decode | PASS: ACK and source-derived DATA vectors with/without FCS | `final-scapy.log`, retained captures |
| External PHY calculation | PASS: 25 production transmission length/SNIR points and exact airtime | `final-module-raw/Ieee802154ExternalPhy_1/test.out`, [limits](external-validation.md) |
| Managed example | PASS: typed/raw, two complete join/DATA/POLL/leave cycles per device; 12 checked scalars | `final-managed-example.log`, `final-managed-example.csv`, `final-managed-example-check.log` |
| Legacy stored trajectories | PASS: Ieee802154Power for 100 s, Coexistence for 5 s, run 0, both stored ingredients | `final-fingerprint.log`, `final-fingerprint-results.json` |
| Legacy visual trajectory | PASS: Ieee802154Power for 100 s, run 0, stored tyf with fake GUI | `final-visual.log`, `final-visual-results.json` |
| IEEE802154 feature disabled | PASS: isolated debug build, empty-network runtime and absent native symbols | `final-disabled-build.log`, `feature-disabled-smoke.log`, `feature-disabled-symbols.log` |
| Scoped architecture/naming and paired interfaces | PASS for native MAC, PHY and application | `gates.json`, `architecture-*.log`, `naming-*.log`, `interfaces-mac.log`, `interfaces-phy.log` |
| Documentation links, seal consistency and release fragments | PASS | `links-*.log`, `document-seals.log`, final release checks |
| Independent lifecycle rereview | **24 PASS, 10 N/A, zero FLAG/QUESTION** | [availability and reset repair](phy-availability.md) |

The protocol count is Admission 2, Association 68, Broadcast 24, Contention 80,
DataAck 16, Indirect 62, InterframeSpacing 16, Lifecycle 42, Orphan 32,
PanConflict 28, PhyLifecycle 20, PhyPib 16, Reassociation 8, ReceiveSecurity 66,
ReceiverEnable 76, SequenceWrap 4 and UnslottedAccess 4. Seed-set is 0; all
declared typed/raw runs execute. Module multiplicities are Discovery 16,
PassiveScan 42, NativeGate 4 and eleven single-run fixtures, including the
expected initialization error. Normalized records count fixtures; raw output
counts distinguish simulations from fixtures.

`final-commands.json` records the explicit unit/module/protocol/fingerprint and
serializer commands, working directories and exit codes. Other direct commands
are `make MODE=debug -j6`, `make MODE=release -j4`, the managed example's
`inet --debug -u Cmdenv -r 0,1`, and its CSV-R checker. The feature control uses
`opp_featuretool disable -f Ieee802154`, a fresh matching debug build and an
empty-network Cmdenv run. No fingerprint or statistical baseline is updated.

### Nine English-check mapping

| Check | Direct evidence |
| --- | --- |
| C-WIRE | Fcs/FrameWire/Commands/Beacons/Capture units, native serializer and Scapy |
| C-ACCESS | UnslottedAccess, Contention, InterframeSpacing protocols and measurement modules |
| C-SEQUENCE | SequenceWrap, DataAck, Indirect, Admission protocols |
| C-RECEIVE | ReceiveSecurity protocol, NativeGate module |
| C-ACK | DataAck, Indirect, Broadcast, Association protocols |
| C-SECURITY | ReceiveSecurity and Admission protocols, opaque protected-frame codec boundaries |
| C-PHY | PhyServices/Measurements/ExternalPhy modules, PhyPib protocol |
| C-SERVICE | MacPib/PendingTransactions units, Lifecycle/PhyLifecycle/Admission protocols |
| C-SCAN | PassiveScan/Discovery modules, Orphan protocol |

### Known failures and limits

The ordinary generic PCAP suite has 4176 frames: 4174 identical, two different,
zero errors/truncations. Both differences are the existing Wi-Fi reassociation
capture frames 141 and 166. The known-failures suite has 341 frames: 301 identical,
24 different, 16 errors, zero truncations. The parent control has the same frame
failure signatures and totals. Diagnostic dumps after existing deserialization
exceptions contain varying bytes, so raw logs are retained without claiming
byte-identical full output. The native capture suite is independently passing.

Global architecture/naming/interface scripts exit 1 on inherited paths. Every
reported candidate file/directory was compared with the continuation HEAD and is
unchanged (`inherited-gate-findings.json`); scoped native checks pass. The source
seal guard still reports PcapReader.cc, as expected: the user explicitly approved
that exact bounded repair. No other sealed file changes. This is not trusted CI
seal approval. The branch-wide series gate also exposes inherited trailer/subject
violations; new commit checks are recorded separately. No PR description is
published by this execution.

Legacy UWB still fails initialization because its unchanged receiver does not
declare `ignoreInterference`. The expected-error fixture reproduces this on the
parent and feature; it does not prove working UWB. The model does not retract
already emitted packet-medium waveforms on local abort. The PHY-only lifecycle
fixture executes STOP/START, while the common crash dispatch is source-reviewed;
the broader MAC Lifecycle fixture separately executes crash. External BER/PER
validation checks a stated mathematical abstraction, not receiver hardware,
chip waveforms or quantitative whole-standard PER conformance. Security
transforms, enhanced frames, periodic beacons, slotted access, GTS, TSCH, DSME
and modern UWB are outside the selected M1/M2 profile.

The supplied skill-source package validation commands cannot run here: their
scripts and skill-suite tests are absent from this INET checkout. No skill
package files are changed. This is recorded as NOT_RUN, not a package-validation
pass. The sections below preserve earlier checkpoint evidence and their then-open
gates; they do not supersede this final result.

Initial baseline `fd6f800222`; continuation baseline `6cb03df7c8`. Working directory `/home/user/omnetpp_ws/inet-ieee802154-standards`.
The initial applicability pass used documentation/source/capture inspection without simulation
or a library build; that checkout had no `out/` or `src/libINET*` artifacts. Later package runs
are recorded separately below. All nine English protocol checks remain `NOT_RUN`. Neither the
initial pass nor the later value/store/contract fixtures establish a conformance verdict.

## Reproducible retrieval

Run from `/home/user/omnetpp_ws/inet-skills`. The launcher checkout is `d36cf47585eef165ba2cc7642fe4d0965616d6b8`; the source identity remains the hash in [source.md](../../standard/ieee802154/source.md).
Generated corpus output stays ignored and is not part of the evidence source tree.

```sh
./bin/inet_process_standards status \
  --standards-dir /home/user/omnetpp_ws/inet-ieee802154-standards/standards \
  --output /home/user/omnetpp_ws/inet-ieee802154-standards/standards/processed
./bin/inet_process_standards build \
  --pdf /home/user/omnetpp_ws/inet-ieee802154-standards/standards/802154-2024.pdf \
  --standards-dir /home/user/omnetpp_ws/inet-ieee802154-standards/standards \
  --output /home/user/omnetpp_ws/inet-ieee802154-standards/standards/processed
./bin/inet_process_standards lint \
  --standards-dir /home/user/omnetpp_ws/inet-ieee802154-standards/standards \
  --output /home/user/omnetpp_ws/inet-ieee802154-standards/standards/processed --json
```

Initial status reported a missing manifest. Build succeeded: 967 pages, 2814 nodes, 4107
occurrences and 3440 references, matching source hash prefix `851d81127297`. Lint exited zero
with 169 rejected false-heading candidates, one tiny node and 18 unresolved-reference warnings;
1148 duplicate-label candidates are informational. This is not a clean full-document reference audit.
An initial build using a relative `--pdf` path from the launcher checkout failed; the absolute-path
command above is the successful reproduction.

For each catalog source node, use the same directory arguments with these operations:

```sh
./bin/inet_process_standards get <canonical-node-id> --document ieee802154-2024 --children --json <directory-arguments>
./bin/inet_process_standards refs <canonical-node-id> --document ieee802154-2024 --json <directory-arguments>
```

The angle-bracket arguments above are templates; actual node IDs and physical page/source spans
are recorded per catalog entry. The initial 40 source excerpts were compared to their retrieved node text
with whitespace normalization. Twenty-seven distinct source nodes yielded 66 outgoing references,
all resolved. No request hit the 100-reference limit. Resolution does not establish applicability
or extraction of the target. In particular, the corpus places continuation prose under table/figure
nodes: retrieving only a parent clause does not retrieve all its normative text.

PDF pages 63, 64 and 91 were rendered and visually inspected for Figures 6-1, 6-2 and 7-15.
The inspected CSMA flowchart tests `NB > macMaxCsmaBackoffs`. The IFS figure shows the
size-dependent spacing after ACK. The ACK format has FCF, DSN and FCS, without addressing.
The continuation after Figure 7-16 was retrieved for the remaining ACK FCF rules.

## Current source findings, not runtime reproductions

| Surface | Reproducible observation | Evidence limit |
| --- | --- | --- |
| `src/inet/linklayer/ieee802154/Ieee802154MacHeaderSerializer.cc` | Fixed `0xCC01`; emitted header size is 2+1+2+8+2+8=23 octets; sequence write masks with `0xFF` | Source arithmetic, not an executed serializer fixture |
| `Ieee802154NarrowbandMac.ned` in the same directory | Default `headerLength=72 b` and MTU 127−9=118 bytes | The 9-octet declaration differs from the 23 emitted octets; actual byte-conversion failure remains to be reproduced |
| `Ieee802154Mac.cc` | `SeqNrParent` allocates per destination; `SeqNrChild` compares received `long` sequence against monotonically expected value with `<` | After serialized 255→0, this comparison can classify a fresh frame as old; no production wrap exchange was executed |
| `src/inet/networklayer/common/NetworkInterface.h` | Generic address/filter APIs use `MacAddress`; protocol data can be attached | Does not prove native-address tags, dispatch or upper consumers work |
| `src/inet/common/packet/recorder/PcapReader.cc` | Link type 195 sets FCS present; 230 sets FCS absent | Sealed source inspected only; no replay/writer correctness claim |

## PHY feasibility inspection

Read-only source inspection at the continuation baseline found an energy-observation path that
does not require a successfully decoded PPDU:

| Boundary | Source evidence | Remaining implementation/evidence |
| --- | --- | --- |
| Existing CCA | `Ieee802154Mac::updateStatusCCA()` checks reception state at timer expiry; duration is rxSetupTime + ccaDetectionTime | Does not establish observation over the complete interval |
| Medium observation | `IRadioMedium::listenOnMedium()` and `RadioMedium::listenOnMedium()` construct interference/background noise and invoke receiver listening decisions | Timed request, cancellation and terminal-result ownership |
| Scalar energy | `FlatReceiverBase::computeListeningDecision()` compares maximum noise power in W over an interval against the ED threshold | Quantitative ED requires averaging and calibration; a Boolean maximum-power result is insufficient |
| Available extension | Public virtual receiver listening decision; scalar noise exposes time-dependent power | Define a protocol-local measurement contract and check cache retention for energy that ends before completion |
| Propagation | IPropagation exposes speed and actual arrivals, but no universal maximum-delay guarantee | Pin a supported propagation model and finite geometry/movement assumptions or provide a separate explicit bound |

These symbols are under `src/inet/physicallayer/wireless/common/` except the existing MAC under
`src/inet/linklayer/ieee802154/`. The scalar medium supports compatible same-center/contained-band
interference; partial overlap has additional restrictions. Medium filters and retained interference
history must be pinned by the fixture. The existing instantaneous radio-state query does not retain
all information needed by a timed observation. No source change is needed in the sealed packet tree
for the candidate protocol-local provider/receiver extension.

The minimum production fixture is a timed CCA/ED request with an undecodable energy pulse wholly
inside the observation interval, ending before completion. Observe outcome, no PSDU delivery,
completion time, threshold cases and cancellation. A separate stationary two-radio fixture under
ConstantSpeedPropagation must compare actual arrivals with the declared ACK timing allowance.
These are proposed checks, not executed results; neither architectural feasibility nor declared
radio defaults establish PHY validation.

## Extended source audit

The continuation adds 42 source-checked statements, bringing the catalog to 82. It records all
31 MAC rows in Table 8-36 and 13 generic PHY rows in Table 12-2, including access markers,
PHY-specific domains and the absence of a PHY default column. Table 12-3 closes the peer-power
structure lookup. These field inventories are not 44 additional catalog statement IDs.

Fifty distinct catalog source nodes produced 122 extracted outgoing references, all resolved,
without query truncation. This is a first-hop target check, not transitive applicability closure.
Table 8-1 has 33 primitive rows (7 unstarred, 26 with starred references) and 73 distinct
cell labels; all 73 were resolved by direct target lookup. Service and PICS tables expose
cell references that the extractor omits; complete continuation inspection remains necessary. Annex E is informative; its generic
ED/LQI option labels do not override mandatory O-QPSK clauses 13.3.12/13.3.13. Extraction also
found false/unresolved Annex references, so the successful catalog-node edge check must not be
reported as a clean whole-standard reference audit.

Physical PDF page 84 was visually checked for the 7.2.11 independent CRC example. The transmitted
bit sequence corresponds to `02 00 6A`, with FCS `E4 79`; polynomial long division using
`x^16+x^12+x^5+1` independently reproduced the remainder. The resulting five-octet ACK has
352 µs PPDU duration under the selected O-QPSK format. This checks source arithmetic, not the
INET serializer. The exact octets and check procedure live in the
[wire check](../../protocol/ieee802154/checks.md#ieee802154-c-wire).

Four bounded specialist lanes supplied the mandatory/service/PICS inventory, PIB/PHY extraction,
receive interpretation and existing medium feasibility. Root integration retained source versus
model distinctions and did not accept the extraction summaries as conformance verdicts.

## Migration inventory

| Consumer | Required later evidence |
| --- | --- |
| `examples/wireless/nic/omnetpp.ini`: CSMAWithUnitDiskRadio, CSMAWithApskScalarRadio, CSMAWithApskDimensionalRadio | These select `Ieee802154Mac` with generic radios; identify explicit legacy retention versus supported replacement PHY before migration. |
| `showcases/wireless/ieee802154/`: Ieee802154, Ieee802154Power | Native-address/payload migration plus radio and power integration. The plain Ieee802154 row is commented out in `tests/fingerprint/showcases.csv`; do not count it as an active baseline. |
| `showcases/wireless/coexistence/`: Coexistence and WpanHosts | Mixed-radio medium behavior and upper-protocol adapters; preserve scenario meaning. |
| `.oppfeatures`: Ieee802154 | MAC and PHY package selection plus feature-disabled build; avoid accidental UWB changes. |
| `tests/serializer/lib/fillers/ChunkFillers_linklayer.cc` and `tests/serializer/REMAINING_GAPS.md` | Replace assumptions deliberately when the new chunk/codec lands; the capture is currently excluded as unrepresentable. |
| `tests/fingerprint/examples.csv`, `tests/fingerprint/showcases.csv` | Scoped before/after evidence for the named configurations; no baseline update in this pass. |
| `Ieee802154UwbIrInterface.ned` | Keep its distinct AckingMac composition; narrowband retirement does not retire UWB. |

This is a first consumer inventory, not proof that all callers have been migrated.

## Existing capture inventory

Executed from the INET checkout; inspection commands exited zero:

```sh
capinfos tests/serializer/pcap/ieee802154.pcap
sha256sum tests/serializer/pcap/ieee802154.pcap
tshark --version
tshark -n -r tests/serializer/pcap/ieee802154.pcap -T fields \
  -e frame.number -e frame.len -e frame.protocols -e wpan.frame_type -e wpan.seq_no
tshark -n -r tests/serializer/pcap/ieee802154.pcap -T fields \
  -e frame.number -e wpan.frame_type -e wpan.version -e wpan.fcs_ok
git log --follow --format='%h %s' -- tests/serializer/pcap/ieee802154.pcap
```

- File SHA-256: `7b8b59bc88f3a23fc979e41570646618b353c9779cb9aa3e9f0d97d6576d5855`.
- 440-byte classic PCAP, 13 packets, 208 captured data bytes, link type **195** (confirmed in
  the PCAP header; capinfos's internal encapsulation number 104 is not the link type).
- Decoder: TShark/Wireshark **4.6.4**. Observed types include legacy beacons/data, immediate or
  enhanced ACK, version-2 commands and multipurpose frames. The file is not a homogeneous M1 fixture.
- `wpan.fcs_ok` is false for frames 3, 5, 7, 9 and 12; empty values for other frames do not prove
  valid FCS. Capture admission must inspect the exact selected frames and decoder preferences.
- History traces to `0a30ea4a5b` and suite relocation `7287f347aa`. The former describes a mixture
  of Wireshark samples, derived and generated captures, without identifying this file's exact
  upstream source. Provenance remains incomplete. Container timestamps are not transmission timing
  validation; capinfos reports dates in 2104.

Do not repair captured bytes or relabel this file as a known-good standard vector. Step 3 needs
independent bounded vectors and explicit per-frame admission.

## Document validation

Executed from the INET checkout, all exited zero:

```sh
python3 doc/project/enforcement/check_links.py doc/project/evidence/standard/ieee802154
python3 doc/project/enforcement/check_links.py doc/project/evidence/protocol/ieee802154
python3 doc/project/enforcement/check_links.py doc/project/evidence/model/ieee802154
git diff --check
```

The three scoped link checks covered 2, 3 and 4 files respectively, with zero broken links.
The initial structural check confirmed 40 unique catalog headings and the 37-selected/3-deferred
partition. The continuation checks 82 unique catalog headings, nine unique check headings, every
catalog ID present in the feature map/checks/ledger, and the 79-selected/3-deferred partition.
Specification documents were checked for model class names and run-verdict leakage.

The broader command `python3 doc/project/enforcement/check_links.py plan/pending` exited 1:
four existing relative links in `pr-1155-resolve-audit-findings.md` do not resolve. None is in the
802.15.4 plan or changed by this pass; that separate plan was left unchanged. Documentation checks
are not simulation evidence, and no serializer, module, protocol or fingerprint result is claimed.

The targeted independent documentation review identified an overbroad group-ACK receive rule,
an invented ED cutoff and interrupted Markdown table rendering. All three were corrected and
confirmed resolved by the same reviewer. The stable results sections received no additional
actionable findings. This review did not exhaust transitive normative dependencies or verify
production behavior; the applicability gate remains open.

## Service and integration closure continuation

Baseline `837ee6dbe8`. Corpus status remained ready/fresh for the pinned IEEE 802.15.4-2024
source; unrelated 802.11 corpora remain absent. Retrieved 11.1.3.1 and Table 12-2 to close the
channel descriptor lookup. The selected M1 descriptor and its distinction from a legacy page
number are recorded in [applicability.md](applicability.md#selected-channel-descriptor).

The [IEEE 802-2024 publication record](https://ieeexplore.ieee.org/document/10935844/footnotes)
identifies the companion standard and its IEEE GET distribution route. Attempts to access the
GET page and PDF through the web tool failed (HTTP 418 and inaccessible PDF endpoint).
No normative companion text was retrieved; the group-address prerequisite remains open.
Publication metadata and informative address guidance are not replacements for that source.

The service lookup resolved direct-data error routing from 8.2.4.4/Table 8-5, 6.6.1–6.6.2,
9.2.2/9.2.4 and Table 8-31. The additional SERVICE-8 excerpt was compared against the complete
Table 8-5 continuation. The catalog now has 83 statements (80 selected, 3 deferred), with nine
English procedures; no executable results were added.

Read-only consumer tracing produced the
[native compatibility decision](applicability.md#native-address-compatibility-decision).
In particular, NetworkInterface's parameter named address is parsed as MacAddress, generic
ARP/ND/forwarding consumers remain 48-bit, and radio-medium address filtering does not provide
a native-address contract. Public attachment, tag, registration and classifier APIs provide the
candidate integration route. This is static source evidence, not successful module initialization
or an architecture-compliance verdict.

The clause-6 walk inventoried 28 descendant clause nodes and 14 table/figure nodes. Detailed
6.4/6.5 leaf retrieval included the passive-scan prose under Figure 6-3, and the 11 distinct
Table 6-1 IE targets resolved through direct lookup despite zero extracted table edges.
Root also retrieved 6.6.4–6.6.6 and Figures 6-7–6-13 to classify ATI/guard-time predicates and
preserve distinct lost-data versus lost-ACK checks. No new timing oracle was inferred from the
diagrams; this pass used their continuation prose. Full M2 service extraction remains deferred.

Continuation validation passed the three scoped link checks (2/3/4 files), git diff --check,
83 unique catalog headings with complete feature/check/ledger mapping, the 80-selected/3-deferred
partition, nine check headings and exact normalized comparison of the new SERVICE-8 excerpt.
Independent targeted review corrected SERVICE-8 metadata to the defined vocabulary and retained
mandatory scan termination at descriptor capacity. The same reviewer confirmed both findings
resolved. This was documentation verification; all executable checks remain NOT_RUN.

## Companion IEEE 802 source closure

The supplied `standards/802-2024.pdf` resolves the earlier retrieval failure. Its pinned identity
is in the [companion source record](../../standard/ieee802/source.md). The tracked processor's
reviewed document list does not include this document, so direct PDF fallback was used without
inventing a processed-corpus node ID or modifying the shared processor.

```sh
sha256sum standards/802-2024.pdf
pdfinfo standards/802-2024.pdf
pdftotext -layout standards/802-2024.pdf /tmp/ieee802-2024.txt
pdftoppm -f 41 -l 41 -scale-to 1500 -png -singlefile standards/802-2024.pdf /tmp/ieee802-address
pdftoppm -f 43 -l 43 -scale-to 1500 -png -singlefile standards/802-2024.pdf /tmp/ieee802-eui64
```

Commands exited zero. Physical pages 41 and 43 were visually inspected; clause 8.2.2 identifies
the group bit in conventional notation and Figure 10 identifies the first octet. Clause 8.5 on
physical page 49 distinguishes group MAC addresses from EUI identities. Combining that source
with IEEE 802.15.4 4.5.1 yields the masks and three synthetic vectors in C-WIRE. Their byte-order
arithmetic was checked independently; it is not an executed production codec or filter result.
Two companion definitions bring the ledger to 85 statements: 83 from IEEE 802.15.4 plus two
from IEEE 802, with 82 selected and three deferred. Source availability and the group-bit layout
are closed; reception/ACK policy remains governed separately by IEEE 802.15.4.

A targeted independent review confirmed the companion source, derived masks and vectors. It
also checked 7.2.2.5, 6.6.2 and 6.6.3.1: no additional receive rejection predicate invalidates a
nonbroadcast group frame solely because AR=1. The documented receive decision applies ordinary
filtering and ACK rules while preserving the sender prohibition. This closes that interpretation;
it does not assert that the violating injected transmitter conforms.

The four scoped link checks passed (2/2/3/4 files), as did whitespace checks and the 85-ID mapping,
82-selected/3-deferred partition and three synthetic byte-vector calculations. The added source
PDF remains an ignored local artifact; no executable runtime check has been added or run.

## Bounded M1 step-0 gate review

At baseline `5f3845f055`, the remaining malformed/capability policy was resolved using 8.2.2,
Table 8-11 and the ordered security early returns. SERVICE-9 records unsupported-parameter
INVALID_PARAMETER; ACCESS-3 makes the IFS size boundary explicitly checkable. Table 8-37 adds
22 Boolean capability/enable fields to the source inventory. Optional Data timestamping is
explicitly disabled; invalid typed MCPS metadata is a documented model representation.

The [dependency record](dependencies.md) classifies all 86 first-frontier targets. Expanding the
34 selected targets produced 80 occurrences: 79 resolved plus a non-protocol trademark footnote
that remains an unresolved parser record. The 18 additional target IDs contain one original
source (Figure 7-16), leaving 17 new to the original source/target union. Figure 4-5 was visually
checked on physical page 51; its outgoing-reference query returned zero. Table 9-6 selects level
zero, records level 4 as reserved/deprecated, and references only already reviewed 9.2.4.
Manual IFS root 6.3.1/Table 8-35 adds the 18-octet boundary. Predicate cuts stop disabled features
without declaring mandatory deferred profile capabilities inapplicable.

Independent standards review returned PASS for the bounded M1 applicability gate, then rechecked
the dedicated IFS addition and retained that verdict without findings. It did not assess runtime
support, a concrete propagation bound, integration fixtures or complete-device conformance.
Detailed mutation, event ordering and API representation remain the assigned implementation work.

Documentation validation after closure passed: scoped `check_links.py` runs for the base-standard,
protocol and model evidence directories (2/3/5 files, zero broken links), `check-seals.sh`
(19 document seal units, index consistent), and `git diff --check`. A structural check confirmed
87 unique catalog IDs, exact coverage-row correspondence, the 84-selected/3-deferred partition,
and nine English checks. These are documentation checks, not executable model evidence.

## Step-1a native address value

The first production subunit is the standalone
[Ieee802154Address](../../../../../src/inet/linklayer/ieee802154/Ieee802154Address.h) value, with
[implementation](../../../../../src/inet/linklayer/ieee802154/Ieee802154Address.cc) and a focused
[unit case](../../../../../tests/unit/Ieee802154Address_1.test). It has no existing MAC, PHY,
serializer, packet-tag or interface caller. Runtime simulation behavior is not changed by this
addition. The service-contract subunit is recorded below; production integration remains later work.

The pre-write contract was completed read-only by the implementer and independently validated
before write authorization. The value owns mode plus complete numeric identity, while parsing
validates before assignment. Wrong-mode access and throwing parse APIs use `cRuntimeError`;
nonthrowing parse refusal retains the prior value. There are no packet-ownership, timer, callback
or lifecycle paths in this subunit. The target subtree is unsealed and no shared-core edit is
needed. The contract deliberately defers byte order, MSG declarations and runtime integration.

Source classification uses IEEE Std 802-2024, 8.2.2 for the numeric bit-56 group flag and all-ones
broadcast; IEEE Std 802.15.4-2024, 6.2 for broadcast and 10.21.5.2/10.4.12.2 for `0xfffe` meaning
association without short allocation. Clause 6.6.1 independently excludes `0xfffe` and `0xffff`
from allocated short source values. All values remain representable, including group extended
values; constructing a value does not certify a valid device EUI identity.

Validation from the repository root, debug mode, default enabled feature set:

- `make MODE=debug -j8`: fresh full build passed, exit 0; rerun after adding the source passed
  and explicitly compiled `Ieee802154Address.cc`. Logs: `/tmp/802154-debug-build.log` and
  `/tmp/802154-address-build.log`.
- `MPLCONFIGDIR=/tmp/802154-matplotlib inet_run_unit_tests -m debug -f 'Ieee802154Address_1\.test'`:
  one executed case, PASS, exit 0. Log: `/tmp/802154-address-unit.log`; normalized envelope:
  `/tmp/802154-address-verification.json`. The earlier test compilation failed on three
  `std::string` arguments passed to the `const char *` constructor; the test calls were corrected
  and the same focused command passed. No runtime assertion failed in that earlier attempt.
- Scoped `check-architecture.sh` and `check-naming.sh` on `src/inet/linklayer/ieee802154` passed,
  as did `git diff --check`; implementer syntax and formatting checks also passed.

The unit test proves the standalone value API, including high-16-bit distinction through a hashed
container, sentinel/mode separation, strict format, malformed/null rejection and full-width boundary
values. It does not prove serialization, production delivery, frame exchanges or M1 support.
Release compilation and runtime/fingerprint campaigns have not been run for this subunit.

Independent review of the final three-file source/test change found no actionable correctness
findings: **11 PASS, 15 N/A, 0 FLAG, 0 QUESTION** under the general semantic checklist. Its API
visibility observation was resolved by making the comparison helper private; retained public mode,
comparison and stream operations have direct behavior assertions. The final debug rebuild and
focused unit rerun both passed after that change. Review artifact with exact file hashes:
`/tmp/802154-address-review.md`. No exception-ledger change or sealed-path approval was needed.

## Step-1a MAC service contracts

The bounded M1 facade adds paired, gate-free
[provider](../../../../../src/inet/linklayer/ieee802154/contract/IIeee802154MacServiceProvider.h)
and [client](../../../../../src/inet/linklayer/ieee802154/contract/IIeee802154MacServiceClient.h)
roles with matching NED interfaces. Protocol-local values represent DATA, GET, SET and RESET,
normal/promiscuous reception and COMM-STATUS. IEEE completion statuses remain separate from
local admission refusals and cancellation/reset/stop outcomes. This adds declarations and a
test-local provider; no existing MAC implements the facade yet.

The pre-write contract assigns packet ownership at acceptance, copies borrowed metadata for
deferred work, detaches pending state before callbacks, and permits inline completion. Request
IDs remain distinct from repeating MSDU handles and cannot be reused across refusal or lifecycle
boundaries. RESET is a barrier; client deletion requires cleanup without invoking a dead module.
The selected facade fixes LegacyTx and DataRate, keeps source identity in the future PIB owner,
and distinguishes wire-carried Source PAN ID from effective receive context. ACK RSSI's numeric
interpretation and uint8 representation are explicit local choices for Table 8-31's inconsistent
type entry. These are contract decisions, not evidence of operational IEEE service behavior.

The test-only [module fixture](../../../../../tests/module/Ieee802154MacServiceContract_1.test)
is the direct observation boundary for these ownership and callback claims. Production PIB
mutation, native interface delivery, PHY services and wire exchanges remain later packages.

Validation from the repository root, debug mode, default enabled feature set:

- `make MODE=debug -j8` passed, exit 0 (`/tmp/802154-service-build.log`). This establishes
  library freshness; existing runtime sources do not include the new facade. The implementer's
  standalone C++17 syntax check and the module fixture compile exercise the new headers.
- `MPLCONFIGDIR=/tmp/802154-matplotlib inet_run_module_tests -m debug -f 'Ieee802154MacServiceContract_1\.test'`
  compiled and executed one case, PASS, exit 0, General run 0. Raw log:
  `/tmp/802154-service-module.log`; normalized envelope: `/tmp/802154-service-verification.json`.
- The fixture checks inline completion and reentrancy, borrowed metadata copying, request-ID and
  MSDU-handle correlation, refusal/acceptance/indication ownership, packet destruction counts,
  DATA/GET/SET cancellation, stale completion, reset abort ordering, shutdown/crash/restart and
  client-deletion cleanup. It also exercises typed GET/SET values, unknown GET, separate
  promiscuous metadata and absent M1 timestamps. These are test-local contract checks.
- An intermediate assertion run failed because the fixture assumed newly created packets had
  no owner. OMNeT++ assigns the current module as owner. Corrected assertions check retained
  caller ownership on refusal and provider-to-client transfer on indication; the final focused
  rerun passed. Failure artifacts: `/tmp/802154-service-module-owner-failure.log` and `.err`.
- `opp_nedtool validate src/inet/linklayer/ieee802154/contract/*.ned`, scoped interface,
  architecture and naming checks, and `git diff --check` passed. NED interfaces declare parameter
  types only; concrete modules supply defaults and validate the typed binding.

Release compilation, operational MAC/PHY tests and fingerprint campaigns were not run for this
contract-only subunit. The nine normative protocol checks retain NOT_RUN verdicts.

Independent review of the final seven production declaration files and module fixture found no
actionable correctness findings: **18 PASS, 8 N/A, 0 FLAG, 0 QUESTION**. The reviewer confirmed
resolution of the fixture ownership assertion defect and the passing final run. Full checklist
and exact file hashes: `/tmp/802154-service-review.md`. Production MAC/PIB behavior and
reset-interruption edge paths remain unverified; no exception-ledger or seal changes were needed.

## Step-1b selected MAC PIB

The [Ieee802154MacPib](../../../../../src/inet/linklayer/ieee802154/Ieee802154MacPib.h)
[implementation](../../../../../src/inet/linklayer/ieee802154/Ieee802154MacPib.cc) owns the 43
selected MAC attributes described in the [mutation contract](pib.md). It is a synchronous class,
with no module, RNG, timers, callbacks or existing MAC caller. The future provider owns admission,
request tracking, effective operational changes and notifications. No mutable PHY PIB is owned here.

The implementation reuses the package-1a GET-confirm and SET-request values. GET returns canonical
unsigned integers, exact Boolean variants or native extended-address values. SET rejects unknown
names, read-only writes, invalid types, unsupported values and inconsistent BE pairs without
changing any attribute. Startup overrides validate as a complete candidate, independent of order.
Reset(false) retains all values; reset(true) restores standard/profile defaults using an injected
DSN and retaining device identity and immutable PHY initialization context, without replaying
startup overrides. Store reset alone is not evidence of an operational MLME reset.

The architecture and normative lanes preceded a complete read-only implementation contract;
the root validated that contract and separately authorized the three source/test files. Normative
review confirmed the explicit local representations for unspecified coordinator context and the
backoff range. Exact table/RESET extraction is in `/tmp/802154-1b-pib-evidence.md`; the tracked
catalog and mutation contract preserve its relevant rules and locators.

Validation from the repository root, debug mode, default enabled feature set:

- `make MODE=debug -j8`: PASS, exit 0; explicitly compiled `Ieee802154MacPib.cc` and linked the
  debug library. Log: `/tmp/802154-pib-build.log`.
- `MPLCONFIGDIR=/tmp/802154-matplotlib inet_run_unit_tests -m debug -f 'Ieee802154MacPib_1\.test'`:
  one executed case, PASS, exit 0. Log: `/tmp/802154-pib-unit.log`; normalized envelope:
  `/tmp/802154-pib-verification.json`.
- Scoped `check-architecture.sh` and `check-naming.sh` passed for
  `src/inet/linklayer/ieee802154`; formatting and `git diff --check` passed.
- Additional static C++ tooling was not run successfully: its compilation database was absent,
  and the include checker stopped on a UTF-8 error in historical commit data. These are tooling
  limits, not passing checks or failures of the focused unit case.

The [unit case](../../../../../tests/unit/Ieee802154MacPib_1.test) independently enumerates 43
expected defaults/access markers, checks exact-name lookup and canonical value types, rejected
writes and state preservation, writable boundaries, unsupported capability enables, full-width
identity, startup errors and BE ordering, CCA rounding, and reset retention/default restoration.
Three draft fixture mistakes (a group-address identity, writing the unknown-coordinator sentinel,
and a successful write inside a failure-only snapshot) were corrected before the first test run.

This evidence establishes the compiled store's behavior only. Production service integration,
NED parameter import, operational change notifications, PHY reset, and RNG stream/draw scheduling
remain provider/composition work. Release compilation and simulation/fingerprint campaigns were
not run. The nine normative protocol-check verdicts remain NOT_RUN.

Independent review of the stable source/test change returned **11 PASS, 15 N/A, 0 FLAG,
0 QUESTION**, with no actionable correctness findings. It independently reran the scoped
architecture and naming checks. Exact file hashes and the full checklist are recorded in
`/tmp/802154-pib-review.md`. Documentation links, seal-index consistency and whitespace checks
also passed. No exception-ledger, source-seal or baseline changes were needed.

## Step-1c PHY service contracts

Executed on 2026-11-15 in `/home/miguel/workspace/inet-ieee802154-standard`, on continuation
baseline `0336bad8b225397f50938dea19c0f6ef111f8fb7`, debug mode, default enabled features.
The [paired declarations](../../../../../src/inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceProvider.h)
and [timing/ownership contract](phy-services.md) define a structural extension API. They add no
operational provider and change no existing MAC, PHY, interface defaults or wire format.

The restored `/home/miguel/workspace/inet-standards/802154-2024.pdf` has 967 pages and SHA-256
`851d81127297f898c956fdd1ce92929324d68cd696fd338d1e876f6a372f22d3`, matching the pinned source.
The `inet_process_standards` launcher in `/home/miguel/workspace/inet-skills` rebuilt the ignored
corpus with 2,814 nodes and 4,107 occurrences. Rechecked sources include 11.1.3.1, 11.2.8,
the PHY services prose on page 600, 12.3.1 and Tables 12-2/12-3. The selected channel, CCA
and PHY PIB nodes' outgoing references resolved. Corpus lint reported 1,148 informational
duplicate-label candidates, 169 false-heading warnings, one tiny node and four unresolved
references outside the selected contract source passages; this is not a clean corpus-wide lint.
Table 12-2 was also checked visually: transmit power uses signed integer dBm. The standard
explicitly leaves PD-SAP/PLME-SAP undefined, so method shapes and result codes are local API
choices. Raw retrieval/lint artifacts are `/tmp/ieee802154-1c-*.json` and the build log is
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-1c-corpus-build.log`.

Selected resolved corpus locators (character spans in the regenerated document):

| Node | Physical PDF pages | Span |
| --- | --- | --- |
| `clause:11.1.3.1` | 565–566 | `2258179:2259441` |
| `clause:11.2.8` | 596–597 | `2373832:2378774` |
| `figure:12-1` continuation | 600 | `2385244:2386315` |
| `table:12-2` | 601–603 | `2389232:2391191`, `2391191:2396657` |
| `table:12-3` | 603 | `2396657:2397656` |

Validation from the repository root after `source setenv -q`:

```sh
make MODE=debug -j8
opp_run_module_tests --load . -p inet -m debug --no-build \
  -f 'Ieee802154(MacServiceContract|PhyServiceContract)_1\.test$'
opp_run_unit_tests --load . -p inet -m debug --no-build \
  -f 'Ieee802154(Address|MacPib)_1\.test$'
```

The debug build passed, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-1c-build-debug.log`). The module run compiled
and executed two cases, **2 PASS**, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-1c-module-combined.log`); the unit
run compiled and executed two cases, **2 PASS**, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-1c-unit-combined.log`).
The new [PHY fixture](../../../../../tests/module/Ieee802154PhyServiceContract_1.test) fixes
General run/seed 0 and tests typed values, full-width peer identities, atomic channel validation,
admission versus completion, accepted/refused packet ownership, destruction before confirmation,
scheduled versus actual transmit timing, readiness and measurement intervals, cancellation,
stale completions, reentrant callbacks, reset barriers, stop/crash/restart and client deletion.
Synthetic receive events check exact-deadline precedence and rejection one simulation tick late.
Initial fixture compilation/setup failures (generated test namespace placement and absent
lifecycle support) were corrected before the passing focused and combined runs.

Scoped architecture, paired-interface and naming checks passed for the new PHY contract
subtree. `opp_nedtool validate` passed for both new NED interfaces. Seal-index consistency,
`whatsnew.py check`, `git diff --check` and link checks scoped to this model's evidence and
`plan/pending` passed. The full `doc/project` link gate failed with 181 missing standards-file
links outside this package, with no missing anchors or IEEE 802.15.4 findings; log:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-1c-links-global.log`. The unrelated root `AGENTS.md` edit was preserved.
No exceptions, seals or baselines were changed. No independent review was executed for this
package.

This is test-local contract evidence. Actual radio/medium timing, CCA/ED behavior, propagation
bounds, PHY-to-MAC PIB context refresh and production packet exchanges remain steps 4–6 work.
Release compilation, fingerprints and normative protocol tests were not run; the nine protocol
verdicts remain `NOT_RUN`. Package 1d, address/payload integration, is next.

## Native integration, codec and energy continuation

The 2026-11-15 continuation preserves baseline `0336bad8b225397f50938dea19c0f6ef111f8fb7`
and the unrelated `AGENTS.md` edit. All additions are currently working-tree changes. Source
paths are under `linklayer/ieee802154` and `physicallayer/wireless/ieee802154`; no sealed
packet source, defaults, UWB composition or fingerprint baselines changed.

From the repository root after `source setenv -q`, fresh debug builds passed after each compiled
source addition (`make MODE=debug -j8`). Build logs are `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-native-build.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-composition-build.log`, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-codec-build.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-tooling-build.log` and `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-energy-build.log`.

| Exact runner arguments after `--load . -p inet -m debug --no-build` | Executed result | Raw log |
| --- | --- | --- |
| `opp_run_module_tests -f 'Ieee802154(Composition\|NativeInterface)_1\.test$'` | 2 PASS, exit 0 | `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-integration-module.log` |
| `opp_run_unit_tests -f 'Ieee802154(FrameWire\|Fcs)_1\.test$'` | 2 PASS, exit 0 | `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-codec-unit.log` |
| `opp_run_unit_tests -f 'Ieee802154Capture_1\.test$'` | 1 FAIL, exit 1 | `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-unit.log` |
| `opp_run_module_tests -f 'Ieee802154EnergyObservation_1\.test$'` | 1 PASS, exit 0 | `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-energy-module.log` |

The native fixture crosses actual `NetworkInterface` gates and the production service adapter,
with a test-local provider. Distinct EUI-64 high bits survive attached interface data, typed
requests and receive tags. Sender metadata cannot choose the receiving upper protocol; opaque
delivery has no protocol tag. Inline confirmations and refused admission produce separate typed
results. The all-production replacement composition publishes the selected PIB identity and
explicitly refuses unavailable DATA capability. It is not yet a radio exchange.

The frame unit fixture checks independent extended/compressed and short/uncompressed bytes,
both legacy versions, all selected address/PAN presence combinations, DSNs 254/255/0/1,
registered serializer conversions and bounded malformed/unsupported/truncated decoding. The
FCS fixture checks the standard's ACK oracle and CRC-16/KERMIT check string, invalid/absent
FCS preservation and 127-octet bounds. No protocol exchange verdict is inferred from these.
An initial fixture held a reference through a temporary converted byte chunk; retaining that
chunk corrected its lifetime. A dissection flag initially included PF_ALLOW_EMPTY, which is
invalid for typed peeks; the final implementation uses the three explicit error allowances.

The capture fixture reaches the actual PCAP reader/writer. Complete records with link types
195/230 retain MPDU bytes and FCS-presence metadata. The truncated case fails because the
reader fabricates zeros up to `orig_len`; the [repair proposal](capture-reader-repair.md)
contains the reproduction and protected-path scope. That result remains a blocker, not a pass.

The energy fixture uses a real scalar medium, an O-QPSK receiver and BPSK interferer. Background
power is measured without PPDU decoding. Two 832-us undecodable transmissions contribute their
copied scalar powers over a 2-ms window; peak and integrated mean match the independent duty
cycle calculation. No packet is delivered. Fixed General run/seed 0. Initial setup failed
because generic radio energy detection rejected high background noise; explicitly raising that
radio-state threshold allows the independent energy observation and demonstrates why state is
not a substitute for measurement. An initial interferer lacked its required protocol tag;
the final fixture supplies it.

Scoped link-layer architecture/naming and release-note checks passed during this continuation.
Remaining whole-plan gates include M2 applicability/services, capture/external validation,
migration, both-mode final builds and scoped regressions. The later record below supersedes
the operational M1 work marked pending in the preceding continuation record.

## Operational direct-data verification

On 2026-11-15 the production MAC, unslotted access provider, timed PHY provider and scalar
radio passed the following batch against a fresh debug library. Baseline remains
`0336bad8b225397f50938dea19c0f6ef111f8fb7` plus working-tree changes; this is not an immutable
release revision. The command from the repository root was:

```sh
source setenv -q
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/ieee802154$' \
  -f 'Ieee802154(UnslottedAccess|DataAck|ReceiveSecurity|SequenceWrap|Lifecycle|PhyPib|InterframeSpacing|Contention)\.test$'
```

Exit status was 0: **8 test files PASS**. Per-run output independently contains **216 PASS,
0 FAIL** verdicts. An earlier selection accidentally omitted the `.test` suffix and executed
zero cases; it provides no evidence. The corrected command above is the recorded batch.

| Production protocol fixture | Explicit passing runs | Claim and limits |
| --- | ---: | --- |
| `Ieee802154UnslottedAccess.test` | 4 | Actual idle/busy CCA, serialized DATA and access failure, typed/raw radios |
| `Ieee802154DataAck.test` | 6 | ACK timing, first/all ACK loss, stable DSN and bounded retry exhaustion |
| `Ieee802154ReceiveSecurity.test` | 48 | 24 receive/filter/security cases on both radio representations; decision coverage, not mutated-frame airtime validation |
| `Ieee802154SequenceWrap.test` | 4 | 260 requests, device-wide DSN wrap across peers, retry stability and receiver restart |
| `Ieee802154Lifecycle.test` | 42 | Five asynchronous states, cancel/reset/stop/crash and initial DOWN; local cleanup and replacement progress |
| `Ieee802154PhyPib.test` | 16 | Actual CCA durations 1/17/128/1000000 us, MLME delegation, atomic failures and reset context |
| `Ieee802154InterframeSpacing.test` | 16 | 18/19-octet boundary, ACK/unacknowledged and broadcast, overlapping receiver readiness |
| `Ieee802154Contention.test` | 80 | Seeds 0–19, two/five contenders on both representations; bounded terminal results, no fairness/PER claim |

Raw batch log: `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-m1-final-protocol.log`. Preserved run output, error logs,
configuration and captures: `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-m1-final-evidence/`; per-file counts are in
`verdicts.json`. Reviewer companions live beside the test files. Diagrams have not been
render-checked. No stored fingerprint or statistical baseline was updated.

The indivisible medium-signal limitation in [lifecycle.md](../../../../../tests/protocol/ieee802154/lifecycle.md)
remains: aborting a local on-air operation cannot retract its already emitted packet-level
signal. These tests do not establish physical cancellation of remote energy or reception.
The protected PCAP reader defect above remains unresolved. M1 release closure and the M2
management claim remain pending; passing this batch does not establish either.

## Indirect services and refreshed regression batch

Production pending DATA/POLL/PURGE and the managed PIB extension built in debug mode on
2026-11-15, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-poll-build.log`). Baseline remains
`0336bad8b225397f50938dea19c0f6ef111f8fb7` plus working-tree changes. The source/protocol-fixture
and matching debug-library manifest is `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-source-manifest.json`, SHA-256
`90e1837c6fffbb22f0bbdd1fe981b84a85c92db07ca326c98bf255ec8375a0e3`. This identifies that tested
state before the subsequent approved capture-reader repair; it is not a final release revision.

The preceding operational protocol command with `|Indirect` added to its explicit filter
passed **9 test files**, exit 0, with **266 PASS, 0 FAIL** run verdicts. Log:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-protocol.log`; output/configuration/captures and per-file counts:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-protocol-evidence/`. The total comprises the preceding 216 cases,
six new command/beacon receive-boundary cases and 44 indirect cases.

A subsequent focused receive run used:

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/ieee802154$' -f 'Ieee802154ReceiveSecurity\.test$'
```

It passed **66/66 runs**, exit 0, zero FAIL verdicts. Log:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-receive-expanded-protocol.log`; artifacts:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-receive-expanded-evidence/`. This replaces the 54-case receive result in the
combined batch; it does not imply another full batch. New predicates include reserved frame
type/version/source mode, local/broadcast short addresses and a foreign source PAN at a PAN
coordinator. These are receive-decision checks, not mutated-frame PHY timing validation.

The [indirect companion](../../../../../tests/protocol/ieee802154/indirect.md) specifies all
44 runs: sleeping receivers, full native identities, FIFO/Frame Pending, loss, source omission,
empty responses, exact expiry, and reset/stop reentry. IndirectTx is correctly ignored under
Table 8-30's non-coordinator/omitted-destination predicates. Association-command integration,
broadcast indirect release and active-extraction interruption coverage remain open.

The matching debug unit command
`opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154(Beacons|Commands|FrameWire|Fcs|PendingTransactions|MacPib|Address)_1\.test$'`
passed **7 cases**, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-unit.log`). The PIB fixture now checks
52 selected attributes, managed bounds, payload bytes, sequence injection and selective
association-reference cleanup. This does not prove association operation.

## Approved captured-length repair and serializer admission

The user explicitly authorized the bounded change to
`src/inet/common/packet/recorder/PcapReader.cc` on 2026-11-15. Only the captured-length resize
and preceding offset check changed in that sealed file. The debug rebuild passed, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-repair-build.log`). The original reproduction now passes, including
complete/truncated stripping and an oversized link-header offset:

```sh
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154Capture_1\.test$'
```

One case PASS, exit 0; `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-repair-unit.log` and
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-repaired-evidence/`. This supersedes the earlier capture FAIL/blocker.
Scapy 2.8.0 also passed the complete ACK captures in both FCS modes; log:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-repaired-decoder.log`.

The module command used the explicit filter
`Ieee802154(Composition|NativeInterface|PhyServiceContract|PhyServices|EnergyObservation|UnslottedAccess|MacServiceContract)_1\.test$|PcapRecorder(Radiotap|ReceiverCopy)_1\.test$`
with `opp_run_module_tests --load . -p inet -m debug --no-build`. All **9 cases PASS**, exit 0,
including the existing real Wi-Fi radiotap/plain capture comparison and receiver-copy recorder.
Log/artifacts: `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-module.log`, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-module-evidence/`.
The repaired radio-disabled composition fixture now permits its intentionally unused radio input.

Serializer workflow discovery resolved `opp_run_opp_tests`. From `tests/serializer`, after
`source ../../setenv -q`, the command was:

```sh
opp_run_opp_tests --load ../.. -p inet -m debug --no-build -f 'serializer_ieee802154\.test$'
```

One case PASS, **2 captures / 4 frames identical**, typed `Ieee802154Frame` use and no raw
fallback, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-admitted-serializer.log`). The independently generated
[fixtures](../../../../../tests/serializer/pcap/generated/ieee802154-m1.md) have SHA-256 values
`25244aa231821dbfbc6305141d6b0644df8b2fafdf1fd110d46977810db8b945` (with FCS) and
`8d6265118cdc95185973df25d9cbc47473a768736c8355abb7654b353b8ef762` (without FCS).
Scapy independently checked both DATA layouts and CRCs; `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-vector-decoder.log`.
Their generator does not import INET. The old mixed capture and stored baselines are untouched.

The additional generic `serializer_pcap(_known_failures)?\.test$` run executed two cases and
both failed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-serializer.log`). The ordinary corpus has two byte
mismatches in unchanged IEEE 802.11 processing, frames 141/166 of the derived Block Ack capture;
the known-failure corpus has two additional differences relative to its pinned count. Both
relevant input captures use link type 105 and have `incl_len == orig_len` in every record
(209 derived and 218 original). The bounded reader change therefore leaves their input bytes
unchanged. This is input/ownership-path attribution, not an executed parent-build comparison.
Those failures remain visible; expectations were not refreshed. An initial serializer filter
anchored at the start matched zero cases and is NOT_RUN; the corrected commands omit that anchor.

## Quantitative PHY checks and refreshed protocol batch

After the desired-signal-power correction and 20-dBm radio maximum alignment,
the fresh debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-quality-build.log`). The module command
`opp_run_module_tests --load . -p inet -m debug --no-build -f 'Ieee802154(Measurements|Composition|NativeInterface|PhyServiceContract|PhyServices|EnergyObservation|UnslottedAccess|MacServiceContract)_1\.test$'`
passed all **8 cases**, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-quality-module.log`).
The production measurement case verifies 18 tabulated powers, ED/LQI saturation,
measurement durations, reception-forced busy CCA, channel isolation and actual
20-dBm transmission. The scalar-channel assumptions and power correction are in
[phy-provider.md](phy-provider.md). This is model calibration evidence, not hardware BER/PER validation.

The refreshed full command
`opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154.*\.test$'`
passed **9 files / 288 simulation runs**, exit 0, zero FAIL verdicts.
Log: `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-quality-protocol.log`; copied outputs and per-file counts:
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-quality-protocol-evidence/`. Counts are Access 4, DATA/ACK 16,
Receive/Security 66, Sequence 4, Lifecycle 42, PHY PIB 16, IFS 16,
Contention 80, and Indirect 44. This supersedes the earlier 266-run combined batch.
It precedes the subsequent START implementation.

## Non-beacon coordinator START

The debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-start-build.log`).
`opp_run_module_tests --load . -p inet -m debug --no-build -f 'Ieee802154Start_1\.test$'`
passed **1 production module case**, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-start-module.log`).
It verifies NO_SHORT_ADDRESS, invalid-channel atomicity, successful PAN/channel
publication, role activation through queued indirect DATA, BUSY with queued work,
retain/default MAC RESET and retained PHY channel. The fixture explicitly selects
initial channel 11; the provider's default is 20. Initial fixture compile/setup
failures were corrected before this passing result. Scan/join operation and
asynchronous alternative PHY bindings are not established by this case.

## Passive and ED scanning

The scan debug builds passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-scan-build.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-ed-scan-build.log`). The command
`opp_run_module_tests --load . -p inet -m debug --no-build -f 'Ieee802154(PassiveScan|Start)_1\.test$'`
passed **2 module files**, with **28 scan simulation runs** plus one START run,
exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-ed-scan-module.log`). Scan outputs/INI are copied to
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-ed-scan-module-evidence/`.
All 28 runs print their terminal success message; the test also forbids errors
on stderr. Fourteen passive runs check ascending/deduplicated channel iteration,
full EUI-64 descriptor identity, beacon deduplication, notification/list policy,
capacity, zero dwell, invalid channel and cancel/reset during receive readiness.
An irrelevant DATA PPDU crosses the dwell boundary, exercising receiver-off before
retuning. Fourteen ED runs check repeated-measurement peaks, actual high-power
PPDUs versus off-channel noise, one-symbol dwell, and cancel/reset restoration,
including cancellation during an ED measurement. Original channel 20 and CCA
duration 17 us must be restored; current MAC backoff remains 20 symbols.

Limits remain explicit in [scanning.md](scanning.md): zero-duration ED is an
accepted UNSUPPORTED_FEATURE result, protected-beacon failure descriptors are
not yet implemented, and active/orphan scans remain unsupported admission.
This is partial M2 evidence, not closure of step 8 or a complete passive-scan
device-profile claim. An initial fixture assertion incorrectly expected a PHY
CCA write to overwrite the current MAC backoff; source inspection established
that it updates the reset context, and the assertion was corrected.

## Active discovery and beacon services

The debug beacon build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-beacon-build.log`).
`opp_run_module_tests --load . -p inet -m debug --no-build -f 'Ieee802154Discovery_1\.test$'`
passed **1 file / 14 simulation runs**, exit 0, all 14 terminal messages present
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-discovery-module.log`, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-discovery-evidence/`).
Both peers use production MAC/PHY interfaces. Cases cover automatic response,
AutoRespond=false with/without higher-layer BEACON, non-coordinator silence,
oversize generated beacon, cancellation during readiness, and RESET during the
actual Beacon Request PPDU. Successful discovery returns native descriptors.
The fixture checks independent BSN 255→0→1 while DSN remains 7, actual wire FCS,
legacy source-only layout and superframe role/association-permit bits. Explicit
enhanced, invalid-channel, security and oversized requests fail without consuming
BSN, and a later valid request succeeds. A missing required fixture interface
parameter was corrected before the passing run.

The focused protocol regression after scan integration also passed
`Ieee802154(Lifecycle|Indirect)\.test$`: **2 files / 86 runs**, exit 0,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-managed-regression.log`. This predates beacon integration and
does not establish a refreshed full protocol batch after that later change.

The subsequent descriptor debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-descriptor-build.log`).
The combined `Ieee802154(Discovery|PassiveScan|Start)_1\.test$` module command
passed **3 files / 53 simulation runs** (38 scan, 14 discovery, one START), exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-descriptor-module.log`). The scan increment adds protected legacy
beacon failure descriptors preserving opaque body/full identity/BSN and excluding
plaintext interpretation, plus empty-beacon NotifyAllBeacons and AutoRequest
combinations. This is unavailable-security reporting, not unsecuring support.
The expanded pending-address unit also passed, including full identity uniqueness,
PAN/expiry exclusion, short-before-extended order and the seven-address limit
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-pending-address-unit.log`).

The refreshed full production protocol command with filter `Ieee802154.*\.test$`
then passed **9 files / 288 runs**, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-discovery-protocol.log`).
Copied outputs, INIs and exact per-file verdict counts are in
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-discovery-protocol-evidence/`. This batch includes the beacon,
active-scan and failure-descriptor implementation and supersedes the earlier
pre-START full batch. It does not imply association/recovery or release-gate completion.

## Shared indirect Association Response

The debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-association-response-build.log`).
The focused `Ieee802154Indirect\.test$` protocol command passed **62 runs**,
62 explicit PASS and zero FAIL, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-association-response-protocol.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-association-response-evidence/`). Nine new typed/raw scenarios
cover response extraction, lost ACK/new poll with stable DSN, expiry, command
exclusion from PURGE, mixed command/DATA FIFO, RESET, cancel, overflow and a
rejection response. Command FP remains zero with DATA pending; command extraction
returns POLL NO_DATA and no MSDU. Source completion is a correlated native
carrier of COMM-STATUS. The higher-layer fixture supplies the response directly;
device association state and production Association Request are still unimplemented.
The full batch above predates this response increment and remains 288 runs;
the new full-batch scope will be 306 runs when rerun.

## Production association controller

The debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-association-build.log`).
The new `Ieee802154Association\.test$` protocol command passed **26 simulation
runs**, 26 explicit PASS, zero FAIL, exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-association-protocol.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-association-evidence/`). The complete production path includes
default RESET, active scan, higher-layer PAN selection, Association Request/ACK,
decision wait, indirect response extraction and native post-join DATA/ACK.
Thirteen typed/raw cases cover short and extended-only success, rejection,
AssociationPermit=false, missing response, cancel/reset during decision wait,
invalid channel, response expiry, short coordinator/extended-reference learning,
request ACK loss, response ACK loss, and RESET during a poll PPDU.
Native clients assert no association success before request ACK plus 30.72 ms.
The response-ACK-loss case accepts the received response at the device while
the coordinator retains it for a new poll; the test explicitly cancels it at 65 ms.
It does not claim coordinator SUCCESS or retract an emitted waveform.

Fixture discovery corrected an unavailable DSL method, missing TapRadioMedium
noise configuration and a listener which scheduled a timer without entering its
own module context. All 26 runs passed after these fixture corrections.
The subsequent allocated-short/EUI lookup extension and its 28-run scope are
pending execution; the 26-run result does not establish that extension.

The allocated-short/EUI extension then passed **28 runs**, zero FAIL, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-associated-poll-protocol.log`, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-associated-poll-evidence/`).
A joined short-address device polls for DATA queued by EUI; actual wire metadata
retains the EUI destination and original payload. The member-reference
representation change subsequently passed the combined Association/Indirect
scope (**28 + 62 = 90 runs**, exit 0,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-member-reference-protocol.log`). It retains extended-only member
references in the same authoritative PAN/EUI-to-short table. The later
disassociation code and its proposed 42-run association scope are not covered by
these earlier results.

## Disassociation and peer cleanup

The debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-disassociation-build.log`).
The focused Association protocol command first passed 42 runs, then its expanded
scope passed **66 runs**, 66 explicit PASS/zero FAIL, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-disassociation-expanded-protocol.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-disassociation-evidence/`). The
[companion](../../../../../tests/protocol/ieee802154/association.md) specifies
all selected direct/indirect leave/removal, failure, identity and cleanup cases.
Confirmed boundaries include short and extended-only members, command POLL
NO_DATA, all five device PIB references cleared, and coordinator cleanup even
after actual access/expiry/ACK failure. Sleeping or unnotified devices retain
their local association; no remote state change is invented.

The same-PAN/full-EUI peer drain destroys all affected packets before callbacks,
preserves another peer's queue, refuses RESET admission while draining and
supports module stop reentry. In the stop-reentry case the first DATA is
DISASSOCIATED, the remaining DATA and native disassociation confirmation are
STOPPED. Commands remain excluded from PURGE. These results do not establish
orphan recovery, PAN conflict or receiver-enable behavior.

## Orphan recovery and realignment

The orphan debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-orphan-build.log`). The focused
`Ieee802154Orphan\.test$` protocol command passed **32 typed/raw runs**, zero FAIL,
exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-orphan-protocol.log`, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-orphan-evidence/`).
The [companion](../../../../../tests/protocol/ieee802154/orphan.md) defines the
configured last-known association, higher-layer member decisions, two-channel
search and actual recovered DATA/ACK. Cases include first/all realignment ACK
loss, outside-scan REALIGNMENT notification, cancel/reset, malformed/foreign or
protected unsupported input, and an advertised channel distinct from the receive
channel. Zero/max ScanDuration are ignored; response-wait determines listening.
The subsequent actual-join/orphan chain extension passed together with the
expanded association and orphan scope: **100 runs** (68 association + 32 orphan),
exit 0 (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-orphan-chain-protocol.log`). It resets, scans, joins,
observes a coordinator move to another PAN/channel, recovers and sends DATA.

## PAN conflict and repeated association

PAN-conflict production-path coverage passed **28 typed/raw runs**, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-pan-conflict-protocol.log`). It checks foreign coordinator
beacons, member notification commands, device notification only after ACK,
queued notification behind accepted DATA, unknown members, protected unsupported
input and reset reentry. DATA completed during CCA follows the selected discard
policy expressly permitted by 6.3.2.1; this is not a missing immediate-ACK feature.

Repeated association passed **8 typed/raw runs**, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-reassociation-protocol.log`). The
[companion](../../../../../tests/protocol/ieee802154/reassociation.md) describes
the leave/cancel/reset episodes, address replacement and late same/foreign-PAN
response inputs. Raw outputs from both suites are retained in
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-management-continuation-evidence/`.

## Scheduled receiver control

The debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-receiver-final-build.log`). The focused
receiver-enable protocol scope passed **76 typed/raw runs**, zero FAIL, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-receiver-enable-policy-protocol.log`,
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-receiver-enable-evidence/`). The
[companion](../../../../../tests/protocol/ieee802154/receiver-enable.md) defines
38 scenarios covering scheduling, counter boundaries, AutoOff, validation,
replacement, primary DATA/ACK/POLL/scan duties, idle policy and lifecycle reentry.
The initial 58-run scope passed before expansion; it is a subset, not another
58 current scenarios. The CCA-discard probe initially expected delivery for a
no-AR frame; source inspection against 6.3.2.1 corrected that expectation. It now
verifies permitted discard and its actual sender/local-access consequences.

## Scan boundaries and broadcast release

The scan boundary debug build passed. `Ieee802154PassiveScan_1.test` passed
**42 runs**, including exact dwell-end/2-ps-late beacon completion, zero-duration
ED without invented energy, and unfinished ED channel reporting
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-scan-boundary-module.log`). `Ieee802154Discovery_1.test` passed
**16 runs**, including access failure on all requested active-scan channels and
their complete UnscannedChannels list (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-active-unscanned-module.log`).

The broadcast debug build passed (`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-broadcast-build.log`).
`Ieee802154Broadcast.test` passed **22 typed/raw runs**, exit 0
(`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-broadcast-protocol.log`, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-broadcast-evidence/`).
The [companion](../../../../../tests/protocol/ieee802154/broadcast.md) defines
actual beacon-advertised release, FIFO, extended broadcast preservation,
expiry/PURGE, failed beacon/release and pending/CCA interruption coverage.
