802.11ax TWT support

**Implemented**:
- New timer-driven Ieee80211TwtManager module, agreement types, and MAC interface.
- Disabled-by-default twt NIC submodule with real radio sleep/wake control.
- TWT Setup, Teardown, and Information action-frame types plus S1G-action serialization/deserialization.
- Runtime TWT setup, teardown, information, and broadcast-schedule primitive types.
- STA/AP primitive handling to install/update/remove manager agreements.
- TWT action routing into management instead of unconditional MAC-layer discard.
- HCF STA sleep gating and HE DL MU candidate filtering for sleeping peers.
- HE TWT capability bits
- Broadcast-TWT Beacon schedules/persistence
- Setup-response actions
- PS-Poll framing, UL gating, agent helpers
- Energy-residence scalars
- Regression configs and focused serializer tests.

**Verified**:
- Release build succeeds.
- Ieee80211TwtFrames_1.test and Ieee80211HeMgmtElements_1.test pass.
- NED validation succeeds.

**Key additions** are under [`twt`](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/twt), with integration in [`Ieee80211Mac.cc`](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211Mac.cc) and [`Ieee80211Frame.msg`](/home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg), [management state machines](src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtSta.cc), [Beacon serialization](src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.cc), and [regression configs](examples/ieee80211/twt/omnetpp.ini).

_One caveat_: running the new full-management regression scenario currently fails during the pre-TWT association exchange due to an existing full-management packet-body handling issue. The code, unit tests, build, and NED validation are sound, but that end-to-end scenario needs one more integration debugging pass.