# Architecture

**Analysis Date:** 2026-06-16

## Pattern Overview

**Overall:** Component-Based Layered Simulation Architecture (following the TCP/IP and OSI Reference Models).

**Key Characteristics:**
- **Modular Composition:** Compound modules (like `StandardHost` and `Router`) are assembled from interchangeable submodules implementing individual layer protocols.
- **Discrete Event Simulation:** Driven by an event loop. Events are messages (`cMessage`) scheduled for execution at a specific simulation time.
- **Message-Passing & Flow:** Submodules interact by exchanging packet objects (`inet::Packet`) via connected gates (`gates`) or direct method calls using interfaces.
- **Lifecycle Support:** Nodes and submodules inherit lifecycle behavior, supporting crash, stop, start, and reboot states at runtime.

## Layers

**Application Layer (`src/inet/applications/`):**
- Purpose: Traffic generators and network services.
- Contains: Ping app, UDP/TCP traffic generators, DHCP clients/servers, and visualizer modules.
- Depends on: Transport Layer interfaces (`IApp` contracts).
- Used by: Node boundary definitions.

**Transport Layer (`src/inet/transportlayer/`):**
- Purpose: End-to-end communication services.
- Contains: TCP, UDP, and SCTP protocol implementations.
- Depends on: Network Layer interfaces.
- Used by: Application layer modules via transport gates.

**Network Layer (`src/inet/networklayer/`):**
- Purpose: Packet routing and interface address configuration.
- Contains: IPv4, IPv6, ARP, ICMP, and routing tables.
- Depends on: Link Layer interfaces.
- Used by: Transport layer protocols.

**Link Layer (`src/inet/linklayer/`):**
- Purpose: Link transmission and media access control.
- Contains: Ethernet, PPP, and IEEE 802.11 MAC/LLC implementations. Integrates the 802.11ax DL OFDMA scheduler (`IIeee80211HeDlScheduler`) at the AP and the multi-user TXOP frame sequence handler (`HeFrameSequenceHandler`) for managing downlink multi-user frame transmissions and subsequent sequential BlockAck sequences.
- Depends on: Physical layer interface.
- Used by: Network layer protocols.

**Physical Layer (`src/inet/physicallayer/`):**
- Purpose: Physical signal transmission, antenna propagation, and reception.
- Contains: Transmitter, receiver, propagation model, and radio medium. Integrates the HE mode representation (`Ieee80211HeMode`), HE MU PHY header serialization, and the Resource Unit (`Ieee80211HeRu`) model where the radio medium (`Ieee80211RadioMedium`) treats independent RUs as parallel, interference-isolated sub-channels.
- Depends on: Common utility classes.
- Used by: Link layer protocols.

## Data Flow

**Simulation Packet Transmission (Down and Up Stack):**

1. **Traffic Initiation:** An application module (`PingApp`) creates an `inet::Packet` containing data chunks, schedules a self-message to send it, and passes it down through `socketOut`.
2. **Transport Encapsulation:** The transport module (e.g. `Udp`) adds its header (e.g. `UdpHeader`) as a chunk to the packet.
3. **Network Routing:** The network module (e.g. `Ipv4`) resolves the destination address, prefixes the network header (`Ipv4Header`), and chooses the network interface.
4. **Link Framing & OFDMA Scheduling:**
   - Single-User: The link module (e.g. `EthernetMac`) serializes/envelopes the frame and transmits it.
   - Downlink MU-OFDMA (IEEE 802.11ax): When an Access Category (AC) wins a TXOP, the MAC coordination module (`HeHcf`) queries the HE DL scheduler (`HeDlSchedulerEqualSizedRUs`). The scheduler selects pending packets for up to N destination stations from the AC's queue and maps them to equal-sized Resource Units (RUs).
5. **Physical MU Transmission:** The physical transmitter (`Ieee80211Transmitter`) forms a Downlink Multi-User (DL MU) frame carrying multiple independent PHY sub-transmissions (one per RU).
6. **Radio Medium Propagation:** The radio medium (`Ieee80211RadioMedium`) calculates path loss and propagation per RU sub-channel independently, avoiding self-interference.
7. **Reception & Filtering:** Each destination station receiver (`Ieee80211Receiver`) parses the HE MU PHY header, identifies its assigned RU allocation using the HE MU Tag, filters out other sub-transmissions, decodes its payload, and passes it up.
8. **Decapsulation:** The receiver host performs the reverse steps up the stack, removing headers and delivering payload to the destination application.
9. **Sequential Acknowledgments:** Following a DL MU frame transmission, the AP's coordination sequence handler (`HeFrameSequenceHandler` / `HeDlMuTxOpFs`) coordinate sequential acknowledgments by sending individual BlockAckRequests (BAR) to each STA, receiving individual BlockAcks (BA) sequentially before completing the TXOP.

**State Management:**
- Each module maintains its own local C++ state (variables, timers, statistics objects).
- Global network state (routing tables, address mappings, physical environment) is stored in global configuration modules (e.g. `Ipv4NetworkConfigurator` and `InterfaceTable`).

## Key Abstractions

**cSimpleModule:**
- Purpose: Base class for all C++ simulation code in OMNeT++.
- Examples: `PingApp`, `Ipv4`, `Udp`.
- Pattern: Hook Methods (`initialize()`, `handleMessage()`, `finish()`).

**inet::Packet:**
- Purpose: Container for simulated data payloads and headers.
- Examples: Extends `cPacket` using a dynamic chunk system (`inet::FieldsChunk`).

**LifecycleOperation:**
- Purpose: Triggers state changes like Start, Stop, or Crash across submodules.
- Examples: `LifecycleOperation` handled in `handleStartOperation` and `handleStopOperation`.

**Ieee80211HeRu:**
- Purpose: Represents a Resource Unit partition in 802.11ax (HE), defining sub-channel bandwidth, center frequency, and subcarrier range.

**IIeee80211HeDlScheduler:**
- Purpose: Interface for Downlink OFDMA MAC schedulers at the AP.

## Entry Points

**cSimpleModule Hook Entry:**
- Location: Native OMNeT++ runner invokes `initialize(int stage)` and `handleMessage(cMessage *msg)` on each simple module.
- Triggers: Start of simulation, packet arrival at gates, or expiration of scheduled self-messages (timers).

## Error Handling

**Strategy:** Simulation failure on critical violations via C++ runtime exceptions.

**Patterns:**
- Call `throw cRuntimeError("description")` to instantly halt the simulation with a stack trace and print the error location.
- Assert validation via `ASSERT` macros.

---

*Architecture analysis: 2026-06-16*
*Update when major patterns change*
