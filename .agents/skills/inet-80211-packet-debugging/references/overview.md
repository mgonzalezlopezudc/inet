# Debugging IEEE 802.11 Packet Exchange in OMNeT++/INET Without Qtenv

## Purpose

Use this skill to investigate IEEE 802.11 packet-generation, channel-access, transmission, reception, acknowledgment, aggregation, retransmission, association, roaming, forwarding, and packet-drop problems in an OMNeT++/INET simulation when Qtenv is unavailable.

The skill covers:

* IEEE 802.11 physical-layer behavior
* Physical and virtual carrier sensing
* DCF and EDCA channel access
* Interframe spaces and contention windows
* ACK, RTS/CTS, Block Ack, and retry procedures
* Fragmentation, A-MSDU, and A-MPDU behavior
* QoS traffic classification and access categories
* Management-frame exchanges
* Scanning, authentication, association, and handover
* AP forwarding and 802.11 address interpretation
* Radio propagation, interference, reception, and error models
* PHY rates, modulation modes, MCS selection, and rate control
* Hidden and exposed stations
* Power-save and modern amendment features when modeled
* Command-line packet analysis
* OMNeT++ scalar, vector, log, and event-log analysis
* C++ debugging with LLDB

IEEE 802.11 defines a common MAC and multiple PHY specifications. The consolidated standard evolves over time, while individual INET versions implement only a subset of the standard. IEEE 802.11-2024 is a consolidated base revision, and later amendments may add further behavior. Never assume that a feature described by the standard is implemented by the checked-out INET source.

INET’s 802.11 model is decomposed into MAC coordination functions, channel-access functions, data services, policy modules, management modules, agents, radios, transmitters, receivers, error models, and radio-medium components. The exact module hierarchy and parameters vary across INET versions.

---

## Core Principle

Debug the exchange in layers:

```text
Application or network-layer packet
        ↓
802.11 management or data service
        ↓
MAC queue and QoS classification
        ↓
DCF, EDCA, or another coordination function
        ↓
Frame exchange: DATA, RTS, CTS, ACK, Block Ack, management
        ↓
PHY transmission construction
        ↓
Radio medium, propagation, attenuation, noise, interference
        ↓
Receiver detection, synchronization, decoding, error decision
        ↓
Recipient MAC processing
        ↓
Duplicate removal, reordering, defragmentation, deaggregation
        ↓
Upper-layer delivery
```

Do not jump directly from “the application sent a packet” to “the receiver did not get it.”

Identify the first layer where observed behavior diverges from expected behavior.

---

