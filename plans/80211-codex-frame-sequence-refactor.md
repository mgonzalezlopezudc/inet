**Short Version**

Yes: the framework is ripe for a rewrite, but I would not rewrite it as “more subclasses of `IFrameSequence`.” The better direction is a typed, executable frame-exchange DSL: standard grammar rules describe legal exchanges, and a generic runtime executes typed transmit/receive steps with explicit validation, timeout policy, ownership, and side effects.

The current design has the right seed: Annex G grammar represented with `SequentialFs`, `AlternativesFs`, `OptionalFs`, `RepeatingFs`. But HE has outgrown that model and now needs custom controller classes and even a custom handler. That is the signal.

**Current Shape**

The core contract is minimal: `IFrameSequence` emits one `IFrameSequenceStep` at a time, either transmit or receive, then `FrameSequenceHandler` drives it. See [IFrameSequence.h](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/contract/IFrameSequence.h:18) and [FrameSequenceHandler.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.cc:17).

The legacy grammar is encoded directly:

- DCF mirrors Annex G.2 with alternatives, optional CTS/RTS-CTS, fragment repeat, last-frame ACK: [DcfFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/DcfFs.cc:15)
- HCF mirrors part of G.3: multicast repetition or TXOP repetition: [HcfFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HcfFs.cc:16)
- TXOP has several G.3 branches, but with explicit TODO gaps: [TxOpFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/TxOpFs.cc:15)
- HE DL/UL use richer stateful controllers that contain a `SequentialFs`: [HeDlMuTxOpFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc:504), [HeUlMuTxOpFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc:142)

**Main Problems**

1. Step semantics are too weak. A receive step only says “receive one” or “collect many” via `completesOnReception()`. It does not encode expected subtype, transmitter, receiver, TID, BlockAck context, trigger ID, RU, PPDU format, or timeout behavior.

2. Timeout behavior is hard-coded in the handler. HE DL MU needs “BlockAck timeout means mark this STA failed and continue”, so there is a custom `HeFrameSequenceHandler` with `dynamic_cast<HeDlMuTxOpFs *>`: [HeFrameSequenceHandler.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.cc:15). That policy belongs on the receive step or transition.

3. Response validation is duplicated and shallow. Primitive ACK/CTS checks repeat the same pattern and mostly check `isForUs()` plus subtype: [PrimitiveFrameSequences.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/PrimitiveFrameSequences.cc:89). HE adds stricter validators locally. This should be reusable.

4. The grammar containers are fragile. `AlternativesFs` trusts selector indexes. `OptionalFs` has a nullable predicate default but calls it unconditionally. `RepeatingFs` can recurse forever if its child yields no step while the predicate stays true. Raw owning pointers are everywhere: [GenericFrameSequences.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.cc:13).

5. Standard coverage is uneven. `SelfCtsFs` is unimplemented; PCF/MCF are placeholders; TXOP has TODO branches; HT is simplified; VHT/S1G/DMG are not really modeled despite Annex G.4 covering them. `getIfs()` has a PIFS TODO: [FrameSequenceContext.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.cc:26).

6. Sequence-specific side effects leak into coordination functions. HE HCF checks concrete sequence types to process MU container packets and failures: [HeHcfTxRx.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRx.cc:91). That should be event/effect-driven.

**Rewrite Direction**

I would split the framework into four layers.

1. `FrameExchangeRule`

Immutable grammar nodes:

```cpp
seq(...)
alt(...)
opt(predicate, ...)
repeat(predicate, ...)
step("RTS", transmit(...))
step("CTS", receive(...))
```

Each rule should carry metadata: name, standard family, clause/annex reference, required capabilities, and attributes such as `individual`, `group`, `QoS`, `normalAck`, `blockAck`, `muUsersRespond`, `aMpdu`, `pifs`.

2. `FrameExchangeRuntime`

A generic executor that consumes a rule graph and emits `StepSpec`s. `FrameSequenceHandler` should not know HE, HT, RTS, BlockAck, or MU. It should only execute the current step and feed results back.

3. `StepSpec`

Replace weak transmit/receive steps with typed specs:

```cpp
enum class StepKind { Transmit, ReceiveOne, ReceiveMany };
enum class TimeoutPolicy { AbortSequence, CompleteRejected, CompleteExpired, Continue };
enum class FailureScope { WholeSequence, LastTx, PerUser, None };
```

A receive step should include a validator:

```cpp
ExpectedResponse {
    frameType;
    transmitter;
    receiver;
    tid;
    triggerId;
    ruIndex;
    ppduFormat;
    blockAckVariant;
}
```

This removes the need for `HeFrameSequenceHandler`.

4. `FrameExchangeEffects`

Sequence completion should emit typed effects:

- `FrameTransmitted(packet)`
- `FrameAcked(tx, ack)`
- `FrameFailed(packet, reason)`
- `MuContainerTransmitted(allocations)`
- `BlockAckReceived(sta, tid, bitmap)`
- `CollectionWindowExpired(results)`

Then HCF/DCF/HE HCF consume effects instead of doing `dynamic_cast` on the running sequence.

**Reusable Building Blocks**

A rewrite should factor these once:

- `RtsCtsExchange`
- `TransmitData`
- `TransmitMgmt`
- `TransmitAndAck`
- `BlockAckReqBlockAck`
- `TriggerResponseCollection`
- `MuBarBlockAckCollection`
- `FragmentBurst`
- `TxopRepeat`
- `ProtectionPrefix`
- `NavSet/NavReset`
- `SoundingExchange`

Then DCF/HCF/HT/HE are mostly rule composition, not hand-written `switch (step)` classes.

**Standard-Compliance Model**

Make the standard grammar explicit and testable.

- Basic G.2: DCF, PS-Poll, broadcast/group, individual fragmented/last exchanges.
- G.3: EDCA/HCCA TXOP, PIFS-marked multicast, CF-Poll/CF-Ack/piggyback branches.
- G.4: HT/VHT/S1G/DMG rule families, with capability-gated branches.
- G.5: HE NAV protection, MU-RTS, DL MU, UL trigger, cascading, sounding.

Unsupported branches should be represented as `UnsupportedRule("PCF beacon sequence", reason)` rather than empty sequences like [PcfFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/PcfFs.cc:15). That makes gaps visible and prevents silent “successful” no-op compliance.

**Migration Plan**

1. Add characterization tests around current DCF/HCF/TXOP/HE flows.
2. Add `TimeoutPolicy` and response validators to receive steps. Remove `HeFrameSequenceHandler`.
3. Replace primitive `switch(step)` classes with reusable `StepFs`-style factories.
4. Convert raw owning pointers in generic sequences to `std::unique_ptr`.
5. Introduce bounded, checked selectors and repeat guards.
6. Add typed IFS handling: SIFS/PIFS/RIFS/zero, not just `getNumSteps() == 0 ? 0 : SIFS`.
7. Rebuild DCF/HCF/TXOP using the rule DSL while keeping public class names.
8. Move HE DL/UL planning state into plan/controller objects, but execute them through the same generic runtime.
9. Replace concrete-sequence checks in HCF/HE HCF with emitted effects.
10. Add rule coverage tests comparing implemented branches to Annex G rule inventory.

The immediate high-value first step is small: make receive timeout behavior and response validation properties of `IReceiveStep`. That alone removes the HE handler fork, clarifies failure semantics, and creates the foundation for the larger rewrite.