# Scope and reproducibility

## Inspect the instantiated model

Reason from the effective NED and INI configuration, not from a similarly named example. Identify only the components involved in the question:

* Wireless interface, MAC, management, agent, queue, classifier, and radio types.
* Radio-medium and analog representation.
* Mode set, channel, frequency, and bandwidth.
* Protection, ACK, retry, fragmentation, aggregation, Block Ack, and rate-control policies.
* Sender, receiver, AP, and relevant intermediate module paths.

Use `inet-ned-ini-analysis` when inheritance, wildcard precedence, or `typename` selection is unclear. Treat the checked-out INET source as authoritative for implemented behavior.

## Establish feature gates

Before diagnosing a standards violation, distinguish four questions:

| Question | Evidence |
| --- | --- |
| Does the scenario require the feature? | Expected exchange or test contract |
| Does the applicable standard define it? | `ieee80211-standards` |
| Does this INET checkout implement it? | NED types and source paths |
| Is it enabled in this run? | Effective configuration and runtime evidence |

Apply this gate to RTS/CTS, QoS/EDCA, fragmentation, A-MSDU/A-MPDU, Block Ack, power save, roaming, HT/VHT/HE/EHT, OFDMA, multi-user behavior, and multi-link behavior. Treat unsupported behavior differently from incorrect behavior.

## Preserve comparable runs

Start with one configuration, run number, and seed. When comparing before and after, keep the seed, build mode, NED path, effective configuration, traffic, and diagnostic overrides equivalent unless the changed item is the variable under test.

Use command-line overrides for temporary capture, logging, event-log, and result diagnostics. Create a dedicated debug configuration only when repeated investigation would otherwise be error-prone.

Record generated captures, logs, event logs, and result files by configuration and run. Do not compare independent randomized trajectories as though they were the same reproduction.
