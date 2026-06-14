# External Integrations

**Analysis Date:** 2026-06-14

## APIs & External Services

**Network Emulation (Raw Sockets):**
- System raw sockets/TAP interfaces - For connecting simulation packets to real physical networks.
  - SDK/Client: Linux sockets API (`sys/socket.h`, `net/if.h`, `pcap.h` for packet capture).
  - Auth: Requires root or `CAP_NET_ADMIN` / `CAP_NET_RAW` privileges on the host system.
  - Files: `src/inet/emulation/` (e.g., `ExtLowerEthernetMac.cc`, `ExtInterface.cc`).

**External Routing Integrations:**
- Quagga/FRRouting (optional) - OSPF/BGP routing engine integrations.
  - Integration Method: TCP/UDP socket connections.
  - Files: `src/inet/routing/bgpv4/bgp/` and OSPF/RIP integration wrappers.

## Data Storage

**Simulation Results (Scalar & Vector files):**
- OMNeT++ native result format (`.sca`, `.vec`) - Custom binary and text formats representing event records, vectors, and scalars.
  - Connections: File writes to the configured `results/` directory during run.
  - Client: SQLite option available via OMNeT++ `cSQLResultManager`.

**PCAP File Writer:**
- Wireshark/Pcap format - Injects simulated packet streams into `.pcap` files for external analysis.
  - Client: Custom PCAP serialization code inside `PcapRecorder` module.
  - Files: `src/inet/common/recorder/PcapRecorder.cc`.

## Authentication & Identity

- None. No user login, OAuth, or authentication services are present in the core simulation codebase (runs purely locally in simulation time).

## Monitoring & Observability

- **Qtenv GUI** - Interactive Qt-based visual interface for stepping through events and tracing message routes.
- **Cmdenv CLI** - Fast command-line run option printing text events and warning outputs.
- **EV Logging System** - Custom macro-based logging in simple modules (`EV_INFO`, `EV_WARN`, `EV_DETAIL`) which routes text streams dynamically to log consoles.

## CI/CD & Deployment

**CI Pipeline:**
- **GitHub Actions / Travis CI / Jenkins** - Automates builds and verification testing.
  - Workflows: `.github/workflows/` (e.g., compile checks, fingerprint tests).
  - Configs: `.travis.yml` in root.

## Environment Configuration

**Sourced Environment:**
- Environment variables set in `setenv`:
  - `INET_ROOT`: Points to repository root.
  - `INET_NED_PATH`: NED package resolution search paths.
  - `PATH` modifications to expose `opp_run` and other tools.

## Webhooks & Callbacks

- **OMNeT++ Lifecycle operations**: Standard callbacks invoked when node status changes (e.g., `handleStartOperation`, `handleStopOperation`).
- **Socket callbacks**: Transport/Network sockets use event callback registration (`INetworkSocket::ICallback` and `ISocketCallback`).

---

*Integration audit: 2026-06-14*
*Update when adding/removing external services*
