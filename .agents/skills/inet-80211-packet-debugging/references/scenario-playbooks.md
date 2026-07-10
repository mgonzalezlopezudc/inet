# IEEE 802.11 scenario playbooks

Use each playbook to choose the first distinctions to prove, not as a requirement to collect every possible datum.

## Management

**No Beacon:** prove generation, transmission, STA channel/dwell, PHY decode, BSSID/SSID filtering, and management delivery.

**Probe Request without response:** prove AP reception, SSID/policy match, response generation, STA channel dwell, response PHY decode, and scan timeout ordering.

**Authentication succeeds but association fails:** decode request/response status and capabilities, then compare AP station-table insertion, STA state transition, timeouts, and management-module fidelity.

**Roaming outage:** measure trigger, scan dwell, candidate selection, authentication/reassociation, new AP forwarding state, and first successful data; separate L2, L3, and transport delay.

## Channel access and responses

**Queued but never transmitted:** locate the queue/AC and association gate, then reconstruct PHY busy, NAV, AIFS/DIFS, backoff freeze/resume, internal collision, radio state, access grant, lifetime, and queue drop.

**DATA without ACK:** first confirm ACK policy and group/unicast status. Then distinguish DATA decode/filtering, ACK generation/transmission, ACK decode, and timeout at the originator.

**RTS without CTS:** distinguish RTS decode, receiver CTS decision, SIFS scheduling, CTS mode/transmission, CTS interference at the originator, and timeout/retry handling.

**Excessive retransmission:** group attempts by TA/RA/TID/sequence/fragment, identify which exchange member fails, and inspect link margin, interference, rate selection, ACK loss, retry reset, and contention-window evolution.

**QoS delay:** prove classification and TID/AC mapping, then inspect queueing, AIFS/CW, internal collisions, TXOP, aggregation delay, rate response, and competing channel occupancy.

**Block Ack stall:** record agreement/TID/window, sent sequences, bitmap and first missing sequence, retransmission/retry limit, reorder timeout, window advancement, and agreement expiry.

## Forwarding and PHY

**AP receives wireless DATA but server does not:** decode DS/address fields, prove AP acceptance and decapsulation, then trace bridge output, wired capture, filtering/VLAN, and network-layer resolution.

**Server response does not reach STA:** trace server output, AP wired ingress, forwarding lookup and association, downlink frame construction, AP queue/access, STA PHY/ACK, and upper-layer delivery.

**Nearby interferer has no effect:** prove shared medium, time/frequency overlap, received interference power, analog/error model support, receiver interference settings, and simultaneous timing.

**Every overlap fails despite a stronger desired signal:** inspect preamble ordering, reception selection/lock, SNIR intervals, error model, and whether capture-like behavior exists; report a model limitation when appropriate.

**Broadcast underperforms unicast:** account for basic-rate selection, absence of ACK/retry, AP group forwarding, power-save buffering, interference, and receiver coverage before treating it as a defect.
