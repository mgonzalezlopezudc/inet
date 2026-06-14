# Architecture

**Analysis Date:** 2026-06-14

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
- Contains: Ethernet, PPP, and IEEE 802.11 MAC/LLC implementations.
- Depends on: Physical layer interface.
- Used by: Network layer protocols.

**Physical Layer (`src/inet/physicallayer/`):**
- Purpose: Physical signal transmission and antenna propagation.
- Contains: Transmitter, receiver, propagation model, and radio medium.
- Depends on: Common utility classes.
- Used by: Link layer protocols.

## Data Flow

**Simulation Packet Transmission (Down and Up Stack):**

1. **Traffic Initiation:** An application module (`PingApp`) creates an `inet::Packet` containing data chunks, schedules a self-message to send it, and passes it down through `socketOut`.
2. **Transport Encapsulation:** The transport module (e.g. `Udp`) adds its header (e.g. `UdpHeader`) as a chunk to the packet.
3. **Network Routing:** The network module (e.g. `Ipv4`) resolves the destination address, prefixes the network header (`Ipv4Header`), and chooses the network interface.
4. **Link Framing:** The link module (e.g. `EthernetMac`) serializes/envelopes the frame and transmits it.
5. **Physical Propagation:** The physical layer calculates signals and delivers them across the channel.
6. **Decapsulation:** The receiver host performs the reverse steps up the stack, removing headers and delivering payload to the destination application.

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

*Architecture analysis: 2026-06-14*
*Update when major patterns change*
