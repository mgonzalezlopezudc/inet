**Verdict**

Current compliance is materially better than the June 24 report says. FEC restrictions, BCC timing, puncturing validation, UL scheduler regression, validation script staleness, and the TWT lifecycle warning are now largely fixed. I would now describe the model as a **standards-aware packet-level 802.11ax/HE simulator**, not a full wire/interoperability compliant implementation.

**Findings**

1. The main report is now stale and should not be treated as current.
[80211ax-codex-standard-compliance-codequality-20260624.md](/home/user/omnetpp_ws/inet/reports/80211ax-codex-standard-compliance-codequality-20260624.md:25) still lists FEC and BCC defects that are fixed in code, and still calls custom wire formats and Multi-TID BA high-priority defects without reflecting the new partial implementations.

2. Wire-format compliance is still only partial.
The HE PHY header explicitly says it is “not a bit-exact HE-SIG encoding” in [Ieee80211PhyHeader.msg](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg:185). Trigger serialization is much more standard-shaped now, but it still packs `triggerId` through AP Tx Power, UL Spatial Reuse, and reserved fields in [Ieee80211MacHeaderSerializer.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc:382). Good simulator metadata, not bit-level conformance yet.

3. Multi-TID Block Ack is improved but not fully demonstrated as true multi-TID operation.
There is now DL BAR generation when negotiated in [HeDlMuTxOpFs.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc:159), response construction in [RecipientBlockAckProcedure.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckProcedure.cc:34), and bitmap processing in [QosAckHandler.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/originator/QosAckHandler.cc:139). But the DL path currently creates one record per request, so the examples advertise the capability more than they prove multi-TID aggregation behavior.

4. FEC validation is now strict, but scheduler adaptation is still coarse.
The calculator rejects BCC for MCS 10/11, 484+ tone RUs, and NSS > 4 in [Ieee80211HePhyCalculator.h](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h:396). BCC timing now forces one encoder in [Ieee80211HePhyCalculator.h](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h:431). One remaining edge: DL/UL choose one coding mode for the whole schedule based on all peers in [HeHcf.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:514), so mixed LDPC/non-LDPC capability cases may fail planning instead of proactively choosing only BCC-legal RU/MCS combinations.

5. Preamble puncturing is now much better.
The parser now validates against constrained 80/160 MHz patterns in [HeHcf.cc](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:87). The HE features docs also correctly state that `"0100"` maps to `0x2`.

6. Documentation and result claims have now been narrowed.
[dl_ofdma/compliance-report.md](/home/user/omnetpp_ws/inet/examples/ieee80211ax/dl_ofdma/compliance-report.md:3) now scopes the claim to MAC response-timeout recovery instead of full wire-format conformance, and links to the local `compliance-sim.log`. [dl_ofdma/walkthrough.md](/home/user/omnetpp_ws/inet/examples/ieee80211ax/dl_ofdma/walkthrough.md:11) now points at the `dl_ofdma` folder, uses the current 0.6 s run limit, records current packet counts, and calls the 80 MHz case a smoke test rather than a capacity benchmark. [bss_coloring/walkthrough.md](/home/user/omnetpp_ws/inet/examples/ieee80211ax/bss_coloring/walkthrough.md:97) now reports 351 disabled and 476 enabled packets.

**Fixed Items Confirmed**

- Validation script now targets `examples/ieee80211ax/dl_ofdma` and `6.4.0aipre2`: [ofdma_example_validation.sh](/home/user/omnetpp_ws/inet/tests/validation/ieee80211/ofdma_example_validation.sh:7).
- `HeUlScheduler_1.test` now passes.
- TWT broadcast scenario completes without the old undisposed-packet warning.
- Spatial reuse exists, but remains a threshold-based OBSS/PD simplification in [Ieee80211Receiver.cc](/home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc:307), not the full SRG/non-SRG, dual-NAV, power-coupled spatial reuse model.

**Verification Run**

Passed:
- 24 HE unit tests: `(Ieee80211He|HeDlScheduler|HeUlScheduler).*`
- `Ieee80211MultiTidBlockAck_1.test`
- `Ieee80211TwtFrames_1.test`
- `tests/validation/ieee80211/ofdma_example_validation.sh`
- UL `MixedUora` example
- TWT `Broadcast` example
- BSS coloring disabled/enabled examples

Only pre-existing uncommitted source edits remain in the working tree; my validation runs did not add tracked changes.
