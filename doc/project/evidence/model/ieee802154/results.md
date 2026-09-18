# IEEE 802.15.4 — applicability-pass evidence

> **Kind:** report · **Status:** snapshot 2026-09-19 · **Seal:** none · **Owns:** — · **Stands on:** [coverage.md](coverage.md), [applicability.md](applicability.md)

Baseline `fd6f800222`, working directory `/home/user/omnetpp_ws/inet-ieee802154-standards`.
Mode: documentation/source/capture inspection; no simulation or library build. The checkout has
no `out/` or `src/libINET*` artifacts. All nine new English checks are `NOT_RUN`; there are no
executable test paths yet. This pass does not establish a Level 2 result or a conformance verdict.

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
are recorded per catalog entry. All 40 source excerpts were compared to their retrieved node text
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
A structural check confirmed 40 unique catalog headings, nine unique check headings, every
catalog ID present in the feature map/checks/ledger, and the 37-selected/3-deferred partition.
Specification documents were checked for model class names and run-verdict leakage.

The broader command `python3 doc/project/enforcement/check_links.py plan/pending` exited 1:
four existing relative links in `pr-1155-resolve-audit-findings.md` do not resolve. None is in the
802.15.4 plan or changed by this pass; that separate plan was left unchanged. Documentation checks
are not simulation evidence, and no serializer, module, protocol or fingerprint result is claimed.
