# 802.11ax DL OFDMA Research Summary

## Core Stack

- **Platform**: OMNeT++ 6.3.0 + INET Framework
- **Language**: C++17, NED, and MSG
- **Build**: GNU Make with `opp_makemake`

## Table Stakes Features (v1)

1. **802.11ax Mode**: Enable `"ax"` modeSet and HE timing parameters.
2. **AP DL OFDMA Scheduler**: Selects multi-STA packets from the winning AC queue.
3. **PHY RU Model**: Treats Resource Units as independent parallel sub-channels on the radio medium.
4. **Sequential Ack**: Schedules back-to-back Block Ack responses from the STAs.

## Core Architectural Layout

- **MAC**: `Ieee80211Mac` intercepts TXOPs. The DL OFDMA Scheduler aggregates frames and builds an HE MU frame.
- **PHY**: `Ieee80211AxMode` defines HE parameters. The radio medium isolates RU sub-channels during signal reception.
- **STA MAC**: Extracts the MPDU corresponding to its assigned RU, then sends Block Ack in sequence.

## Critical Watch-Outs

- Perform CCA on the full primary channel bandwidth, not on individual RUs.
- Ensure strict timing offsets (SIFS) for sequential multi-user Block Ack sequences to avoid collisions.
- Prepend L-SIG legacy preamble to allow backward-compatible NAV settings for legacy nodes.
