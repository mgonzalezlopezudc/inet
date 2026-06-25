# 802.11ax DL MU OFDMA Implementation Walkthrough

This note describes how the current INET codebase implements the parts of
802.11ax that matter for downlink multi-user OFDMA, with emphasis on the
main design decisions visible in the implementation. It also lists scoped
quality-improvement ideas that stay within DL MU OFDMA.

## Implementation Walkthrough

802.11ax support is modeled as a narrow HE/DL MU OFDMA layer over existing
INET 802.11 machinery.

At the PHY mode level, HE adds timing, subcarrier counts, MCS/NSS tables, and
bitrate/duration computation while reusing the existing OFDM/VHT code
abstractions. See `src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.cc`.
The important design choice here is reuse: HE is not a separate PHY stack, it
extends the established mode/preamble/data-mode pattern.

RU modeling is deliberately simple. `Ieee80211HeRu` is just `{ index,
centerFrequency, bandwidth }`, and `calculateHeRus()` splits the channel into
equal-width contiguous slices in
`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h`. The
real scheduler, `HeDlSchedulerEqualSizedRUs`, preserves candidate order, caps
by `maxMuStations`, and assigns one equal-width RU per selected STA in
`src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.cc`.
That is explicitly a model simplification, and the tests guard it as such.

At the MAC coordination layer, `HeHcf` is the HE-specific entry point. It
replaces the frame sequence handler with `HeFrameSequenceHandler`, scans the
winning EDCAF queue, skips multicast/broadcast, requires QoS data, and requires
an active originator Block Ack agreement before a station becomes MU-eligible.
If HE mode is not active, the head eligible packet is not MU-eligible, or fewer
than two candidates exist, it falls back to normal `Hcf`. This is one of the
strongest design choices: DL MU OFDMA is an opportunistic specialization, not a
fork of the whole HCF path.

`HeDlMuTxOpFs` builds the MU TXOP. It asks the scheduler for RU allocations,
validates queued packets again, computes the sequential Block Ack protection
duration, assigns sequence numbers, forces `BLOCK_ACK`, moves selected packets
into in-progress state, and stores duplicates in an `Ieee80211HeMuTag` attached
to a container packet. The TXOP then sends one HE-MU container followed by
sequential Block Ack handling and BAR/Block Ack exchanges for later STAs. That
is a pragmatic simulation design for DL MU OFDMA acknowledgment without
implementing a broader trigger-based UL workflow.

The PHY header has receiver-visible MU metadata: each sub-transmission gets its
own `ruIndex`, plus the full allocation map of `ruIndex -> staAddress` in
`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg`.
The runtime container uses `Ieee80211HeMuTag` to carry actual per-RU packet
copies through the medium.

The medium does the OFDMA fan-out. `Ieee80211RadioMedium::addTransmission()`
detects the MU tag, recalculates equal RUs, creates one sub-packet per
allocation, assigns narrower analog models, scales transmit power by RU
bandwidth, inserts an `Ieee80211HeMuPhyHeader`, and registers sub-transmissions
with the normal radio medium. It also keeps the main transmission for logical
tracking, suppresses it as a physical receiver/interference candidate, and
suppresses self-interference among sibling RU sub-transmissions.

Finally, the receiver filters HE MU packets by RU assignment. If a packet is
`ieee80211HePhy`, it peeks the HE MU PHY header, compares the receiver NIC MAC
address with the allocation table, and only accepts the sub-transmission whose
`ruIndex` matches that STA.

## Important Design Decisions

- HE functionality reuses the existing 802.11 mode, radio, HCF, and frame
  sequence infrastructure instead of creating a parallel stack.
- DL MU OFDMA is gated by QoS data and active Block Ack agreements, keeping the
  implementation aligned with existing Block Ack bookkeeping.
- The scheduler abstraction is intentionally small: candidates in, channel
  parameters in, per-STA RU allocations out.
- The current RU model is equal-bandwidth frequency slicing, not the full
  802.11ax RU tone taxonomy.
- A single MAC-level MU container represents the PPDU, while the radio medium
  expands it into per-RU physical sub-transmissions.
- MU metadata exists in two forms: a runtime `Ieee80211HeMuTag` that owns
  per-RU packet copies, and an `Ieee80211HeMuPhyHeader` that exposes RU
  assignment information to receivers.
- The medium models sibling RU transmissions as non-interfering and scales
  power by each RU's bandwidth share.
- Receiver-side RU filtering happens before normal reception feasibility and
  SNIR/result handling.

## Quality Improvement Ideas

1. Make RU geometry single-source. The scheduler returns full `Ieee80211HeRu`,
   but the medium recalculates RUs from allocation count and ignores
   `alloc.ru.centerFrequency` and `alloc.ru.bandwidth`. For the current
   equal-width scheduler this works, but it weakens the scheduler contract.
   Within DL MU OFDMA scope, carry RU geometry into the MU tag or derive
   sub-transmissions directly from scheduler allocations.

2. Remove hardcoded channel defaults in `HeDlMuTxOpFs`. It currently falls back
   to `20 MHz` and `5.18 GHz`, and maps HE band mode to fixed `2.412/5.18 GHz`.
   Prefer passing the actual channel context into the frame sequence so
   scheduling and medium fan-out agree.

3. Make MU container assembly transactional. Most validation happens before
   queue mutation, which is good, but late failures after some removals or
   in-progress updates can still be awkward. Build a validated allocation plan
   first, then mutate queue/in-progress/tag in one final pass with guaranteed
   cleanup on error.

4. Tighten ownership around `Ieee80211HeMuTag`. It owns raw `Packet *` values
   and manually deletes them. The `setAllocations()` shallow-copy shape is
   risky. A clearer transfer API, deleted copy-like setters, or explicit
   duplication inside the tag would reduce double-delete/leak hazards.

5. Validate serialized HE MU header bounds. The serializer writes `ruIndex` and
   allocation count as bytes, but the model fields are `int` and array size.
   Add explicit range checks before serialization and reject invalid
   deserialized allocation maps early.

6. Simplify candidate lookup. `collectCandidateStations()` and
   `buildMuContainerPacket()` scan vectors and queues repeatedly. Keeping the
   first eligible packet per destination in ordered form would make behavior
   easier to reason about and reduce accidental mismatch between "candidate
   chosen" and "packet later selected".

7. Encapsulate sequential Block Ack step logic. The `step / 2` and even/odd
   handling works, but it is brittle. A small explicit state object like
   `{sta, needBar, waitForBa}` would make timeout and retry behavior less
   error-prone.

8. Add focused tests for cleanup paths: scheduler returns invalid or duplicate
   RU indices, late validation failure after candidate collection, timeout for
   one STA while others continue, and ownership/duplication of MU tag packets.

## Verification

The focused HE/OFDMA unit-test filter passed:

```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0aipre2/setenv -f
source setenv -q
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```

Result: 11 tests passed.
