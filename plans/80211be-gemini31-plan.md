# Implement 802.11 EHT (Wi-Fi 7) MAC & PHY

This detailed implementation plan covers the introduction of 802.11be EHT (Extremely High Throughput, Wi-Fi 7) into INET. It focuses on packet-level EHT PHY behavior (320 MHz, 4096-QAM, MRUs), integrated MAC-level Multi-Link Operation (MLO), and EHT Multi-User scheduling, while preserving existing HE/AX behaviors.

> [!TIP]
> **Standards-Backed Implementation Workflow**
> - All constants, frame formats, and tables will be referenced from the `index.sqlite` standards database via `bin/inet_process_standards search "<term>"`. 
> - Chunk IDs will be recorded in C++ comments for traceability.

---

## 1. EHT PHY Headers & Resource Units (MRU)

### EHT Resource Units (MRU) Implementation
- **Allocation Class**: Create `Ieee80211EhtMruAllocation` to replace/extend the HE RU logic. It will define the standard tone plans for 20/40/80/160/320 MHz.
- **MRU Support**: EHT introduces Multiple Resource Units (MRU) per user. The allocation class will validate allowed MRU combinations and their indices according to 802.11be Clause 36.3.12.
- **Preamble Puncturing**: A puncturing bitmap will be added to the MAC-to-PHY control info. `Ieee80211EhtMruAllocation` will validate this bitmap against allowed EHT puncturing patterns (e.g., removing specific 20 MHz subchannels).

### EHT PHY Headers
- **Classes**: Create `Ieee80211EhtPhyHeader` extending the base PHY header.
- **Fields**: U-SIG (Universal SIG) and EHT-SIG.
- **PHY Calculator**: Implement `Ieee80211EhtPhyCalculator` to translate MRU indices and 4096-QAM MCS into `numberOfSubcarriers` and compute exact EHT symbol durations.

---

## 2. Management Frames and Serializers

The management element architecture will follow the existing HE pattern (e.g., `Ieee80211HeMgmtElements.h`). 
1. **MIB Structs**: `Ieee80211EhtCapabilities`, `Ieee80211EhtOperation` (already partially defined in `mib/`).
2. **Message Elements**: We will add the following element definitions to `Ieee80211MgmtFrame.msg` (or an EHT-specific `.msg`):
   - `Ieee80211EhtCapabilitiesElement`
   - `Ieee80211EhtOperationElement`
   - `Ieee80211MultiLinkElement` (MLE)
   - `Ieee80211TidToLinkMappingElement`
3. **Element Conversion**: Create `src/inet/linklayer/ieee80211/mgmt/Ieee80211EhtMgmtElements.h` containing translation functions:
   - `makeEhtCapabilitiesElement(const Ieee80211EhtCapabilities&)`
   - `makeEhtCapabilities(const Ieee80211EhtCapabilitiesElement&)`
   - `getEhtMgmtElementsLength(const Ptr<const Ieee80211MgmtFrame>&)`
4. **Serialization**: In `Ieee80211MgmtFrameSerializer.cc`, write exact bit-level serialization/deserialization for the EHT `.msg` elements, translating the element fields into their on-the-wire chunk formats based on the 802.11be standard.

---

## 3. AP/STA Management Flows

The management state machines will be updated in the `mgmt` folder to negotiate EHT and MLO.

### Explicit Management Flow (`Ieee80211MgmtAp.cc`, `Ieee80211MgmtSta.cc`)
- **Beaconing / Probing**: 
  - `Ieee80211MgmtAp::sendBeacon()` and `sendProbeResponse()`: Read the AP's local MIB EHT Capabilities/Operation and convert them to `EhtCapabilitiesElement`/`EhtOperationElement`. If MLO is enabled, construct a `MultiLinkElement` carrying Per-STA profiles for the other links belonging to the MLD. Add these to the `Ieee80211BeaconFrame`.
  - `Ieee80211MgmtSta::handleBeacon()`: Parse these new elements and update the `apEhtCapabilities` and MLO profiles in its internal AP list.
- **Association (MLO Setup)**:
  - `Ieee80211MgmtSta::sendAssociationRequest()`: Insert the STA's `EhtCapabilitiesElement` and an MLE containing its `mldMacAddress` and the subset of links it intends to associate with.
  - `Ieee80211MgmtAp::handleAssociationRequest()`: Extract the elements. Call `negotiateEhtCapabilities()` against its local capabilities. Process the requested links in the MLE, allocate Link IDs, and formulate the `AssociationResponse` containing the negotiated MLE.
  - `Ieee80211MgmtSta::handleAssociationResponse()`: Apply the negotiated Link IDs and capabilities to its local `Ieee80211MldMac` state.

### Simplified Management Flow (`Ieee80211MgmtApSimplified.cc`, `Ieee80211MgmtStaSimplified.cc`)
- Bypass byte-level frame serialization completely.
- `Ieee80211Agent` coordinates the flow. When the STA connects, it exchanges `Ieee80211AgentCommand` messages that directly carry C++ object references to `Ieee80211EhtCapabilities` and `Ieee80211MultiLinkState`.
- `Ieee80211MgmtApSimplified` and `Ieee80211MgmtStaSimplified` will instantly negotiate the EHT parameters and Link mapping locally via pointer passing, transitioning directly to the authenticated/associated state and updating the `Ieee80211MldMac` without overhead.

---

## 4. Integrated MLO inside the MAC

To model MLO faithfully according to the 802.11 standard, the MAC layer will be split into an Upper MAC (handling MLD functions) and multiple Lower MACs (handling link-specific functions).

