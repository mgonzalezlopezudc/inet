# Implementation Plan: 802.11 Compliance Enhancements

This document outlines the detailed plan to address:
1.  **Step 1:** Fix the Python test runner (.omnetpp directory conflict).
2.  **Step 3:** Implement OBSS Spatial Reuse (BSS Color CCA Tuning).
3.  **Step 4:** Implement Multi-TID Block Ack request and response.

---

## User Review Required

> [!IMPORTANT]
> The OBSS PD threshold is currently modeled statically at the physical layer (`Ieee80211Receiver`). In hardware, this threshold may dynamically shift depending on the node's transmit power. The default threshold will be set to a parameter-configurable `-62 dBm` following standard IEEE 802.11ax guidelines.

> [!NOTE]
> Since bit-level encoding is not present in INET, Multi-TID Block Ack bitmaps will be represented using standard `uint64_t` structures (mimicking a compressed block ack bitmap of 64 frames), matching the existing representation of Multi-STA Block Acks.

---

## Open Questions
No immediate open questions are blocking. Design assumptions are aligned with standard packet-level abstractions in INET.

---

## Proposed Changes

### Component: Python Simulation Infrastructure (Step 1)

#### [MODIFY] [project.py](file:///home/user/omnetpp_ws/inet/python/inet/simulation/project.py)
*   **Modify `find_simulation_project_from_current_working_directory()`:**
    *   Change:
        ```python
        project_file_name = os.path.join(path, ".omnetpp")
        if os.path.exists(project_file_name):
            with open(project_file_name) as project_file:
        ```
    *   To:
        ```python
        project_file_name = os.path.join(path, ".omnetpp")
        if os.path.isfile(project_file_name):
            with open(project_file_name) as project_file:
        ```
*   **Modify `collect_binary_simulation_distribution_file_paths()`:**
    *   Change the inner helper `append_file_if_exists()`:
        ```python
        def append_file_if_exists(file_name):
            if os.path.exists(file_name):
                file_paths.append(file_name)
        ```
    *   To:
        ```python
        def append_file_if_exists(file_name):
            if os.path.isfile(file_name):
                file_paths.append(file_name)
        ```
        This prevents directory paths (such as the `.omnetpp` settings folder) from being included in copy distributions, preventing script exceptions during packaging.

---

### Component: Physical Layer (Step 3: BSS Color CCA Tuning)

#### [MODIFY] [Ieee80211Receiver.ned](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.ned)
*   Add parameters to enable and configure Spatial Reuse:
    ```ned
    bool enableSpatialReuse = default(false);
    double obssPdThreshold @unit(dBm) = default(-62 dBm);
    ```

#### [MODIFY] [Ieee80211Receiver.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h)
*   Declare fields and override methods:
    ```cpp
    protected:
      bool enableSpatialReuse = false;
      W obssPdThreshold = W(NaN);
    ```

#### [MODIFY] [Ieee80211Receiver.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc)
*   **In `initialize(int stage)`:**
    *   Read parameters:
        ```cpp
        if (stage == INITSTAGE_LOCAL) {
            enableSpatialReuse = par("enableSpatialReuse");
            obssPdThreshold = mW(math::dBmW2mW(par("obssPdThreshold")));
            ...
        }
        ```
*   **In `computeIsReceptionPossible(listening, reception, part)`:**
    *   Apply Spatial Reuse filter check:
        ```cpp
        auto transmission = reception->getTransmission();
        auto heMuPhyHeader = peekHeMuPhyHeader(transmission);
        if (enableSpatialReuse && heMuPhyHeader != nullptr) {
            auto networkInterface = getContainingNicModule(this);
            auto mib = networkInterface ? dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib")) : nullptr;
            if (mib != nullptr && heMuPhyHeader->getBssColor() != 0 && heMuPhyHeader->getBssColor() != mib->heOperation.bssColor) {
                // It's an OBSS frame. Check if the received signal power is below OBSS PD threshold.
                auto analogModel = getAnalogModel();
                auto maxPower = analogModel->computeMaxPower(reception, reception->getStartTime(), reception->getEndTime());
                if (maxPower < obssPdThreshold) {
                    return false; // Below threshold -> treat as ignorable background noise (reception impossible)
                }
            }
        }
        ```
