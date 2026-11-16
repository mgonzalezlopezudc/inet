# IEEE 802.15.4 GTS inactivity expiration

Plan: [B3-05](../../../plan/pending/802154-superframe-gts-plan.md#b3-05--ieee802154-add-gts-activity-expiration). Logical test: `Ieee802154GtsExpiry`.

The fixture uses a PAN coordinator and an associated device on a one-hop
scalar medium. Both native typed frames and raw frames are exercised. For
the exact reception-boundary cases, an additional scripted PHY transmits a
valid frame with the associated device's short address; its propagated
waveform end is observed independently at the coordinator.

```mermaid
graph LR
  C[PAN coordinator] --- D[Associated device]
  S[Scripted PHY: boundary cases] -. owner DATA .-> C
```

The inactive limit is measured in complete superframe opportunities, not
elapsed wall-clock seconds: 512 at BO 0, and two at BO 8 or BO 9. First
actual RF publication arms the grant. The copied physical grant end is the
settlement boundary; a matching received DATA or ACK at that boundary counts.
Automatic removal produces one coordinator indication and a zero-start
descriptor retaining the old direction and length. The later descriptor's
four-superframe advertisement lifetime is separate from grant inactivity.

From the checkout root, build and run the owning checks:

```sh
make MODE=debug -j4
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154GtsState_1[.]test$'
opp_run_module_tests --load . -p inet -m debug --no-build -f 'Ieee802154SuperframeController_1[.]test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154(GtsExpiry|GtsData|GtsDeallocation)[.]test$'
```

| Scenario | Setup, stimulus and observation |
| --- | --- |
| 0–2 | Silent device-transmit grants at BO 0, 8 and 9 expire after exactly 512, 2 and 2 complete opportunities, respectively. The callback time is measured from the first actual positive beacon RF start; the device receives the later removal. |
| 3 | A BO 8/SO 8 grant changes to BO 9 at the old CFP end. The old copied opportunity cannot consume the newly rebased count. |
| 4–5 | In a real CFP, device DATA is received but its ACK is lost, or coordinator DATA is sent but its ACK is lost. The former renews the transmit grant; the latter does not renew the receive grant. |
| 6 | Actual unacknowledged downlink suspends monitoring beyond the normal bound. A later acknowledged transmission re-arms monitoring, and its lost ACK leaves two silent opportunities before removal. |
| 7–8 | A coordinator PHY outage spans more than 64 scheduled BO 3 superframes, or prevents the first prepared positive beacon from reaching RF. Catch-up exceeds the descriptor-aging cap; a never-published grant starts its full lifetime only on its first actual publication. |
| 9–10 | Removing an earlier grant relocates a survivor without renewing it; separately, RESET from the automatic-removal indication completes after that indication and produces no duplicate. |
| 11 | After unacknowledged downlink has suspended monitoring, an acknowledged DATA request is cancelled before RF. The grant remains suspended and no second DATA is emitted. |
| 12–13 | A same-BO immediate START changes phase before any old opportunity completes, or after one completes. The replacement preserves completed inactivity without counting an abandoned opportunity. |
| 14–16 | Valid owner DATA ends one simulation tick before, exactly at, or one tick after the last grant edge. The first two renew; the late waveform cannot renew. The observed physical waveform end, owner address and native indication are checked. |
| 17 | An immediate same-BO START begins RF exactly at the old grant end. The complete old opportunity settles before the new phase's first completed opportunity expires the grant. |
| 18 | A matching ACK to actual coordinator DATA in the last allowed receive opportunity renews that grant; two subsequent silent opportunities expire it. |

Unacknowledged coordinator DATA supplies no missing-ACK evidence. This model
suspends receive-grant monitoring on actual AR0 RF start and re-arms a fresh
interval on a later actual AR1 start; only a matching received ACK renews it.
Refused or cancelled pre-RF work does not change monitoring. Changed BO
publication rebaselines armed survivors while preserving suspension; relocation
alone does not renew them. These bookkeeping choices are the selected model
policy described in the [beacon profile](../../../doc/project/evidence/model/ieee802154/beacon-profile.md#gts-allocation-timing-and-capacity).

The fresh debug acceptance passed three unit fixtures, three module fixtures
and all 236 protocol cases: these 38 typed/raw runs and 198 existing controls.
There were no errors, skips or ProtocolTest failures. Mermaid rendering is
**NOT_RUN**. PHY waveform and MAC callback observations are distinct; the
scripted boundary cases do not claim an application-generated last-tick frame.
