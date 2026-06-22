# Implementation Plan - IEEE 802.11ax BSS Coloring Example

This plan describes how to expose, configure, and serialize the HE BSS Color capability in the INET framework, and how to create a demonstration simulation showcasing spatial reuse (OBSS/PD) between two overlapping BSSs (OBSS) on the same channel.

## User Review Required

> [!NOTE]
> We will add the `heBssColor` parameter to `Ieee80211Mib` so users can explicitly set the BSS Color for APs and STAs.
> When a STA associates with an AP, its local BSS Color will be dynamically updated to match the AP's BSS Color to model the standard behavior.

## Proposed Changes

### Core 802.11 Framework

---

#### [MODIFY] [Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned)
- Add `int heBssColor = default(0);` parameter under the HE parameters section. BSS Color value ranges from 1 to 63, with 0 meaning disabled or unspecified.

#### [MODIFY] [Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc)
- Parse the `heBssColor` parameter in `initialize(int stage)` under stage `INITSTAGE_LOCAL`.
- Set `heOperation.bssColor` to this parsed value after validating that it is between 0 and 63.

#### [MODIFY] [Ieee80211Radio.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc)
- In `encapsulate(Packet *packet) const`, if the constructed header is an `Ieee80211HeMuPhyHeader`, fetch the containing NIC's MIB and set the header's BSS Color to the MIB's `heOperation.bssColor`.

#### [MODIFY] [Ieee80211MgmtSta.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtSta.cc)
- In `handleAssociationResponseFrame` upon successful association, copy the associated AP's BSS color into the station's local `mib->heOperation.bssColor`.
- In `disassociate` and `handleDisassociationFrame`, reset `mib->heOperation.bssColor` back to `0`.

#### [MODIFY] [Ieee80211MgmtStaSimplified.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtStaSimplified.cc)
- In `initialize` stage `INITSTAGE_LINK_LAYER`, during direct association setup, copy `apMib->heOperation.bssColor` into `mib->heOperation.bssColor`.

---

### New BSS Coloring Example Simulation

Create a new simulation folder under `examples/ieee80211/bss_coloring/`.

---

#### [NEW] [BssColoringNetwork.ned](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/BssColoringNetwork.ned)
Define a new NED network `BssColoringNetwork` consisting of:
- `radioMedium`: `Ieee80211ScalarRadioMedium`
- `configurator`: `Ipv4NetworkConfigurator`
- `ap1`: `AccessPoint` at coordinates `(150, 250)`
- `sta1`: `WirelessHost` at coordinates `(100, 250)`, associated with `ap1`
- `ap2`: `AccessPoint` at coordinates `(350, 250)`
- `sta2`: `WirelessHost` at coordinates `(400, 250)`, associated with `ap2`
- `server1` and `server2`: `StandardHost` connected to `ap1` and `ap2` respectively to generate traffic.

#### [NEW] [omnetpp.ini](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/omnetpp.ini)
Define configurations:
- `[General]`: Base 802.11ax settings on channel 36 (5.18 GHz). Traffic generation from `server1` -> `sta1` and `server2` -> `sta2`.
- `[Config BssColoringDisabled]`: Set `ap1.wlan[*].mib.heBssColor = 1`, `ap2.wlan[*].mib.heBssColor = 2`, but disable spatial reuse `**.receiver.enableSpatialReuse = false`. This baseline config should show that since both BSSs overlap, they defer to each other (CSMA/CA CCA-CS/ED deferred transmission) and share the channel capacity.
- `[Config BssColoringEnabled]`: Enable spatial reuse `**.receiver.enableSpatialReuse = true` and `**.receiver.obssPdThreshold = -62dBm`. AP1 and AP2 use different BSS Colors (1 and 2). This config should show concurrent transmissions and higher aggregated throughput since they ignore each other's transmissions if the received power is below the OBSS/PD threshold.

#### [NEW] [walkthrough.md](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/walkthrough.md)
- Provide a detailed explanation of the BSS coloring mechanism, configuration parameters, and how to observe spatial reuse in action.

---

## Verification Plan

### Automated Tests
- Run existing HE/OFDMA unit tests to ensure no regressions:
  ```sh
  export CCACHE_DISABLE=1
  source /home/user/omnetpp-6.4.0aipre2/setenv -f
  source setenv -q
  bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
  ```

### Manual Verification
- Compile the changes in release mode:
  ```sh
  make -j$(nproc) MODE=release
  ```
- Run the simulation configs using Cmdenv to check for errors and output throughput:
  ```sh
  opp_run -u Cmdenv -l src/libINET.so -c BssColoringDisabled examples/ieee80211/bss_coloring/omnetpp.ini
  opp_run -u Cmdenv -l src/libINET.so -c BssColoringEnabled examples/ieee80211/bss_coloring/omnetpp.ini
  ```