### Queuing (Standard Context)
The 802.11 standard models QoS with **per-AC (Access Category) queues** at the MLD level. Each AC (VO, VI, BE, BK) has its own queue, and the TID-to-Link mapping determines which links can pull from which AC queues. 

### NED Modules and C++ Classes Breakdown

#### `inet.linklayer.ieee80211.mac.Ieee80211MldMac` (Upper MAC)
- **Responsibilities**: 
  - Represents the primary Multi-Link Device (MLD) identity. Holds the `mldMacAddress`.
  - Maintains the **per-AC queues** (VO, VI, BE, BK).
  - Manages the **Sequence Number Space** (per-TID).
  - Manages **BlockAck Agreements** and Reorder Buffers (per-TID, agnostic of the link the frame arrived on).
  - Performs **Traffic Steering / TID-to-Link Mapping**.
- **Pluggable Traffic Steering**:
  - We will introduce an `IMloTrafficSteeringPolicy` interface.
  - Pluggable C++ classes like `EhtMloAirtimeSteeringPolicy` or `EhtMloBacklogSteeringPolicy` can be configured. This is distinct from HE MU scheduling (which schedules multiple STAs in an OFDMA PPDU).
- **Connections**: Connects to the Upper Layer (Network) on one side, and has an array of connections to `Ieee80211Mac` (Lower MAC) submodules on the other side.

#### `inet.linklayer.ieee80211.mac.Ieee80211Mac` (Lower MAC / Link MAC)
- **Responsibilities**: 
  - Represents a single link of the MLD. Holds the `linkId` and `linkLocalMacAddress`.
  - Manages link-specific HCF/DCF state machines, NAV timers, and contention windows. 
  - Because EHT is built on HE, the Lower MAC will instantiate **HE/EHT specific HCFs** (e.g., `EhtHcf` which inherits from `HeHcf`), allowing it to handle MU EDCA timer operations.
  - Manages link-specific rate control instances (e.g., `EhtMinstrelRateControl`).
  - Contains link-specific submodules (`rx`, `tx`, `dcf`, `hcf`).

### Conflict Handling & Link States
- **STR (Simultaneous Transmit and Receive)** vs **NSTR (Non-STR)**: A central cross-link state tracker in the `Ieee80211MldMac` will pause HCF backoffs in NSTR links when a paired link initiates transmission/reception.

---

## 5. EHT MU Schedulers and Frame Sequences

EHT Multi-User operation will reuse the HE MU machinery but extend it for 802.11be specifics.

### Frame Sequences
EHT largely retains the HE frame sequence state machine concepts, but will be subclassed or extended to handle EHT-specific constraints, PHY limits, MRUs, and cross-link BAs for MLO.
- **Single-User Frame Sequences**: EHT SU will reuse and extend the standard `TxOpFs` or `HtTxOpFs`, adapting it to construct EHT PHY headers. If significant divergence occurs (e.g., MLO specific TXOP management), we will introduce `EhtTxOpFs` as a direct subclass.
- **Multi-User Frame Sequences**: EHT MU will directly subclass the HE MU frame sequences:
  - **`EhtDlMuTxOpFs`** (Extends `HeDlMuTxOpFs`): EHT Downlink Multi-User sequence. Applies EHT MRU allocations, 4096-QAM, and EHT PHY headers (U-SIG/EHT-SIG).
  - **`EhtUlMuTxOpFs`** (Extends `HeUlMuTxOpFs`): EHT Uplink Multi-User sequence. Sends an EHT Trigger frame and expects EHT Trigger-Based (TB) PPDUs.

### Trigger Frames and BSR
- **Trigger Frames**: Create `Ieee80211EhtTriggerFrame` extending the standard HE trigger frame. Update the User Info fields to specify EHT MRU allocations (including 3984-tone 320 MHz allocations), U-SIG puncturing info, and 4096-QAM MCS targets.
- **Buffer Status Report (BSR)**: Update BSR parsing so APs accurately track queue sizes of EHT STAs.

### DL/UL MU Schedulers
We will provide pluggable `IEhtDlScheduler` and `IEhtUlScheduler` interfaces. The base implementations will reuse the HE schedulers via subclassing:
- **`EhtDlSchedulerBase`** (Subclasses `HeDlSchedulerBase`): 
  - Overrides the RU allocation logic to use `Ieee80211EhtMruAllocation`, allowing combinations like 996+484-tone MRUs per user across 320 MHz tone plans.
  - Formulates the `Ieee80211EhtPhyHeader` with U-SIG and EHT-SIG fields instead of the HE counterpart.
- **`EhtUlSchedulerBase`** (Subclasses `HeUlSchedulerBase`): 
  - Formulates EHT Trigger frames containing EHT MRU allocations for uplink OFDMA.
  - Handles the reception parameters for expected EHT TB PPDUs.

---

## 6. Verification Plan

### Automated Verification
- Verify standards pipeline status: `bin/inet_process_standards status`
- Build INET using the recommended workflow (with `CCACHE_DISABLE=1`).
- Run EHT and MLO unit tests:
  ```bash
  bin/inet_run_unit_tests -m release -f "(Ieee80211Eht|Eht|Mlo|Ieee80211He).*\\.test"
  ```

### Scope & Assumptions
- Target is AP/STA infrastructure only. Ad hoc and mesh EHT/MLO are out of scope.
- EHT is enabled for 5/6 GHz only; 2.4 GHz remains non-EHT.
- PHY remains packet-level like the existing HE model.
- Existing `ax` (HE) behavior and tests must remain unchanged.
