# 802.11ax DL OFDMA Features Research

## Table Stakes (Must-Have for v1)

- **802.11ax Mode Enablement**: Add the `"ax"` modeSet to `Ieee80211Mac` and define HE physical parameters (slot times, IFS, CW limits) in `Ieee80211ModeSet`.
- **Resource Unit (RU) Division**: Support standard 20 MHz channel partitioning into smaller RUs (e.g., 26-tone, 52-tone, 106-tone, and 242-tone).
- **AP DL OFDMA MAC Scheduler**: Multi-user scheduler that aggregates frames from the winning EDCAF queue and allocates them to different STAs using distinct RUs.
- **Physical Layer RU sub-channels**: Represent RUs as parallel independent sub-channels on the radio medium to calculate SNR, path loss, and packet success rate independently for each destination STA.
- **Sequential Block Acknowledgment**: Support sequential acknowledgment responses from receiving stations on their respective RUs after receiving the DL MU frame.

## Differentiators (Deferred/Out of Scope)

- **Uplink OFDMA (UL OFDMA)**: Simultaneous transmissions from multiple STAs back to the AP using trigger frame coordination.
- **Multi-User Block Ack Requests (MU-BAR)**: Using a single trigger-based request to coordinate multi-user acknowledgments.
- **Per-STA Queue System**: Maintaining a dedicated set of EDCA queues for each connected STA at the AP.
- **Dynamic RU Subcarrier Interference**: Modeling individual subcarrier frequencies and fading patterns.

## Complexity & Dependencies
- The DL OFDMA MAC scheduler depends directly on the winning AC queue's state and destination addresses.
- The physical layer sub-channel receiver calculations depend on the scheduler's RU assignments embedded in the physical frame's HE-SIG-B metadata.
