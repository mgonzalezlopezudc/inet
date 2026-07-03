## Contents

* 28. Scenario Playbooks
* 28.1 No Beacon Seen
* 28.2 Probe Request Sent but No Probe Response
* 28.3 Authentication Succeeds but Association Fails
* 28.4 Data Queued but Never Transmitted
* 28.5 DATA Transmitted but No ACK
* 28.6 Repeated RTS With No CTS
* 28.7 Excessive Retransmissions
* 28.8 QoS Voice Traffic Has High Delay
* 28.9 Block Ack Window Stalls
* 28.10 AP Receives Wireless Frame but Server Does Not
* 28.11 Server Sends Response but STA Does Not Receive It
* 28.12 Roaming Causes Long Outage
* 28.13 Nearby Interferer Has No Effect
* 28.14 Every Overlap Fails, Even With a Much Stronger Desired Signal
* 28.15 Broadcast Works Poorly While Unicast Works

---

# 28. Scenario Playbooks

## 28.1 No Beacon Seen

1. Confirm AP management module is detailed enough to emit beacons.
2. Confirm beacon generation event.
3. Confirm radio tuned to intended channel.
4. Confirm transmission reaches medium.
5. Check STA channel at that time.
6. Check received power and interference.
7. Check PHY decode.
8. Check MAC address/BSSID filtering.
9. Check management delivery.
10. Check scan state.

## 28.2 Probe Request Sent but No Probe Response

1. Capture Probe Request at STA.
2. Confirm AP receives it.
3. Check SSID matching.
4. Check AP management logic.
5. Confirm response generated.
6. Confirm response transmission rate.
7. Check SIFS or response timing used by implementation.
8. Confirm STA remains on channel long enough.
9. Check PHY reception at STA.
10. Inspect scan timeout.

## 28.3 Authentication Succeeds but Association Fails

1. Confirm authentication state on both peers.
2. Decode Association Request.
3. Inspect capabilities and rates.
4. Decode response status.
5. Verify AP station-table insertion.
6. Verify STA state transition.
7. Check timeout races.
8. Check whether simplified and detailed management modules were mixed.
9. Check AP capacity or policy.
10. Check subsequent deauthentication.

## 28.4 Data Queued but Never Transmitted

1. Identify queue and access category.
2. Check head-of-line state.
3. Check association requirement.
4. Check PHY busy state.
5. Check NAV.
6. Check AIFS/DIFS wait.
7. Check backoff counter.
8. Check internal EDCA collision.
9. Check radio mode.
10. Check channel-access grant.
11. Check packet lifetime or queue drop.

## 28.5 DATA Transmitted but No ACK

1. Confirm ACK is expected.
2. Check group-address status.
3. Check ACK policy.
4. Confirm recipient PHY sees signal.
5. Check decode outcome.
6. Check recipient address filtering.
7. Check ACK generation.
8. Check recipient radio switching.
9. Confirm ACK transmission.
10. Check ACK power, mode, and interference at originator.
11. Check ACK timeout.
12. Determine whether DATA or ACK was lost.

## 28.6 Repeated RTS With No CTS

1. Confirm target address.
2. Confirm receiver tuned to channel.
3. Check RTS power and decode.
4. Check receiver’s CTS decision.
5. Confirm CTS mode.
6. Check SIFS timing.
7. Check CTS transmission.
8. Check hidden interference at originator.
9. Check CTS timeout.
10. Check retry-limit behavior.

## 28.7 Excessive Retransmissions

1. Group attempts by sequence and fragment.
2. Determine whether DATA, ACK, RTS, or CTS fails.
3. Compare recipient and originator captures.
4. Check hidden nodes.
5. Check rate selection.
6. Check link margin.
7. Check interference timing.
8. Check duplicate removal.
9. Check retry-counter reset.
10. Check contention-window evolution.

## 28.8 QoS Voice Traffic Has High Delay

1. Verify traffic-class tags.
2. Verify TID.
3. Verify access-category mapping.
4. Inspect voice queue.
5. Inspect AIFSN and contention windows.
6. Check internal collisions.
7. Check TXOP.
8. Check aggregation delay.
9. Check rate-control response.
10. Compare channel occupancy by other categories.

## 28.9 Block Ack Window Stalls

1. Record agreement TID and window.
2. List sent MPDU sequences.
3. Decode bitmap.
4. Identify first missing sequence.
5. Confirm retransmission.
6. Check retry limit.
7. Check recipient reorder timeout.
8. Check window advancement.
9. Check agreement expiration.
10. Inspect duplicate and old-sequence handling.

## 28.10 AP Receives Wireless Frame but Server Does Not

1. Decode To DS and address fields.
2. Confirm AP accepts MPDU.
3. Confirm decapsulation.
4. Inspect bridge or forwarding table.
5. Confirm output interface.
6. Check VLAN and filtering.
7. Capture AP wired interface.
8. Inspect ARP/ND and IP routing.
9. Check server link capture.
10. Separate 802.11 failure from distribution-system failure.

## 28.11 Server Sends Response but STA Does Not Receive It

1. Capture server transmission.
2. Capture AP wired ingress.
3. Check AP forwarding lookup.
4. Confirm STA association.
5. Confirm downlink 802.11 frame construction.
6. Decode From DS and address fields.
7. Inspect AP queue and access category.
8. Check downlink PHY and interference.
9. Check STA ACK.
10. Check upper-layer delivery.

## 28.12 Roaming Causes Long Outage

1. Determine handover trigger.
2. Measure scan start.
3. Record dwell per channel.
4. Record Probe Responses or Beacons.
5. Measure candidate selection.
6. Measure authentication.
7. Measure association.
8. Check new AP forwarding state.
9. Check stale old-AP state.
10. Measure first successful data.
11. Separate L2, L3, and transport delays.

## 28.13 Nearby Interferer Has No Effect

1. Confirm same radio medium.
2. Confirm frequency overlap.
3. Confirm dimensional or appropriate interference model.
4. Check signal spectral shape.
5. Check received interference power.
6. Check receiver `ignoreInterference`-like settings.
7. Check error model enabled.
8. Check simultaneous timing.
9. Check medium caches.
10. Verify the interferer is not below detection at all receivers.

## 28.14 Every Overlap Fails, Even With a Much Stronger Desired Signal

1. Check receiver capture behavior.
2. Check whether capture effect is modeled.
3. Check preamble arrival ordering.
4. Check SNIR calculation interval.
5. Check error model.
6. Check whether reception selection locks onto first signal.
7. Document model limitation if appropriate.

## 28.15 Broadcast Works Poorly While Unicast Works

1. Confirm no ACK is expected.
2. Compare PHY rates.
3. Check basic-rate selection.
4. Check absence of retries.
5. Check AP forwarding.
6. Check power-save buffering.
7. Check interference.
8. Compare recipient sensitivity and coverage.
9. Avoid treating ordinary broadcast loss as an ACK defect.

---

