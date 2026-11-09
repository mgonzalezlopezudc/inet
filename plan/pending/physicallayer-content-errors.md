# Implementation plan: error decisions and corruption in packet-level radio reception

> **Kind:** procedure · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [implementation plan procedure](../../doc/project/guide/write-an-implementation-plan.md), [IEEE 802.11 rules](../../doc/project/domain/ieee80211.md), [Packet design](../../doc/project/design/packet-anatomy.md)

Checkout and revision: worktree `inet-physicallayer-content-errors`, branch `topic/physicallayer-content-errors`, base `master` at `6a6bd60bd0`. The checkout has no other local changes.

Plan revision: draft 3, 2026-11-09. Draft 2 (2026-11-05) gave errors in the content only to the `chunk` corruption mode, and its decisions kept the received content. A first implementation of draft 2 existed on this branch. In the review of 2026-11-09, the maintainer asked for one rule for all four corruption modes: the decisions sample the errors first and store them, and the result changes the representation later, as the stored errors tell. Draft 3 makes that change, and the series of section 6 replaces the draft 2 implementation. On the same day, the maintainer asked for the order of section 6, in which no commit adds code that a later commit changes again (D16). Section 8 lists the changed decisions.

Approval: the maintainer approved draft 2 on 2026-11-05. For draft 3, the maintainer agreed to D1 (the packet mode follows the uniform rule), D10 (incorrect flags in the byte and bit modes), and D11 (the corruption of bytes and bits follows the region rate) on 2026-11-09. The maintainer then approved draft 3 on 2026-11-09, and added the two speed changes of section 4.8 (D14), the HT and VHT repair (D15), and the order of the series (D16) on the same day.

## 1. Problem and intended behavior

The four corruption modes of a flat receiver do not follow one rule. They draw at different times, and they keep their outcomes in different objects:

| Mode | When the draws occur | What the decision keeps | When the representation changes |
| --- | --- | --- | --- |
| `packet` | one draw at the decision of each part that the radio asks | a success flag | at the result: a copy with a packet-level bit error |
| `chunk` | one draw at the decision; at the result, one draw for each chunk, again and again until some error occurs | a success flag | at the result: incorrect copies of the chunks with errors |
| `byte`, `bit` | one draw at the decision; at the result, one draw for each byte or bit of the packet, again and again until some error occurs | a success flag | at the result: a raw packet with flipped values and no incorrect flag |

The chunk, byte, and bit modes sample two times: the decision and the corruption are two independent samples. All modes use the SNIR of the whole reception.

