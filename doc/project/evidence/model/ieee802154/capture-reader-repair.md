# IEEE 802.15.4 — captured-byte boundary repair proposal

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [frame-codec.md](frame-codec.md), [results.md](results.md)

The user approved this exact sealed file and bounded repair in this session.
The implemented repair passes the production capture unit, including both FCS
link types, truncated records, radiotap stripping and invalid offsets. The final
evidence and parent/current generic serializer comparison are in [results.md](results.md).

`tests/unit/Ieee802154Capture_1.test` reproduces a shared PCAP reader defect using production
`PcapWriter` and `PcapReader`. An ACK has five bytes; writing it with snaplen 3 records
`incl_len=3`, `orig_len=5`. Reading returns `{02,00,6a,00,00}` instead of the three supplied
bytes `{02,00,6a}`. Both complete-record FCS modes (195/230) passed before this assertion.
The focused debug run exited 1; output says `Truncated PCAP: captured=3, returned=5`.
Artifacts: `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-unit.log` and
`tests/unit/work/Ieee802154Capture_1/test.{out,err}`.

Cause: `src/inet/common/packet/recorder/PcapReader.cc`, `readPacket()`, resizes its output
vector to `packetHeader.orig_len - offset` and copies from a zero-initialized capture buffer.
Marking the resulting chunk incomplete does not make the extra zeros captured evidence.

Proposed bounded repair: in that file only, validate that the decoded link-header offset fits
the captured length, then size the returned byte vector from `packetHeader.incl_len - offset`.
Keep the existing incomplete marker when `orig_len > incl_len`. Preserve bytes and timestamp
behavior for complete captures. This changes no protocol wire format or baseline. The file is
inside the sealed `common/packet/` folder; this proposal does not authorize its edit.

After explicit authorization, apply that repair and rerun `Ieee802154Capture_1.test` plus the
existing directly related `PcapReader*.test` cases. The required result is exactly three
returned bytes, marked incomplete, with no fabricated FCS. Additional generic PCAP parsing
issues are outside this bounded proposal and need their own investigation.

The user explicitly approved this file and bounded repair on 2026-11-15. The
implementation now checks the link-header offset against `incl_len` before sizing
the returned vector from that captured length. The focused fixture additionally
checks complete and truncated radiotap stripping and rejects a declared link-header
length exceeding the captured record. The radiotap stripping fixture uses an opaque
synthetic payload; it makes no claim that those payload bytes are an 802.11 frame.

Validation after a fresh debug build passed on 2026-11-15:

- `opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154Capture_1\.test$'`:
  one PASS, exit 0, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-capture-repair-unit.log`.
- Existing real radiotap/plain and receiver-copy recorder modules passed in the
  nine-case module batch, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-current-module.log`.
- The admitted `serializer_ieee802154.test` passed two captures/four frames with
  typed serializer coverage and no raw fallback, `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-admitted-serializer.log`.

The generic serializer corpus still reports unchanged Wi-Fi byte mismatches; the
[current results](results.md#approved-captured-length-repair-and-serializer-admission)
record the complete outcomes and input-length attribution. No expectations changed.
