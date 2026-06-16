# Architecture Research

**Domain:** Wireless Simulation / IEEE 802.11ax DL MU OFDMA Correctness
**Researched:** 2026-06-16
**Confidence:** HIGH

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                       MAC Layer (L2)                        │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐   ┌───────────────┐   ┌───────────────┐   │
│  │   HeHcf      ├───►  Edcaf        ├───► HeDlScheduler │   │
│  │ (Coordination│   │(ChannelAccess)│   │ (RU Scheduler)│   │
│  └──────┬───────┘   └───────────────┘   └───────────────┘   │
│         │                                                   │
│  ┌──────▼───────┐                                           │
│  │HeDlMuTxOpFs  │                                           │
│  │ (Frame Seq)  │                                           │
│  └──────┬───────┘                                           │
├─────────┼───────────────────────────────────────────────────┤
│         │             PHY Layer (L1)                        │
├─────────┼───────────────────────────────────────────────────┤
│  ┌──────▼───────┐   ┌───────────────────────────┐           │
│  │Ieee80211Radio├───►   Resource Units (RUs)    │           │
│  │(Transmitter) │   │ (Independent sub-channels)│           │
│  └──────────────┘   └───────────────────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| `HeHcf` | Coordinates MAC transmission/reception, wins TXOPs, and triggers multi-user sequences. | C++ simple module (`HeHcf.cc`) connected to MAC layer gates. |
| `HeDlSchedulerEqualSizedRUs` | Inspects pending queues, matches packets to STAs, and assigns them to available Resource Units. | C++ class implementing `IIeee80211HeDlScheduler` interface. |
| `HeDlMuTxOpFs` | Manages the step-by-step transmission of the HE-MU PPDU container, subsequent BARs, and reception of sequential Block Acks. | C++ class inheriting from `IFrameSequence` interface. |
| `Ieee80211Radio` | Converts packets into wave signals and performs independent attenuation/SNR calculations per sub-channel band. | C++ simple module with transmitter/receiver submodules. |

## Architectural Patterns

### Pattern 1: Multi-User Frame Container
**What:** Packets destined to multiple STAs are enclosed within a container packet (`HE-MU-PPDU`) carrying an `Ieee80211HeMuTag`.
**When to use:** Used by `HeDlMuTxOpFs` to send multi-user frames concurrently.
**Trade-offs:** Avoids duplicating standard preamble fields but requires the receiver's physical layer to filter allocations.

### Pattern 2: Interface-Driven Scheduling
**What:** The MAC coordination function (`HeHcf`) delegates RU allocation to an implementation of `IIeee80211HeDlScheduler`.
**When to use:** Allows switching scheduling algorithms (e.g. equal-sized RUs vs proportional fair) without modifying frame sequence code.

## Data Flow

### Request Flow

```
[Winning AC wins TXOP]
          │
[HeHcf triggers scheduler] ──► [HeDlScheduler assigns RUs]
          │
[HeDlMuTxOpFs builds container PPDU with allocations]
          │
[Ieee80211Radio transmits container over sub-channels]
```

### Sequential Ack Flow

```
[AP transmits HE-MU-PPDU] ──► [STAs receive on assigned RUs]
          │
[AP schedules BAR 1] ──► [STA 1 responds with BlockAck]
          │
[AP schedules BAR 2] ──► [STA 2 responds with BlockAck]
```

## Anti-Patterns

### Anti-Pattern 1: Direct Queue Modification in Scheduler
**What people do:** The scheduler pops packets directly from `IPacketQueue`.
**Why it's wrong:** If the transmission sequence fails or is aborted before transmission, packets are lost.
**Do this instead:** The scheduler only *inspects* queues. Packets are removed only when the transmission sequence starts.

### Anti-Pattern 2: Dynamic Casts in Event Loop
**What people do:** Using raw C-style or `dynamic_cast` to cast packets/headers.
**Why it's wrong:** Causes segmentation faults when casting fails due to unexpected frames.
**Do this instead:** Always use OMNeT++'s `check_and_cast<T*>()` which halts with a descriptive stack trace.

---
*Architecture research for: 802.11ax DL MU OFDMA correctness*
*Researched: 2026-06-16*
