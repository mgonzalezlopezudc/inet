# IEEE 802.15.4 indirect transaction ownership

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [applicability.md](applicability.md)

## Broadcast release contract

The next increment implements 10.22.2 and 7.3.1.2 for selected non-beacon
operation: queued broadcast DATA sets Frame Pending in an explicitly requested
or discovery-response beacon; successful beacon transmission releases queued
broadcasts through unslotted access. No periodic beacon schedule is introduced.
The existing store owns the immutable frames and packets. Both broadcast address
forms share the local PAN's short-broadcast queue key; the original wire address
and destination PAN remain unchanged. Broadcasts do not appear in the pending
address list, never request ACK, and use ordinary persistence/overflow/PURGE.
Only a successful beacon releases them. Non-beacon operation drains further
broadcasts after each success; a failed access waits for another beacon. Per-device
poll responses retain their existing FIFO and stable-DSN rules.

A successfully decoded beacon from an associated device's known coordinator
can establish receive demand for the advertised broadcast. A validated broadcast
DATA consumes that demand, retaining it if Frame Pending announces another.
Ordinary unrelated or invalid frames do not consume it. The MAC's existing idle
restoration combines this demand with poll, scheduled receive and idle policy.
Scan-only beacon observations do not create it. Reset/stop/disassociation clear
it; no speculative completion event or second radio owner is added.

The mapped production checks cover one/multiple queued broadcasts, FCF/address
list bits, actual broadcast delivery without ACK, empty/failed beacons, failed
release followed by a later beacon, expiry/PURGE/reset/stop during release and
full extended-broadcast wire preservation. Contract ownership, callbacks and
source protection were checked against the existing store/MAC before edits.

The M2 coordinator uses one pending store for DATA and Association Response frames.
IEEE Std 802.15.4-2024, 10.22.2 requires bounded capacity, FIFO order per device,
Frame Pending when another transaction exists, expiration, and success only on
extraction (including the requested ACK). Clause 6.6.3.4 requires a failed indirect
attempt to remain pending with the same DSN until another Data Request arrives.
Clause 10.21.2 requires Association Response to use this indirect procedure.

The source is `802154-2024.pdf`, SHA-256
`851d81127297f898c956fdd1ce92929324d68cd696fd338d1e876f6a372f22d3`.
The exact corpus nodes are `clause:10.22.2`, `clause:10.22.3`,
`clause:6.6.3.4` and `clause:10.21.2`, document `ieee802154-2024`.
The first three were retrieved again on 2026-11-15. See the applicability audit
for the M1 selection; this document does not close the expanded M2 audit.

## Implementation contract

`Ieee802154PendingTransactions` owns immutable complete frame references and local
transaction tokens. The MAC remains responsible for accepted service packets,
timers and terminal callbacks. The store has no module callbacks, random choices
or timers. An entry is keyed by PAN ID and the complete native destination address;
different PANs, address modes and high EUI-64 bits cannot alias.

An insertion refuses a full store without consuming a token or changing its frame.
Capacity is at least one. Tokens are unique among pending entries. Selection returns
the oldest unexpired entry for one device, without removing or modifying it. A
failed attempt needs no mutation; the stored DSN and frame remain stable. Successful
extraction or purge removes exactly the named token. Expiration returns removed
entries to the owner for its terminal notifications. The selected model boundary
is half-open: an entry with deadline equal to the current time is expired, even if
the poll event precedes its expiry timer in scheduler order.

The production caller must process expiration before reporting Frame Pending and
must keep an active exchange's token until ACK success or expiry. ACK/timeout races,
reset, stop and purge notifications belong to that caller, not this value store.

## Verification

`Ieee802154PendingTransactions_1.test` exercises full native keys,
per-device FIFO, shared command/data capacity, unchanged retry bytes, exact-boundary
expiration and removal. This is helper coverage. Operational poll exchanges,
packet cleanup and management results still require production protocol tests.

## Command codec contract

The command-content codec interprets IDs 1–8 from Table 7-11. Content fields follow
10.21.5.1–3 (association and disassociation), 10.22.4.1 (poll), 10.17.4.1
(PAN conflict), 10.20.3.1 (orphan), 7.5.2 (beacon request) and 7.5.4/Figure 7-30
(realignment). These nodes and Figures 10-156/158/159 were retrieved on 2026-11-15.
The selected realignment form has the seven-octet content and an eight-bit channel;
an extended-channel form is recognized as unsupported, not interpreted as page 0.
Reserved bits in an optional unextended channel octet are ignored on receive.

The complete-frame codec carries Command ID plus Content as its payload and gains
legacy command MHR support. It does not infer operational support from structural
decoding. The M1 MAC explicitly discards command frames after monitor handling;
they cannot become MCPS DATA indications. Security-enabled content remains opaque.
Typed command variants cannot mix fields belonging to different command IDs.
Unknown IDs, truncated known content and extra content are distinct decode results.
Independent hand-derived byte vectors and registered full-frame conversions are
checked in `Ieee802154Commands_1.test`; operation-specific addressing, ACK and
PIB effects remain the management owner's responsibility.

The fresh debug build passed on 2026-11-15, exit 0. The combined unit command
`opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154(Commands|FrameWire|Fcs|PendingTransactions|MacPib|Address)_1\.test$'`
passed all six cases, exit 0. Logs: `/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-command-build.log` and
`/home/miguel/workspace/ai-logs/executions/tmp-preservation-20261115/ieee802154/ieee802154-command-unit.log`. These are working-tree results on baseline
`0336bad8b225397f50938dea19c0f6ef111f8fb7`, not production M2 exchange evidence.
