## Plan: HE DL MU PPDU Window Visibility

Show HE DL MU PPDU details in both places you requested: the packet inspector/info text and the live canvas transmission figure. Implement this in two incremental phases so value appears quickly (inspector first), then visual richness (canvas labels/tooltips), while keeping behavior optional via visualizer parameters.

**Steps**
1. Phase 1: Inspector-level HE MU detail expansion (MVP).
2. In Ieee80211PhyProtocolPrinter::print, keep existing HE MU detection and extend printed content to include per-user allocation summary: RU index, STA ID, MCS, NSS, DCM. This step is independent and can land first.
3. Add formatting guards so long user arrays remain readable (single-line compact format by default; optional truncation marker if user count exceeds a threshold).
4. Validate output by opening packet inspector/info column during HE MU transmissions and checking the new summary string.
5. Phase 2: Canvas overlay and tooltip enrichment.
6. In MediumCanvasVisualizer::createSignalFigure, detect HE MU PHY header from transmission packet and augment packet label text with compact HE MU summary (user count + RU mapping). Depends on Step 2 decisions about formatting consistency.
7. Add enriched tooltip text on the signal figure that includes full per-user rows: RU, STA, MCS, NSS, DCM. Keep label short, tooltip detailed to avoid clutter.
8. Introduce optional visualizer parameters in MediumCanvasVisualizer.ned to control behavior: enable/disable HE MU details, max users shown in label, include/exclude PHY fields. Depends on Step 6.
9. Keep non-HE behavior unchanged and preserve existing default rendering when no HE MU header is present.
10. Phase 3: Validation and regression checks.
11. Run focused HE unit tests and inspect for formatting/build regressions.
12. Manually run an HE MU scenario in Qtenv and verify both surfaces: inspector text and canvas label/tooltip.

**Relevant files**
- /home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyProtocolPrinter.cc — extend Ieee80211PhyProtocolPrinter::print HE MU output.
- /home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg — source of available HE MU user fields (ruIndex, staId, mcs, numberOfSpatialStreams, dcm).
- /home/user/omnetpp_ws/inet/src/inet/visualizer/canvas/physicallayer/MediumCanvasVisualizer.cc — update createSignalFigure for HE MU label + tooltip enrichment.
- /home/user/omnetpp_ws/inet/src/inet/visualizer/canvas/physicallayer/MediumCanvasVisualizer.ned — add toggles/limits for HE MU text visibility.
- /home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeMuPhyHeaderSerializer_1.test — reference for HE MU header population patterns.
- /home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeMuRuAttenuation_1.test — reference for constructing packets that carry HE MU headers.
- /home/user/omnetpp_ws/inet/tests/unit/HeDlScheduler_1.test — scheduler-side RU allocation context for sanity checks.

**Verification**
1. Build affected targets in release mode after changes.
2. Run HE-focused unit tests from repository root:
3. Source environments first: source /home/user/omnetpp-6.4.0/setenv -f and source setenv -q.
4. Execute: bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test".
5. Launch a Qtenv run that produces HE DL MU traffic and verify:
6. Packet inspector/info column shows RU->STA plus MCS/NSS/DCM.
7. Canvas propagating signal label shows compact HE MU summary.
8. Canvas tooltip shows detailed per-user rows.
9. Toggle new NED params to confirm enable/disable and truncation behavior.

**Decisions**
- Included scope: both surfaces (inspector + canvas) and full field coverage (RU mapping and PHY fields), per your answers.
- Excluded scope: new standalone visualizer module. Reuse MediumCanvasVisualizer first to minimize maintenance and integration risk.
- Formatting approach: concise on-label text, detailed tooltip text.
- Frequency details note: ruIndex is in HE MU user info, but per-user center frequency/bandwidth is not directly carried in that struct; if needed later, compute/match from transmission/channel context as a follow-up enhancement.

**Further Considerations**
1. If label clutter appears with many users, prefer showing first N users plus +K more in label while keeping complete tooltip detail.
2. If STA IDs are less readable than MACs in your workflow, add optional address resolution in a later step using MAC-layer context.
3. If you also want timeline-style visibility, a future extension can mirror the same summary into event log text without changing packet semantics.
