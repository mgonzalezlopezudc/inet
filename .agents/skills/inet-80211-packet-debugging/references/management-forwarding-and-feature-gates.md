# Management, forwarding, and feature gates

## Scanning and association

Detailed and simplified INET management modules have different contracts. Confirm the instantiated type before expecting realistic scanning, authentication, association, or roaming frames.

For infrastructure setup, trace state and frames through:

```text
scan → candidate selection → authentication → association → data eligibility
```

For a missing Beacon or Probe exchange, distinguish frame generation, channel/dwell timing, PHY reception, BSSID/SSID filtering, candidate recording, and scan timeout. For association, inspect request capabilities, response status, AP station-table insertion, STA state transition, and any immediate deauthentication/disassociation.

Do not infer WPA/RSN or key exchange from the presence of 802.11 Authentication frames unless the model implements it.

For roaming, measure the last usable old-AP frame, trigger, scan dwell, candidate selection, authentication/reassociation, new AP forwarding update, and first usable data. Separate RF outage, scan latency, management latency, distribution-system convergence, network-layer state, and transport recovery.

## AP forwarding

An infrastructure AP is both an 802.11 peer and a distribution-system forwarding point. Trace wireless and wired observations separately:

```text
uplink:   STA --To DS--> AP wireless → decapsulation/bridge → wired destination
downlink: source → AP bridge → 802.11 construction --From DS--> STA
```

Inspect association state, address transformation, BSSID and DS bits, interface tags, bridge/learning state, output queue, wired capture, VLAN/filtering policy, and ARP/ND or routing when relevant. Wireless-to-wireless traffic through one AP normally produces two over-the-air exchanges rather than a direct STA-to-STA frame.

## Broadcast and multicast

Group-addressed traffic normally has no ordinary ACK or MAC retry and often uses a basic/robust rate. Check AP replication, group addressing, forwarding, power-save buffering, and recipient filtering. Do not compare group reliability directly with acknowledged unicast or diagnose the expected lack of ACK as failure.

## Power save

Apply power-save expectations only after confirming implementation. Track radio sleep/wake, AP knowledge, buffering, TIM/DTIM or trigger indication, buffered release, More Data state, and return to sleep. A radio state change alone does not prove AP buffering, U-APSD, TWT, or another complete protocol mechanism.

## Modern feature gate

For HT, VHT, HE, EHT, DMG/EDMG, S1G, OCB, or vehicular behavior, prove both implementation and configuration before applying standards invariants. In particular, do not infer:

* Actual MIMO or beamforming from a spatial-stream count.
* A-MPDU from generic aggregation.
* OFDMA or multi-user signaling from independent ordinary frames.
* EHT multi-link operation from several unrelated radios.
* Directional training from an omnidirectional 60 GHz configuration.
* Infrastructure association requirements for modes designed to operate differently.

Document the model abstraction when PHY signaling, resource units, per-user decisions, link identity, puncturing, channel matrices, or specialized management procedures are simplified or absent.