IEEE 802.11 also needs errors in place. In this hypothetical case, one HT PPDU carries an A-MPDU with four MPDUs, and a hidden station starts to transmit during the last MPDU. A real receiver keeps the first three MPDUs. INET uses one SNIR for the whole reception, so all four MPDUs fail together or succeed together. [PR #1363](https://github.com/inet-framework/inet/pull/1363) needs the partial result for its A-MPDU work.

The intended behavior follows one rule in all modes:

- **Region:** the unit of one decision. In the `packet` mode, a region is a whole signal part. In the `chunk`, `byte`, and `bit` modes, a region is a chunk of a part. A part without packet content is one region.
- **Error decision:** the decision of each part (HEADER, DATA) decides each of its regions one time. A region fails if its SNIR is not above the threshold of the receiver, or else if one draw against its error rate fails. Both use the SNIR of the time of that region. The decision stores the failed regions with their rates.
- **Corruption:** the result applies the stored failed regions to the representation of the mode. It makes no new decision. The `packet` mode gives a packet-level bit error. The `chunk` mode gives an incorrect copy of each failed chunk. The `byte` and `bit` modes flip bytes or bits in each failed chunk, as its rate gives them, and mark the chunk incorrect.

In the case above, the chunks of the first three MPDUs keep their high SNIR and stay correct, and the chunks of the fourth MPDU fail.

## 2. Current implementation and evidence

The facts below come from the source of master before this branch.

| Source | Current behavior | Consequence |
| --- | --- | --- |
| [FlatReceiverBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.cc) `computeIsReceptionSuccessful()` | Applies the SNIR threshold of `SnirReceiverBase`. Then it draws one number against `computePacketErrorRate(snir, part)` with `dblrand()` of the receiver. A rate of 0 or 1 draws no number. | The decision comes before the content. |
| [FlatReceiverBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.cc) `computeReceivedPacket()` | A success duplicates the transmitted packet. A failure calls `errorModel->computeCorruptedPacket(snir)`. It adds an `ErrorRateInd` with the rates of the WHOLE part. | The content follows the decision. |
| [ErrorModelBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/ErrorModelBase.cc) `computeCorruptedPacket()`, `corruptBytes()`, `corruptBits()`, `hasProbabilisticError()` | Applies `corruptionMode` with the BER of the WHOLE part. The byte and bit modes call `peekAllAsBytes()` or `peekAllAsBits()` and draw for each unit with `dblrand()` of the error model. A `while (!isCorrupted)` loop repeats the sampling until some error occurs; its TODO comment says that the decision is already made. The result has no incorrect flag. | The corruption is a second sample. A declared checksum does not find the flipped values. |
| [ErrorModelBase.h](../../src/inet/physicallayer/wireless/common/base/packetlevel/ErrorModelBase.h) | `enum class CorruptionMode` with `CM_UNDEFINED`, `CM_PACKET`, `CM_CHUNK`, `CM_BYTE`, `CM_BIT` belongs to `ErrorModelBase`. | A receiver cannot read the mode through `IErrorModel`. |
| [Ieee80211ErrorModelBase.cc](../../src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.cc) | `computeCorruptedPacket()` refuses every mode except `packet`. NIST and Yans return NaN from `computeBitErrorRate()`. | IEEE 802.11 has only whole-packet errors. |
| [ErrorModelBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/ErrorModelBase.cc) `getScalarSnir()`, [ISnir.h](../../src/inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h) | Only the SNIR of the whole reception exists. `ScalarSnir` and `DimensionalSnir` can evaluate an interval. | C5 adds the interval to the contract. |
| [SnirReceiverBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/SnirReceiverBase.cc) | Compares the SNIR of the whole reception with `snirThreshold` for every part. | A short dip anywhere fails every part. |
| [RadioMedium.cc](../../src/inet/physicallayer/wireless/common/medium/RadioMedium.cc), [Radio.cc](../../src/inet/physicallayer/wireless/common/radio/packetlevel/Radio.cc) | `computeReceptionResult()` uses only the WHOLE decision. `isReceptionSuccessful()` computes a decision without the cache (C6 repairs it). With separate parts, the cached SNIR misses later interferers (C2 repairs it). | The decisions of separate parts must share one sample, with the current SNIR. |
| [ReceiverBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/ReceiverBase.cc) `computeIsReceptionAttempted()` | The result depends on the receptions in progress at the time of the query. | A part decision that WHOLE computes later can have another attempted flag. The chain of the parts must not read the attempted flags. |
| [CommunicationCacheBase.cc](../../src/inet/physicallayer/wireless/common/base/packetlevel/CommunicationCacheBase.cc) | Keeps one decision for each radio, transmission, and part, and one result for each reception. | A part decision can keep the errors of its part until the result. |
| [Ieee80211NistErrorModel.cc](../../src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.cc), [Ieee80211YansErrorModel.cc](../../src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc) | `getDataSuccessRate()` adds the 16 SERVICE bits for non-HT OFDM modes (C7 moves them). | A region counts only its own bits. |
| [Ieee80211Radio.cc](../../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc), [ApskRadio.cc](../../src/inet/physicallayer/wireless/apsk/packetlevel/ApskRadio.cc), IEEE 802.15.4 | The IEEE 802.11 packet holds the PHY header chunk, the PSDU, and a tail and pad chunk. The APSK packet holds an `ApskPhyHeader`, the payload, and padding. The IEEE 802.15.4 packet holds no header chunk. No shared interface gives these ranges. | Each flat receiver names the range of its parts. |
| [Rx.cc](../../src/inet/linklayer/ieee80211/mac/Rx.cc) `isFcsOk()`, [Packet.h](../../src/inet/common/packet/Packet.h) | Rx drops a frame if `hasBitError()` is true or the content is incorrect. `Packet::hasBitError()` includes the incorrect flags of the content. | A single MPDU needs no MAC change. |
| Packet API (sealed) | Master `7ec0d4d3f3` keeps the flags of chunks in parts, conversions, and merges. | An ordinary insertion keeps the flags of the regions. |
| Configurations | [examples/wireless/corruptionmode](../../examples/wireless/corruptionmode/omnetpp.ini) runs the four modes with `ApskRadio`. Most wireless configurations use the default `packet` mode. [showcases/wireless/throughput](../../showcases/wireless/throughput/omnetpp.ini) uses `separateReceptionParts`. | The change of the packet mode moves the baselines of every wireless configuration with errors. |

IEEE Std 802.11-2024 gives these requirements for its first user:

- 19.3.21 and Figure 19-27: an HT-SIG CRC failure gives PHY-RXEND.indication(FormatViolation). After a valid header, the PHY decodes the symbols until the end of the PSDU.
- 8.3.5.15.2: the RXERROR values contain no error for the content of the PSDU. The MAC finds such errors.
- 9.7.1, 9.7.2, and Annex O.2: each A-MPDU subframe has a delimiter with a CRC, each MPDU has its own FCS, and after an invalid delimiter the receiver moves forward 4 octets.
- 10.3.2.3.7: "EIFS shall not be invoked for an A-MPDU if one or more of its frames are received correctly."

Thus the PHY must give the PSDU with its errors in place, and a header failure stays a failure of the whole reception.

## 3. Scope and acceptance criteria

1. **A1, one decision rule:** in all four modes, the decision of the HEADER part and the decision of the DATA part each decide their regions one time. A region fails if its SNIR is not above the threshold, or else if one draw against its error rate fails. The threshold check is a condition, not a probability, and draws no number. The decision stores the failed regions with their rates. The WHOLE decision combines the part decisions and draws nothing.
2. **A2, regions:** the `packet` mode has one region for each part. The `chunk`, `byte`, and `bit` modes have one region for each chunk of the part. A part without packet content is one region.
3. **A3, time and SNIR:** each region uses the SNIR of its own time interval, with `snirMode`, `snirOffset`, and `snirThresholdMode`.
4. **A4, header rule:** if the header has a failed region, or the packet does not hold the complete range of a part, the DATA part draws nothing, and the received packet has a packet-level bit error, in all modes.
5. **A5, corruption:** the result applies the stored failed regions without a new decision. `packet`: a packet-level bit error. `chunk`: an incorrect copy of each failed chunk. `byte`, `bit`: each failed chunk becomes a raw chunk with flipped units, marked incorrect. The flipped units follow the rate of the region, and at least one unit is flipped. The other chunks stay unchanged, and the transmitted packet stays unchanged.
6. **A6, draws:** the decisions draw in the generator of the receiver, at most once for each region. The byte and bit corruption draws in the generator of the error model, at most one draw more than the number of flipped units of each failed region. Later queries of the same decision draw nothing.
7. **A7, separate parts:** with `separateReceptionParts = true`, the decisions occur at the ends of their parts and make the same draws. Their SNIR includes the interferers that started before the decision.
8. **A8, IEEE 802.11:** the HEADER range is the PHY header chunk, and the DATA range is the PSDU that the length field gives. All four modes work with the NIST, Yans, and BER table models.
9. **A9, APSK and IEEE 802.15.4:** `ApskReceiver` and `Ieee802154NarrowbandReceiver` use the same rule.
10. **A10, no error model:** a flat receiver without an error model keeps its current behavior.
11. **A11, baselines:** each commit updates the fingerprints that it moves and names the statistics baselines that it moves. The packet mode moves the baselines of every wireless configuration with errors.
12. **A12, scope:** the series changes no MAC code except C1, and no file in `common/packet/`.

These items are follow-up work:

- **F1:** A-MPDU member acceptance, the EIFS rule for A-MPDUs, the NAV from members, and Block Ack. PR #1363 owns them.
- **F2:** HT-SIG fields and the HT-SIG CRC check. PR #1363 owns them.
- **F3:** smaller regions for raw content. With `sendRawBytes`, each part is one chunk and thus one region.
- **F4:** a symbol-exact time map for IEEE 802.11, layered receivers, correlated FEC errors, and errors in the SERVICE, tail, and pad bits (D6).
- **F5:** the whole-part rate of `StochasticErrorModel` reads a bit model that flat packet-level transmissions do not have, when `packetErrorRate` is not set.

## 4. Proposed design and reasons

### 4.1 Responsibilities

| Component | Responsibility |
| --- | --- |
| `ISnir` | The SNIR of a time interval (C5) |
| Error model | The rate of a region (C7), its corruption mode, the representation of the stored errors (`computeCorruptedPacket()`), and the draws of the byte and bit corruption |
| `FlatReceiverBase` | The regions of each part, their time, the threshold, and the decision draws; the order of the parts; the header rule |
| A concrete flat receiver | The packet range of its HEADER and DATA parts (`computePartRange()`) |
| `RadioMedium` | The cache: one decision for each part with its errors, and one result for each reception; the interference, noise, and SNIR, which a new overlapping transmission removes (C2) |
| `ReceptionDecision` | The flags of a part decision and the errors of its part |
| MAC | Its current checks of the content |

A PHY can refine the generic behavior at three points: the range of a part, the time map of a region, and the threshold check of a region. The series needs only the first point for IEEE 802.11.

### 4.2 Data structures

```cpp
// A region of the transmitted packet that the decision found erroneous.
struct ErrorRegion
{
    b offset;                         // in the transmitted packet
    b length;
    double errorRate;                 // the region rate of the error model, at the SNIR of the region
};

// The error decision of one signal part (HEADER or DATA).
struct PartErrors
{
    bool isDecoded = false;           // the prerequisites held, and the packet holds the complete range of the part
    bool hasErrors = false;           // at least one region failed, also a region without packet content
    std::vector<ErrorRegion> regions; // the failed regions that the packet holds, in packet order
};
```

`ReceptionDecision` keeps the `PartErrors` of its part:

```cpp
ReceptionDecision(const IReception *reception, IRadioSignal::SignalPart part, bool isReceptionPossible,
        bool isReceptionAttempted, bool isReceptionSuccessful, const PartErrors& errors = PartErrors());
const PartErrors& getErrors() const;
```

### 4.3 Interfaces

```cpp
class INET_API IErrorModel : public virtual IPrintableObject
{
  public:
    enum class CorruptionMode { CM_UNDEFINED = -1, CM_PACKET, CM_CHUNK, CM_BYTE, CM_BIT };   // moved from ErrorModelBase

    virtual CorruptionMode getCorruptionMode() const = 0;

    // unchanged, also for ErrorRateInd
    virtual double computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const = 0;
    virtual double computeBitErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const = 0;
    virtual double computeSymbolErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const = 0;

    // the rate of a region (C7)
    virtual double computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part,
            b length, simtime_t startTime, simtime_t endTime) const = 0;

    // the representation: applies the stored failed regions; replaces computeCorruptedPacket(snir)
    virtual Packet *computeCorruptedPacket(const Packet *packet, const std::vector<ErrorRegion>& regions) const = 0;
};
```

`FlatReceiverBase`, protected:

```cpp
virtual bool computePartRange(const ITransmission *transmission, IRadioSignal::SignalPart part, b& offset, b& length) const;
virtual void computeRegionInterval(const IReception *reception, IRadioSignal::SignalPart part,
        b offset, b length, b partLength, simtime_t& startTime, simtime_t& endTime) const;
virtual PartErrors computePartErrors(const IReception *reception, const ISnir *snir, IRadioSignal::SignalPart part) const;
const PartErrors& getPartErrors(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const;
```

`SnirReceiverBase`, protected: `computeIsSnirAboveThreshold(snir, startTime, endTime)` checks the threshold for a time interval.

`ErrorModelBase`, protected: `computeCorruptedChunk(packet, region)` gives the corrupted form of one failed region in the `chunk`, `byte`, and `bit` modes. `corruptUnits()` flips the units of one failed region.

These functions go away:

- `IErrorModel`: `computeCorruptedPacket(snir)`.
- `ErrorModelBase`: `corruptPacket()`, `corruptChunks()`, `corruptBytes()`, `corruptBits()`, `computeCorruptedPacket(packet, ber)`, `hasProbabilisticError()`, and the retry loop.
- `FlatReceiverBase`: the error-model draw of `computeIsReceptionSuccessful()`.
- `Ieee80211ErrorModelBase`: the refusal of the non-packet modes.

### 4.4 The reception steps

The example is an IEEE 802.11 PPDU with the PHY header chunk H, the PSDU chunks D1, D2, D3, and the tail and pad chunk T. The steps are the same in all modes. Only the regions of 3b and 3c and the corruption of step 4 depend on the mode.

| Step | When | Component | Action | Stored |
| --- | --- | --- | --- | --- |
| 1 | the transmission starts | `RadioMedium::addTransmission()` | Computes the arrivals, and removes the cached interference, noise, and SNIR of the overlapped receptions (C2). | arrival, listening |
| 2 | the arrival starts | `Radio::startReception()` | Decides whether to attempt the reception. | nothing |
| 3 | the arrival ends | `Radio::endReception()` → `RadioMedium::getReceptionDecision(WHOLE)` | Computes the interference, noise, and SNIR, and asks the receiver for the WHOLE decision. | interference, noise, SNIR |
| 3a | same time | `FlatReceiverBase` | Checks the SNIR of the preamble interval against the threshold. No draw (D7). | — |
| 3b | same time | `FlatReceiverBase`, HEADER decision | Decides the regions of the header. | HEADER decision with its `PartErrors` |
| 3c | same time | `FlatReceiverBase`, DATA decision | Only if the header has no failed region: decides the regions of the PSDU. | DATA decision with its `PartErrors` |
| 3d | same time | `FlatReceiverBase`, WHOLE decision | Combines the two decisions and applies the header rule. Keeps the failed regions of the DATA part. No draw. | WHOLE decision with the failed DATA regions |
| 4 | same time | `RadioMedium::receivePacket()` → `FlatReceiverBase::computeReceivedPacket(snir, decisions)` | Gives the failed regions of the WHOLE decision that it receives to `computeCorruptedPacket()`. Adds the tags. | the result, until `receivePacket()` copies it |
| 5 | same time | `Radio`, then the MAC | Decapsulates and sends up. The MAC checks `hasBitError()`. | — |

| Mode | 3b, 3c: regions and draws (receiver) | 4: corruption |
| --- | --- | --- |
| `packet` | one region for each part (H; D1+D2+D3): the threshold, then one draw | a copy with a packet-level bit error; no draws |
| `chunk` | one region for each chunk (H; D1, D2, D3): the threshold, then one draw each | an incorrect copy of each failed chunk; no draws |
| `byte`, `bit` | the same as `chunk` | each failed chunk becomes a raw chunk with flipped units, marked incorrect; draws in the error model |

With separate parts, the radio asks for the HEADER decision at the end of the header and for the DATA decision at the end of the data. The draws stay the same.

### 4.5 Pseudo code

```cpp
const IReceptionDecision *FlatReceiverBase::computeReceptionDecision(listening, reception, part, interference, snir) const
{
    if (errorModel == nullptr)
        return NarrowbandReceiverBase::computeReceptionDecision(...);         // unchanged (A10)
    possible = ...; attempted = ...;
    PartErrors errors;
    bool received;
    switch (part) {
        case PREAMBLE:
            received = isPreambleAboveThreshold(reception, snir);
            break;
        case HEADER:
            if (isPreambleAboveThreshold(reception, snir))
                errors = computePartErrors(reception, snir, HEADER);
            received = errors.isDecoded && !errors.hasErrors;
            break;
        case DATA: {
            const auto& header = getPartErrors(listening, reception, HEADER);   // from the cache
            if (header.isDecoded && !header.hasErrors)
                errors = computePartErrors(reception, snir, DATA);
            received = errors.isDecoded && !errors.hasErrors;
            break;
        }
        case WHOLE: {                                                        // keeps the regions that the result applies
            const auto& header = getPartErrors(listening, reception, HEADER);
            const auto& data = getPartErrors(listening, reception, DATA);
            errors.isDecoded = header.isDecoded && !header.hasErrors && data.isDecoded;   // the header rule (A4)
            errors.hasErrors = !errors.isDecoded || data.hasErrors;
            if (errors.isDecoded)
                errors.regions = data.regions;
            received = !errors.hasErrors;
            break;
        }
    }
    return new ReceptionDecision(reception, part, possible, attempted, attempted && received, errors);
}

PartErrors FlatReceiverBase::computePartErrors(reception, snir, part) const
{
    PartErrors errors;
    if (!computePartRange(transmission, part, partOffset, partLength))
        return errors;                                                        // not decoded
    errors.isDecoded = true;
    // the whole part in the packet mode, else the chunks of the part; a part without packet content is one region
    for (auto [offset, length] : regions(transmission, partOffset, partLength, errorModel->getCorruptionMode())) {
        computeRegionInterval(reception, part, offset - partOffset, length, partLength, startTime, endTime);
        double rate = errorModel->computePacketErrorRate(snir, part, length, startTime, endTime);
        bool failed;
        if (!computeIsSnirAboveThreshold(snir, startTime, endTime))
            failed = true;                                                    // a condition, no draw
        else
            failed = rate == 1 || (rate != 0 && !(dblrand() > rate));         // one draw
        if (failed) {
            errors.hasErrors = true;
            if (partOffset != b(-1))
                errors.regions.push_back({offset, length, rate});
        }
    }
    return errors;
}

// a new overload of ReceiverBase, which computeReceptionResult() calls with the decisions that it receives
Packet *FlatReceiverBase::computeReceivedPacket(snir, decisions) const
{
    const auto& regions = the errors of the WHOLE decision in decisions;
    if (regions.empty())                                                     // also without an error model
        return ReceiverBase::computeReceivedPacket(snir, decisions);         // the transmitted packet, with a bit error if a decision failed
    packet = errorModel->computeCorruptedPacket(transmittedPacket, regions);
    clear the tags and add the protocol tag, as ReceiverBase::computeReceivedPacket() does;
    return packet;
}

Packet *ErrorModelBase::computeCorruptedPacket(const Packet *packet, const std::vector<ErrorRegion>& regions) const
{
    // the caller owns the received packet, so the function does not enter the module
    copy = packet->dup();
    if (corruptionMode == CM_PACKET) {                     // a packet-level bit error
        copy->setBitError(true); return copy;
    }
    content = a new SequenceChunk;
    position = b(0);
    for (const auto& region : regions) {                   // the content between the regions stays as it is
        if (region.offset > position)
            content->insertAtBack(packet->peekAt(position, region.offset - position, flags));
        content->insertAtBack(computeCorruptedChunk(packet, region));
        position = region.offset + region.length;
    }
    if (position < packet->getTotalLength())
        content->insertAtBack(packet->peekAt(position, packet->getTotalLength() - position, flags));
    copy->replaceAll(content, flags);
    return copy;
}

Ptr<const Chunk> ErrorModelBase::computeCorruptedChunk(const Packet *packet, const ErrorRegion& region) const
{
    switch (corruptionMode) {
        case CM_CHUNK:                                     // an incorrect copy, no draws
            chunk = packet->peekAt(region.offset, region.length, flags);
            corrupted = chunk->dupShared(); corrupted->markIncorrect(); return corrupted;
        case CM_BYTE:                                      // the bytes of the region; bits if the length is not whole bytes
        case CM_BIT:
            units = the bytes or bits of packet->peekAt<BytesChunk or BitsChunk>(region.offset, region.length, flags);
            corruptUnits(units.size(), region.errorRate, [&] (int i) { flip units[i]; });
            raw = a new BytesChunk or BitsChunk of units; raw->markIncorrect();
            return raw;
    }
}

// Flips the units of a failed region as its rate gives them. The region failed, so at least one unit
// is flipped: the positions are sampled under that condition, without a retry loop.
void ErrorModelBase::corruptUnits(int n, double errorRate, const std::function<void(int)>& corruptUnit) const
{
    double p = -std::expm1(std::log1p(-errorRate) / n);   // the unit rate: 1 - R = (1 - p)^n
    if (p >= 1)
        corruptUnit(i) for every unit;                     // no draws
    else if (p <= 0)
        corruptUnit(intuniform(0, n - 1));                 // the region failed by the threshold, and its rate is 0
    else {
        // the first error, in [0, n), under the condition of at least one error
        double k = min(floor(log1p(-dblrand() * errorRate) / log1p(-p)), n - 1);
        while (k < n) {
            corruptUnit(k);
            k += 1 + floor(log1p(-dblrand()) / log1p(-p)); // the geometric gap to the next error
        }
    }
}
```

The unit rate comes from the region rate, so the decision of the region and its corruption always agree. The sampling is exact for independent unit errors. It replaces the retry loop of master. For a rate between 0 and 1, it draws one number more than the number of flipped units.

### 4.6 Region error rates of the models (C7)

| Model | HEADER region | DATA region |
| --- | --- | --- |
| `StochasticErrorModel` | With `packetErrorRate`: 1 − (1 − PER)^(region duration / reception duration). Otherwise: 1 − (1 − BER)^length. | Same |
| `ApskErrorModel`, `Ieee802154ErrorModel` | 1 − (1 − BER)^length, with the BER of the modulation at the region SNIR | Same |
| `Ieee80211ErrorModelBase` (NIST, Yans) | 1 − `getHeaderSuccessRate(mode, length, regionSnir)` | 1 − `getDataSuccessRate(mode, length, regionSnir)` |
| `Ieee80211BerTableErrorModel` | 0, as in its whole-part model | The table PER for the region length in bytes at the region SNIR |

The region rate of a PREAMBLE or WHOLE part is a usage error. `getDataSuccessRate()` of NIST and Yans no longer adds the SERVICE bits. `Ieee80211ErrorModelBase::computePacketErrorRate()` adds them for OFDM modes, so the whole-part rates keep their values.

The generic `computeRegionInterval()` divides the time of a part in proportion to the bits. For IEEE 802.11, the DATA part also carries the SERVICE, tail, and pad bits, so the map is at most one OFDM symbol from the real position. The mode objects give the part times, so the map adds no duration formula ([AR-WLAN-PHY-AUTHORITY](../../doc/project/domain/ieee80211.md#ar-wlan-phy-authority)).

### 4.7 Alternatives that this plan does not use

- **The received content in the decision (draft 2):** the decision of each part kept the received chunks of the part. The decision then changes the representation, and the modes do not follow one rule.
- **Draws at the result:** the decision and the content would be two samples that can disagree, as in the byte and bit modes of master.
- **One draw for the whole reception in the packet mode (D1 of draft 2):** the packet mode would stay an exception, and with separate parts it would draw for each asked part. The maintainer chose the uniform rule and the baseline update.
- **Units as regions in the byte and bit modes:** a decision for each byte or bit would need a threshold check and an SNIR for each unit. The region is the chunk, and the corruption samples the units.
- **Random units after a threshold failure:** the flips would not follow the error rate (D11).
- **The approach of PR #1363:** only HT aggregates get chunk errors, with the SNIR of the whole reception.

### 4.8 Speed

This section is an estimate from the code. Nothing was measured.

| Mode | Work for each received frame | Expected effect |
| --- | --- | --- |
| `packet` | Master: one SNIR value of the whole reception, which the threshold and the rate share, and one rate. Draft 3: about five interval SNIR values (the preamble, and the threshold and the rate of each part), two rates, up to two draws, and two more decisions in the cache. | Small with the scalar model, because an interval value is a maximum over a noise function with few steps. Visible with the dimensional model, because each interval value partitions the SNIR function. |
| `chunk` | Two interval values and one rate for each chunk. A frame without errors costs only a copy at the result. | Small with the scalar model; with many chunks and the dimensional model, visible |
| `byte`, `bit` | Master serializes the whole packet, draws once for each unit, and repeats this until some error occurs, on average 1 / (1 − (1 − BER)^n) times. Draft 3 serializes only the failed chunks, and draws once for each flip, plus one. | Much faster. A small BER after a threshold failure no longer almost stops the simulation. |

Two changes remove most of the extra cost (D14). Neither change moves a result:

- **C5:** `ScalarSnir` and `DimensionalSnir` keep the values of the last interval query. The threshold and the rate of a region ask for the same interval, and both `snirThresholdMode` and `snirMode` are `"min"` by default. Thus the second query uses the kept value.
- **C2:** the removal of the cached SNIR in `RadioMedium::addTransmission()` goes through all transmissions of the cache for each radio. In the default mode, it never finds a cached SNIR, but it still costs one lookup for each radio and live transmission. The loop thus asks `ICommunicationCache::computeInterferingTransmissions(radio, startTime, endTime)`. In `CommunicationCacheBase`, that function uses the interval tree of the radio. The loop keeps its strict overlap check. `setCachedInterval()` already put the new transmission into the tree, so the loop still skips it.

## 5. Affected components and compatibility

**Simulation results:** the packet mode now decides the header and the data separately, each with the SNIR of its own time. Thus the fingerprints and statistics of every wireless configuration with errors change (C10). The `chunk`, `byte`, and `bit` modes change their results too (C10). C6 changes the scalar "reception decision computation count" in configurations with separate parts. The statistics repository needs updates for C6 and C10 at the landing.

**C++ API:** `ISnir` and `IErrorModel` get pure virtual functions, and `IErrorModel::computeCorruptedPacket()` changes its parameters. An implementation outside INET must follow. `CorruptionMode` moves to `IErrorModel`. `ReceptionDecision` gets a constructor parameter. The functions of section 4.3 go away.

**Configuration:** `corruptionMode` keeps its values and its default. IEEE 802.11 now supports all four values.

**Documentation:** each changing commit carries its release note under `doc/whatsnew/`. The migration guide section `doc/src/migration-guide/development/physicallayer-content-errors.rst` describes the interface changes. The users guide section "Error Models" in [ch-physicallayer.rst](../../doc/src/users-guide/ch-physicallayer.rst) describes the rule of section 1.

**PR #1363:** after this series, the PR can drop its commit `4d9ddb1fe6` and the generic part of `eb7e4157e3`, and set `corruptionMode = "chunk"`.

## 6. Future commits and dependencies

The four repairs come first, because they do not depend on the rest. Then three commits prepare the SNIR, the decision cache, and the region rates. Two more commits prepare the receiver side and the error model side of the region decisions without a caller. One commit then moves the behavior of all four modes and removes the old path, so that no commit adds code that a later commit of the series changes again (D16). Each commit marks its entry done when it is complete.

| Commit and proposed subject | Dependencies |
| --- | --- |
| `C0: plan: add: define error decisions and corruption for packet-level reception` | none |
| `C1: AckingMac: fix: check the bit error before the header is read` | C0 |
| `C2: physicallayer: fix: compute the SNIR of a reception again after a new interferer` | C0 |
| `C3: physicallayer: fix: initialize the base of the stochastic and BER table error models` | C0 |
| `C4: ieee80211: fix: do not count the signal field of HT and VHT in the preamble` | C0 |
| `C5: physicallayer: add: SNIR of a time interval` | C0 |
| `C6: physicallayer: change: compute each part decision once` | C0 |
| `C7: physicallayer: add: error rates of signal regions` | C5 |
| `C8: physicallayer: add: packet ranges and region times of signal parts` | C0 |
| `C9: physicallayer: add: corruption of failed signal regions` | C0 |
| `C10: physicallayer: change: decide the errors of each region before the corruption` | C1 to C9 |
| `C11: plan: change: record the results of the error decision series` | C10 |
| `C12: plan: location: move the error decision plan to done` | C11 |

These commits change the model of the physical layer, not a protocol exchange, so protocol tests and protocol reviewer documents do not apply. Unit tests and module tests check the mechanism at the PHY boundary ([AR-WLAN-QUAL-TESTS](../../doc/project/domain/ieee80211.md#ar-wlan-qual-tests)).

### C0

C0 adds this plan. It changes no source.

### C1

Status: open. Purpose and scope: `AckingMac::handleLowerPacket()` reads its MAC header with `peekAtFront()` before it checks `hasBitError()`. If the header chunk is incorrect, the peek stops the simulation with "Returning an incorrect chunk is not allowed". The commit moves the check above the read. The other wireless MACs already check first.

Verification: the AckingMac module tests, and the run of the C10 module test in which the MAC header chunk is incorrect.

### C2

Status: open. Purpose and scope: with `separateReceptionParts = true`, the radio medium caches the interference, noise, and SNIR of a reception at its first decision. Nothing removes them, so a transmission that starts later does not count for the rest of the reception, also in the packet mode. `RadioMedium::addTransmission()` now deletes the cached interference, noise, and SNIR of each reception that the new transmission overlaps at a radio. It asks the interval tree of the radio for these receptions (section 4.8). The decisions made so far stay, because they were correct at their time. In the default mode, nothing is cached at that time, so the commit changes no result there.

Verification: the run of the C10 module test with separate parts and a late interferer, the `ConcurrentRadio` tests, and the fingerprints.

### C3

Status: open. Purpose and scope: `StochasticErrorModel::initialize()` and `Ieee80211BerTableErrorModel::initialize()` do not call `ErrorModelBase::initialize()`, so `corruptionMode`, `snirMode`, and `snirOffset` stay undefined. The commit adds the call. The whole-part rate of the BER table model reads the data length from the bit model of the transmission, and a packet-level IEEE 802.11 transmission has none. The model now reads the PSDU length from the length field of the PHY header, as `Ieee80211ErrorModelBase` does. No example, showcase, or test uses these two models now. The open [PR #1431](https://github.com/inet-framework/inet/pull/1431) repairs the same initialization of `StochasticErrorModel`; if it lands first, this commit keeps only the part of the BER table model.

Verification: the C10 module test runs the BER table model.

### C4

Status: open. The maintainer added this commit on 2026-11-09 (D15), after a full fingerprint run of the packet mode found the defect: `examples/wireless/lan80211ac -c Ping1` stopped with "Invalid lower or upper arguments", because a DATA region interval had its start after its end. `Ieee80211HtPreambleMode::getDuration()` and `Ieee80211VhtPreambleMode::getDuration()` include HT-SIG or VHT-SIG-A, and the header mode of `Ieee80211HtMode` and `Ieee80211VhtMode` is that same field. `Ieee80211Transmitter` computes the data duration as the total minus the preamble and the header. The DATA part of every HT and VHT transmission is thus 8 µs too short, and a short VHT frame has a negative data duration; the ARP request of that configuration has 72 µs in total and −4 µs of data. The preamble modes no longer contain the signal field, and `Ieee80211HtMode::getDuration()` and `Ieee80211VhtMode::getDuration()` add the header duration, as the other modes do. The total durations and `getPreambleLength()` stay the same.

Verification: the IEEE 802.11 module tests, and the full fingerprint run, which may move only configurations in which the part times of HT or VHT count: separate reception parts or the dimensional radio.

### C5

Status: open. Purpose and scope: the interval overloads of `ISnir` in `ScalarSnir`, `DimensionalSnir`, and `UnitDiskSnir`. The whole-reception getters call the interval functions with the limits of the reception and keep their cached values. `DimensionalSnir` keeps its SNIR function, so that many interval queries do not divide the functions again. `ScalarSnir` and `DimensionalSnir` also keep the limits of the last interval query, with its minimum, maximum, and mean, each computed when first asked (section 4.8). Release note `doc/whatsnew/incompatible/physicallayer-snir-interval.txt` and the migration guide section.

Verification: the fingerprints must keep all values. A unit test would need a stub transmission and a stub reception, because `ReceptionBase` reads its part durations from a transmission. Thus the module tests of C10 check the interval values.

### C6

Status: open. Purpose and scope: `RadioMedium::isReceptionSuccessful()` returns the result of `getReceptionDecision()`, so that the decisions of separate parts come from the cache. Remove the TODO in `Radio::endReception()`.

Verification: the radio module tests and the fingerprints keep their results. The scalar "reception decision computation count" increases in configurations with separate parts, because these decisions now go through the counted function. The statistics repository holds `showcases/wireless/throughput/omnetpp.ini-General-#0.sca` with this scalar (9876 on master), which needs an update at the landing.

### C7

Status: open. Purpose and scope: the region overload of `IErrorModel::computePacketErrorRate()`, the interval `ErrorModelBase::getScalarSnir()`, the protected `checkRegionPart()`, the region rates of section 4.6, and the SERVICE move. No receiver calls the region rate yet. Release note `doc/whatsnew/incompatible/physicallayer-region-error-rates.txt` and the migration guide section.

Verification: a new unit test `tests/unit/RegionErrorRates_1.test` uses a small subclass of `StochasticErrorModel` with a fixed BER, without a network. It checks that the region rate is 1 − (1 − BER)^length, that the limits of the BER give 0 and 1, and that the PREAMBLE and WHOLE parts have no regions. The fingerprints must keep all values; this checks the SERVICE move.

### C8

Status: open. Purpose and scope: the receiver side of the region decisions, without a caller. `ErrorRegion` and `PartErrors` in `IErrorModel.h`, and the errors of `ReceptionDecision` with `getErrors()`. `FlatReceiverBase::computePartRange()`, which refuses by default, and `computeRegionInterval()`. The part ranges of `ApskReceiver` (the header and the data with the padding), `Ieee802154NarrowbandReceiver` (a header that the packet does not hold, and the whole packet as the data), and `Ieee80211Receiver` (the PHY header chunk, and the PSDU that the length field gives; a PSDU longer than the remaining content gives an incomplete range). `SnirReceiverBase::computeIsSnirAboveThreshold()` for a time interval. The overload `ReceiverBase::computeReceivedPacket(snir, decisions)`, which `computeReceptionResult()` calls and which gives the same packet as before. The migration guide section.

Verification: the fingerprints must keep all values. The module tests of C10 check the ranges and the region times.

### C9

Status: open. Purpose and scope: the error model side of the region decisions, without a caller. The enum `CorruptionMode` moves to `IErrorModel`, and `IErrorModel` gets `getCorruptionMode()` and `computeCorruptedPacket(packet, regions)`. `ErrorModelBase` gives a packet-level bit error in the `packet` mode, an incorrect copy of each failed chunk in the `chunk` mode, and flipped bytes or bits in the `byte` and `bit` modes, through the protected `computeCorruptedChunk()` and `corruptUnits()` (sections 4.5 and D11, D13). The old `computeCorruptedPacket(snir)` stays until C10. Release note and the migration guide section.

Verification: a new unit test `tests/unit/RegionCorruption_1.test` checks that `computeCorruptedPacket()` in the `chunk` mode marks exactly the chunks of the given regions, that the content between the regions keeps its bytes and flags, and that the `packet` mode only sets the packet-level bit error. A new module test `tests/module/UnitCorruption_1.test` runs `corruptUnits()` in a small network of two error models and a tester, because the error model draws from the random number generator of its module:

- With the rate 1, every unit flips and nothing is drawn.
- With the rate 0, exactly one unit flips.
- With other rates, each sample flips at least one unit, and the positions stay in the region.
- Over 10000 samples, the mean number of flips is within 2 % of n·p / (1 − (1 − p)^n).
- In the byte and bit modes, a failed region becomes a raw chunk with the flipped units, marked incorrect; a region that is not whole bytes uses bits in the byte mode; the input stays unchanged.

The fingerprints must keep all values.

### C10

Status: open. Purpose and scope: `FlatReceiverBase` decides the regions of each part in all four modes (section 4.5): `computeReceptionDecision()`, `computeIsPreambleReceived()`, `computePartErrors()`, `getPartErrors()`, and the override of `computeReceivedPacket(snir, decisions)`. The WHOLE decision keeps the failed DATA regions, which the received packet gets. Remove the old path: `IErrorModel::computeCorruptedPacket(snir)`; `corruptPacket()`, `corruptChunks()`, `corruptBytes()`, `corruptBits()`, `computeCorruptedPacket(packet, ber)`, `hasProbabilisticError()`, and the retry loop of `ErrorModelBase`; `FlatReceiverBase::computeIsReceptionSuccessful()` with its error-model draw, and the `bool` override of `computeReceivedPacket()`; the refusal of `Ieee80211ErrorModelBase`. Update all fingerprints that move. Release notes, the users guide paragraph, and the migration guide section.

Verification:

- A new module test `tests/module/ContentErrors_ApskScalarRadio_1.test` sends one UDP packet with `ApskScalarRadio` and prints the flag of each chunk of the received frame and the draws of the receiver. Its runs: the packet mode and the chunk mode under late interference, with and without separate parts; interference over the header; an error model that gives the rates 0, 1, and 0.5 to the regions in turn, without and with separate parts; an error model with the rate 0 for the header and 0.5 for the data in the packet mode, without and with separate parts; and the byte and bit modes under late interference.
- A new module test `tests/module/ContentErrors_Ieee80211Radio_1.test` sends a PSDU of three chunks without a MAC, so that no backoff or retry changes the timing. It runs ERP-OFDM, HT, and HR/DSSS modes, the NIST, Yans, and BER table models, the dimensional radio, and the packet, byte, and bit modes. The PSDU bound has no test, because `TapRadioMedium` changes received packets after the decisions and thus cannot make a transmission with a too long length field.
- A new module test `tests/module/ConcurrentRadio_7.test` runs two overlapping receptions in the chunk mode with the dimensional radio.
- The reception tests check that no module deletes a packet that it does not own.
- The full fingerprint run shows which configurations move. Each moved configuration must be a wireless configuration with errors in the reception, or a configuration of `examples/wireless/corruptionmode`.

### C11 and C12

C11 records the executed results, the design facts, and the gaps. C12 moves the plan to `plan/done/` without a content change ([PR-SPLIT-MOVE](../../doc/project/rule/pull-request.md#pr-split-move)).

## 7. Verification methods and expected results

Follow [run-the-gates.md](../../doc/project/guide/run-the-gates.md). Build in this worktree with its own environment, and make sure that `INET_ROOT` points to it. A fresh debug build is necessary after each commit. The proposed commands run from the worktree root:

```sh
make MODE=debug -j8
opp_run_unit_tests -m debug
opp_run_module_tests -m debug
opp_run_protocol_tests -m debug
opp_run_fingerprint_tests -m debug
```

Each commit must build alone. A full fingerprint run follows C9 and C10: before C10, no fingerprint may move, and C10 may move only the configurations that its message names. The protocol suite must give the same unexpected failures as master. On master `6a6bd60bd0`, the protocol suite has 67 unexpected failures, `examples/ipv6/mipv6 -c Handover` fails its fingerprint, and 22 fingerprint configurations need disabled features. Update the moved fingerprints with the procedure of [change-a-baseline.md](../../doc/project/guide/change-a-baseline.md). A release build and `check-cpp.sh` run on C10. No step measures the speed; section 4.8 is an estimate.

## 8. Risks, unresolved decisions, and approval scope

### Decisions

- **D1, the packet mode (changed in draft 3, agreed 2026-11-09):** the packet mode follows the uniform rule: one region for each part, and no draw for the WHOLE decision. Draft 2 kept the single draw for the whole reception. The cost is the update of the baselines of every wireless configuration with errors.
- **D2, a generic API (kept):** the threshold and the rate apply to each region with the SNIR of its time. A PHY can refine the range, the time map, and the threshold check. Draft 3 states that the threshold check is a condition, not a probability.
- **D3, the sealed packet API (done):** the repair landed on master as `7ec0d4d3f3`.
- **D4, the byte and bit modes (changed in draft 3):** they join the uniform rule in C10. Draft 2 kept their old behavior.
- **D5, the random number generators (changed in draft 3):** the decisions draw in the generator of the receiver. The byte and bit corruption draws in the generator of the error model, as master does.
- **D6, the SERVICE, tail, and pad bits (kept):** no region contains them.
- **D7, the preamble (kept):** only the threshold over its interval applies.
- **D8, the SNIR cache with separate parts (answered 2026-11-05):** repair it in this series, as C2.
- **D9, the header check of AckingMac (answered 2026-11-05):** repair it in this series, as C1.
- **D15, the HT and VHT part durations (answered 2026-11-09):** repair them in this series, as C4, before the region decisions.
- **D16, the order of the series (answered 2026-11-09):** the repairs come first, then the prepare commits, and one commit moves the behavior of all four modes. A series that moved the modes one by one needed temporary conditions on the mode, which later commits changed and removed. The cost: the fingerprints of all four modes move in one commit, so a bisect does not separate the packet mode from the other modes.
- **D10, incorrect flags in the byte and bit modes (new, agreed 2026-11-09):** a raw chunk with flipped units is marked incorrect, so that a declared checksum also finds the errors.
- **D11, the units of a failed region (new, agreed 2026-11-09):** the flips follow the rate of the region, and at least one unit is flipped. A threshold failure with the rate 0 flips one unit.
- **D12, errors in the decision (new, approved with draft 3):** a part decision keeps the failed regions of its part, not its received content. The corruption of a region becomes a protected function of `ErrorModelBase`.
- **D13, regions that are not whole bytes (new, approved with draft 3):** in the `byte` mode, a failed region whose length is not a whole number of bytes uses its bits as units.
- **D14, speed (new, agreed 2026-11-09):** C5 keeps the values of the last interval query, and C2 visits only the overlapping receptions (section 4.8). Neither change moves a result.

### Risks

- **R1:** an operation after `decapsulate()` can read a part of a flagged chunk without `PF_ALLOW_INCORRECT`. The module tests run the whole reception path.
- **R2:** the full fingerprint run after C10 moves many configurations. A configuration without errors in the reception must not move; such a movement is a defect.
- **R3:** the statistics baselines of C6 and C10 need an update in the statistics repository at the landing.
- **R4:** the WHOLE decision calls the radio medium during its own computation. `ReceiverBase` already does this for the attempted flag.
- **R5:** in the `byte` and `bit` modes, a chunk type without a serializer stops the simulation at its first failed region. Master has the same limit for the whole packet.

The approval covers draft 3 of this plan and the commits C0 to C12. It does not cover F1 to F5.

## 9. Results

C11 records the results.
