# 802.11ax DL OFDMA Architecture Research

## Component Boundaries

### MAC Layer (AP Side)
- **`Ieee80211Mac` / `Hcf`**: Intercepts winning EDCA channel access opportunities.
- **DL OFDMA Scheduler**: A new scheduling component at the AP that:
  1. Identifies the winning Access Category (AC).
  2. Scans the winning AC's `pendingQueue` for packets addressed to different active stations (STAs).
  3. Selects up to N packets (where N is the number of available RUs).
  4. Generates an HE Multi-User (MU) physical frame metadata structure detailing RU allocations.
- **`Tx` component**: Formats and transmits the multi-user frame down to the physical layer.

### PHY Layer
- **HE PHY Mode (`Ieee80211AxMode`)**: Extends `Ieee80211ModeBase` to supply HE slot times, IFS times, and MCS tables.
- **RU Sub-channel Medium**:
  - The physical transmitter divides the allocated bandwidth into RUs based on the scheduler's allocations.
  - The radio medium delivers the signal to the receiving STAs.
  - Each destination STA only decodes the signal from its assigned Resource Unit (RU) sub-channel, performing independent path loss, SNR, and bit/packet success rate calculations.

### MAC Layer (STA Side)
- **`Rx` component**: Processes the incoming frame, checks the HE-SIG-B allocation metadata, and extracts the MPDU addressed to this specific STA.
- **Ack / BlockAck Procedure**: Triggers a sequential transmission of Block Acks back to the AP.

## Data Flow Diagram

```
[ AP MAC Queue (Winning AC) ]
              │
              ▼ (extract multi-STA packets)
     [ DL OFDMA Scheduler ]
              │
              ▼ (assign RUs & build HE MU frame)
      [ AP PHY / Radio ] ─── (transmit on RUs in parallel) ───┐
                                                               │
     ┌─────────────────────────┬───────────────────────────────┘
     ▼                         ▼
[ STA-A PHY (RU 1) ]      [ STA-B PHY (RU 2) ]
     │                         │
     ▼                         ▼
[ STA-A MAC (decapsulate) ] [ STA-B MAC (decapsulate) ]
     │                         │
     ▼ (send sequential Ack)   ▼ (send sequential Ack after SIFS)
[ STA-A TX BlockAck ]     [ STA-B TX BlockAck ]
```

## Build Order Implications
1. **HE Mode definition**: Define `Ieee80211AxMode` and support `ax` modeSet.
2. **HE PHY Header & Preamble**: Represent HE MU PPDU headers and SIG fields.
3. **AP DL OFDMA Scheduler**: C++ class implementing multi-user packet aggregation.
4. **PHY Layer RU Reception**: Handle multi-user parallel reception and independent sub-channel noise calculations.
5. **Sequential Block Ack**: Support sequential MAC acknowledgment sequences.
