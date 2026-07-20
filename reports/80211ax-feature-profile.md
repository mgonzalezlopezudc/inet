# Focused 802.11ax project feature profile

## Purpose

This profile reduces local release and debug compilation while working on the
802.11ax modification plan. It distinguishes the feature closure declared by
`.oppfeatures` from the additional features required by the current unit-test
harness and focused simulation configurations.

## Declared 802.11 feature closure

The smallest dependency closure rooted at `Ieee80211` is:

- `Ieee80211`
- `Ieee802`
- `Ieee8022`
- `Queueing`
- `PhysicalLayerWirelessCommon`
- `PhysicalLayerCommon`

There is no separate HE/802.11ax project feature. Consequently, enabling
`Ieee80211` also compiles the other IEEE 802.11 modes and the EHT code located
in the same owned packages.

This six-feature closure builds the INET library after feature generation, but
it is not sufficient for the repository's shared unit-test library or the HE
test networks.

## Validated HE unit-development profile

The smallest profile validated against the focused HE unit-test harness is:

```text
Ethernet
Ieee802
Ieee80211
Ieee8022
Ipv4
Mobility
PhysicalLayerCommon
PhysicalLayerWirelessCommon
ProtocolElement
Queueing
TcpCommon
```

Enable it from a full feature state with:

```sh
opp_featuretool disable -f all
opp_featuretool enable -r Ieee80211
opp_featuretool enable -r Ethernet Ipv4 Mobility TcpCommon
```

`ProtocolElement` is enabled transitively. With the current tree this profile
generates 1,034 library object targets, compared with 801 for the declared
six-feature closure.

The additions are build/test infrastructure requirements rather than HE PHY
requirements:

- `Ethernet`: required by direct IEEE 802.11 FCS linkage and the shared tests.
- `Ipv4`: required by always-built common L3 code and test networks.
- `TcpCommon`: required by the shared unit-test support library.
- `Mobility`: provides `StationaryMobility` used by focused HE test networks.
- `ProtocolElement`: dependency of the selected supporting features.

For focused 802.11ax examples and fingerprint configurations, additionally
enable `Udp` and `Loopback`. Enable `Power` only for TWT/energy configurations
that instantiate power consumers or storage. Canvas/OSG visualization is not
needed for Cmdenv verification.

## Validation evidence

- Clean release and debug libraries compile and link with the 11-feature
  profile.
- Complete focused HE companion slice: 48/48 tests pass in release and 48/48
  pass in debug.
- Release log after independent-review blocker fixes:
  `/tmp/inet-he-review-fixed-release-pass-20260720.log`.
- Debug log after independent-review blocker fixes:
  `/tmp/inet-he-review-fixed-debug-pass-20260720.log`.

The clean debug build also exposed an unrelated feature-boundary defect:
`IPsec.cc` included the generated IPv6 header unconditionally while all IPv6
uses were guarded. Guarding that include with `INET_WITH_IPv6` allows IPv6 to
remain disabled; enabling the IPv6 stack is not necessary for 802.11ax.

## Scope warning

This is a focused development profile, not a claim that every INET example or
test can run. Add features only when a selected configuration instantiates
their NED packages or the relevant build/test harness links their code.