*   **In `computeIsReceptionAttempted(listening, reception, part, interference)`:**
    *   Apply similar check to bypass starting a reception if it is an ignorable OBSS frame.

---

### Component: MAC Frame Structs and Serialization (Step 4: Multi-TID Block Ack)

#### [MODIFY] [Ieee80211Frame.msg](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg)
*   Define standard records for multi-TID requests and responses:
    ```protobuf
    struct Ieee80211MultiTidBlockAckReqRecord
    {
        uint8_t tid;
        uint16_t startingSequenceNumber;
    }
    
    struct Ieee80211MultiTidBlockAckRecord
    {
        uint8_t tid;
        uint16_t startingSequenceNumber;
        uint64_t bitmap;
    }
    ```
*   Update the frame headers:
    ```protobuf
    class Ieee80211MultiTidBlockAckReq extends Ieee80211BlockAckReq
    {
        multiTid = 1;
        compressedBitmap = 1;
        Ieee80211MultiTidBlockAckReqRecord records[];
    }
    
    class Ieee80211MultiTidBlockAck extends Ieee80211BlockAck
    {
        multiTid = 1;
        compressedBitmap = 1;
        Ieee80211MultiTidBlockAckRecord records[];
    }
    ```

#### [MODIFY] [Ieee80211MacHeaderSerializer.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc)
*   **In `Ieee80211MacHeaderSerializer::serialize(...)`:**
    *   *Case `ST_BLOCKACK_REQ` when `multiTid` and `compressedBitmap` are true:*
        Serialize the record count followed by each record (TID and sequence number).
        ```cpp
        auto multiTidReq = dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(chunk);
        stream.writeByte(multiTidReq->getRecordsArraySize());
        for (unsigned int i = 0; i < multiTidReq->getRecordsArraySize(); ++i) {
            const auto& rec = multiTidReq->getRecords(i);
            stream.writeByte(rec.tid);
            stream.writeUint16Be(rec.startingSequenceNumber);
        }
        ```
    *   *Case `ST_BLOCKACK` when `multiTid` and `compressedBitmap` are true:*
        Serialize the record count followed by each record (TID, starting sequence number, and 64-bit bitmap).
        ```cpp
        auto multiTidAck = dynamicPtrCast<const Ieee80211MultiTidBlockAck>(chunk);
        stream.writeByte(multiTidAck->getRecordsArraySize());
        for (unsigned int i = 0; i < multiTidAck->getRecordsArraySize(); ++i) {
            const auto& rec = multiTidAck->getRecords(i);
            stream.writeByte(rec.tid);
            stream.writeUint16Be(rec.startingSequenceNumber);
            stream.writeUint64Be(rec.bitmap);
        }
        ```
*   **In `Ieee80211MacHeaderSerializer::deserialize(...)`:**
    *   Parse corresponding structures back from the stream into `Ieee80211MultiTidBlockAckReq` and `Ieee80211MultiTidBlockAck` frames.

---

## Verification Plan

### Automated Tests
*   Run the compiled unit tests for the HE scheduling and physical layers to verify no regressions:
    ```sh
    bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
    ```
*   Create two new unit tests under `tests/unit/`:
    1.  `Ieee80211HeSpatialReuse_1.test`: Verifies that a receiver with spatial reuse enabled correctly ignores OBSS transmissions under the OBSS PD threshold.
    2.  `Ieee80211MultiTidBlockAck_1.test`: Verifies correct serialization/deserialization of Multi-TID BAR and BA frames, and validates that the records array is correctly packed.

### Manual Verification
*   Run the example `he_features` configurations inside Qtenv to visually inspect that timing, packet extension, and capability parameters are parsed correctly, and that no runtime warnings or errors are thrown.
