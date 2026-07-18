//
// Copyright (C) 2005 Michael Tuexen
// Copyright (C) 2008 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
// Copyright (C) 2009 Thomas Reschka
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/common/packet/recorder/PcapRecorder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "inet/common/DirectionTag_m.h"
#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/packet/recorder/PcapngWriter.h"
#include "inet/common/packet/recorder/PcapWriter.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/stlutils.h"
#include "inet/common/StringFormat.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/InterfaceTable.h"

#ifdef INET_WITH_IEEE80211
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#endif

#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
#include "inet/physicallayer/common/Signal.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmission.h"
#endif

namespace inet {

namespace {

enum RadiotapPresentBit {
    RADIOTAP_FLAGS = 1,
    RADIOTAP_RATE = 2,
    RADIOTAP_CHANNEL = 3,
    RADIOTAP_ANTENNA_SIGNAL = 5,
    RADIOTAP_DBM_TX_POWER = 10,
    RADIOTAP_RX_FLAGS = 14,
    RADIOTAP_TX_FLAGS = 15,
    RADIOTAP_MCS = 19,
    RADIOTAP_AMPDU = 20,
    RADIOTAP_VHT = 21,
    RADIOTAP_HE = 23,
    RADIOTAP_HE_MU = 24,
    RADIOTAP_0_LENGTH_PSDU = 26,
    RADIOTAP_EHT = 34,
};

enum RadiotapFlags {
    RADIOTAP_F_FCS = 0x10,
    RADIOTAP_F_BADFCS = 0x40,
};

enum RadiotapChannelFlags {
    RADIOTAP_CHANNEL_2GHZ = 0x0080,
    RADIOTAP_CHANNEL_5GHZ = 0x0100,
};

#ifdef INET_WITH_IEEE80211

// See https://www.radiotap.org/fields/HE.html. These are deliberately kept
// local instead of depending on a platform-specific radiotap header.
enum RadiotapHeData1 {
    RADIOTAP_HE_FORMAT_SU = 0,
    RADIOTAP_HE_FORMAT_EXT_SU = 1,
    RADIOTAP_HE_FORMAT_MU = 2,
    RADIOTAP_HE_FORMAT_TRIG = 3,
    RADIOTAP_HE_UL_DL_KNOWN = 0x0010,
    RADIOTAP_HE_DATA_MCS_KNOWN = 0x0020,
    RADIOTAP_HE_DATA_DCM_KNOWN = 0x0040,
    RADIOTAP_HE_CODING_KNOWN = 0x0080,
    RADIOTAP_HE_SPATIAL_REUSE_KNOWN = 0x0400,
    RADIOTAP_HE_MU_STA_ID_KNOWN = 0x0800,
    RADIOTAP_HE_BW_RU_ALLOC_KNOWN = 0x4000,
};

enum RadiotapHeData2 {
    RADIOTAP_HE_GI_KNOWN = 0x0002,
};

uint16_t getRadiotapHeFormat(physicallayer::Ieee80211HePpduFormat format)
{
    switch (format) {
        case physicallayer::HE_MU_DOWNLINK: return RADIOTAP_HE_FORMAT_MU;
        case physicallayer::HE_TRIGGER_BASED_UPLINK: return RADIOTAP_HE_FORMAT_TRIG;
        case physicallayer::HE_SINGLE_USER: return RADIOTAP_HE_FORMAT_SU;
        case physicallayer::HE_EXTENDED_RANGE_SU: return RADIOTAP_HE_FORMAT_EXT_SU;
        default: throw cRuntimeError("Unknown HE PPDU format: %d", (int)format);
    }
}

int getRadiotapHeBandwidth(Hz bandwidth)
{
    auto value = bandwidth.get();
    return value < 30e6 ? 0 : value < 60e6 ? 1 : value < 120e6 ? 2 : 3;
}

int getRadiotapHeRuAllocation(int toneSize)
{
    switch (toneSize) {
        case 26: return 4;
        case 52: return 5;
        case 106: return 6;
        case 242: return 7;
        case 484: return 8;
        case 996: return 9;
        case 1992: return 10;
        default: return -1;
    }
}

#endif

void appendPadding(std::vector<uint8_t>& bytes, size_t alignment)
{
    bytes.resize(bytes.size() + (alignment - bytes.size() % alignment) % alignment, 0);
}

void appendUint16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(value & 0xff);
    bytes.push_back(value >> 8);
}

void appendUint32(std::vector<uint8_t>& bytes, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
        bytes.push_back((value >> (8 * i)) & 0xff);
}

void setUint16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value)
{
    bytes.at(offset) = value & 0xff;
    bytes.at(offset + 1) = value >> 8;
}

void setUint32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
    for (size_t i = 0; i < 4; i++)
        bytes.at(offset + i) = value >> (8 * i);
}

#ifdef INET_WITH_IEEE80211

struct MpduRange
{
    b offset;
    b length;
};

bool getIeee80211AmpduMpduRanges(const Packet *packet, b frontOffset, b backOffset, std::vector<MpduRange>& mpduRanges)
{
    const int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    auto dataLength = packet->getDataLength();
    auto endOffset = dataLength - backOffset;
    if (frontOffset + ieee80211::LENGTH_A_MPDU_SUBFRAME_HEADER > endOffset)
        return false;

    auto peekDelimiter = [&] (b offset) {
        return dynamicPtrCast<const ieee80211::Ieee80211MpduSubframeHeader>(packet->peekDataAt(offset, b(-1), parsingFlags));
    };

    try {
        if (peekDelimiter(frontOffset) == nullptr)
            return false;
        auto offset = frontOffset;
        while (offset < endOffset) {
            if (offset + ieee80211::LENGTH_A_MPDU_SUBFRAME_HEADER > endOffset)
                return false;
            const auto& delimiter = peekDelimiter(offset);
            if (delimiter == nullptr || delimiter->getLength() <= 0)
                return false;
            auto mpduOffset = offset + delimiter->getChunkLength();
            auto mpduLength = B(delimiter->getLength());
            if (mpduOffset + mpduLength > endOffset)
                return false;
            mpduRanges.push_back({mpduOffset, mpduLength});
            offset = mpduOffset + mpduLength;
            if (offset == endOffset)
                return true;
            auto paddingLength = B((4 - (delimiter->getChunkLength() + mpduLength).get<B>() % 4) % 4);
            if (offset + paddingLength >= endOffset)
                return false;
            offset += paddingLength;
        }
    }
    catch (cRuntimeError&) {
        return false;
    }
    return false;
}

#endif

std::vector<uint8_t> makeRadiotapHeader(const Packet *packet, b frontOffset, b backOffset, Direction direction, const physicallayer::ITransmission *transmission, const physicallayer::IReception *reception)
{
    std::vector<uint32_t> presentWords;
    auto setPresentBit = [&](int bitIndex) {
        int wordIndex = bitIndex / 31;
        int bitOffset = bitIndex % 31;
        if (wordIndex >= (int)presentWords.size())
            presentWords.resize(wordIndex + 1, 0);
        presentWords[wordIndex] |= 1U << bitOffset;
    };

    setPresentBit(RADIOTAP_FLAGS);
    std::vector<uint8_t> bytes(4, 0); // version, pad, length

    bool isHe = false;
    bool isEht = false;
    bool isVht = false;
    bool isHt = false;
    bool hasRate = false;
    uint8_t radiotapRate = 0;

    uint8_t mcsKnown = 0;
    uint8_t mcsFlags = 0;
    uint8_t mcsIndex = 0;

    uint16_t vhtKnown = 0;
    uint8_t vhtFlags = 0;
    uint8_t vhtBandwidth = 0;
    uint8_t vhtMcsNss[4] = {0};
    uint8_t vhtCoding = 0;
    uint8_t vhtGroupId = 0;
    uint16_t vhtPartialAid = 0;

    std::vector<MpduRange> mpduRanges;
    bool isAmpdu = false;
    bool isLastSubframe = false;
    uint32_t ampduRef = 0;

    Ptr<const physicallayer::Ieee80211HeMuReq> heMuReq;
    Ptr<const physicallayer::Ieee80211HeMuRxTag> heMuRx;
    Ptr<const physicallayer::Ieee80211HeMuCommonReq> heMuCommonReq;
    const physicallayer::Ieee80211HeMode *heMode = nullptr;
    const physicallayer::Ieee80211EhtMode *ehtMode = nullptr;
    const physicallayer::Ieee80211VhtMode *vhtMode = nullptr;
    const physicallayer::Ieee80211HtMode *htMode = nullptr;

#ifdef INET_WITH_IEEE80211
    heMuReq = packet->findTag<physicallayer::Ieee80211HeMuReq>();
    heMuRx = packet->findTag<physicallayer::Ieee80211HeMuRxTag>();
    heMuCommonReq = packet->findTag<physicallayer::Ieee80211HeMuCommonReq>();
    if (heMuReq != nullptr || heMuRx != nullptr || heMuCommonReq != nullptr) {
        isHe = true;
    }
    else {
        const physicallayer::IIeee80211Mode *mode = nullptr;
        auto modeReq = packet->findTag<physicallayer::Ieee80211ModeReq>();
        if (modeReq != nullptr)
            mode = modeReq->getMode();
        else {
            auto modeInd = packet->findTag<physicallayer::Ieee80211ModeInd>();
            if (modeInd != nullptr)
                mode = modeInd->getMode();
        }
        if (mode != nullptr) {
            auto dm = mode->getDataMode();
            if (dm != nullptr) {
                double rateVal = dm->getNetBitrate().get() / 500000.0;
                if (std::isfinite(rateVal) && rateVal >= 1 && rateVal <= 255) {
                    hasRate = true;
                    radiotapRate = static_cast<uint8_t>(std::round(rateVal));
                }
            }

            heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(mode);
            if (heMode != nullptr) {
                isHe = true;
            }
            else {
                ehtMode = dynamic_cast<const physicallayer::Ieee80211EhtMode *>(mode);
                if (ehtMode != nullptr) {
                    isEht = true;
                }
                else {
                    vhtMode = dynamic_cast<const physicallayer::Ieee80211VhtMode *>(mode);
                    if (vhtMode != nullptr && dm != nullptr) {
                        isVht = true;
                        auto vhtDm = dynamic_cast<const physicallayer::Ieee80211VhtDataMode *>(dm);
                        if (vhtDm != nullptr) {
                            vhtKnown = 0x0001 | 0x0004 | 0x0040; // STBC, GI, BW known
                            vhtFlags = (vhtDm->getGuardIntervalType() == physicallayer::Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT ? 0x04 : 0);
                            double bw = vhtDm->getBandwidth().get();
                            vhtBandwidth = (bw < 30e6 ? 0 : bw < 60e6 ? 1 : bw < 100e6 ? 4 : 11);
                            auto mcs = vhtDm->getModulationAndCodingScheme()->getMcsIndex();
                            auto nss = vhtDm->getNumberOfSpatialStreams();
                            vhtMcsNss[0] = (mcs << 4) | nss;
                            vhtCoding = (vhtDm->getCode() != nullptr && vhtDm->getCode()->isLdpc() ? 1 : 0);
                        }
                    }
                    else {
                        htMode = dynamic_cast<const physicallayer::Ieee80211HtMode *>(mode);
                        if (htMode != nullptr && dm != nullptr) {
                            isHt = true;
                            auto htDm = dynamic_cast<const physicallayer::Ieee80211HtDataMode *>(dm);
                            if (htDm != nullptr) {
                                mcsKnown = 0x01 | 0x02 | 0x04 | 0x10; // BW, MCS, GI, FEC known
                                if (htDm->getBandwidth().get() > 30e6) mcsFlags |= 1; // 40 MHz
                                if (htDm->getGuardIntervalType() == physicallayer::Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) mcsFlags |= (1 << 2);
                                if (htDm->getCode() != nullptr && htDm->getCode()->isLdpc()) mcsFlags |= (1 << 4);
                                mcsIndex = htDm->getModulationAndCodingScheme()->getMcsIndex();
                            }
                        }
                    }
                }
            }
        }
    }

    for (b startOffset = b(0); startOffset <= frontOffset; startOffset += b(8)) {
        for (b backOffsetIdx = b(0); backOffsetIdx <= backOffset; backOffsetIdx += b(8)) {
            std::vector<MpduRange> tempRanges;
            if (getIeee80211AmpduMpduRanges(packet, startOffset, backOffsetIdx, tempRanges)) {
                for (size_t i = 0; i < tempRanges.size(); ++i) {
                    if (tempRanges[i].offset == frontOffset) {
                        isAmpdu = true;
                        mpduRanges = tempRanges;
                        ampduRef = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(packet) & 0xFFFFFFFF);
                        if (i == tempRanges.size() - 1) {
                            isLastSubframe = true;
                        }
                        break;
                    }
                }
            }
            if (isAmpdu) break;
        }
        if (isAmpdu) break;
    }
#endif

    // IEEE 802.11 packets recorded by PcapRecorder contain the MAC trailer.
    uint8_t flags = RADIOTAP_F_FCS;
    if (packet->hasBitError())
        flags |= RADIOTAP_F_BADFCS;
    bytes.push_back(flags);

    // 2. RADIOTAP_RATE (2)
    if (hasRate) {
        setPresentBit(RADIOTAP_RATE);
        bytes.push_back(radiotapRate);
    }

#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    const physicallayer::ISignalAnalogModel *analogModel = nullptr;
    simtime_t startTime;
    simtime_t endTime;
    if (reception != nullptr) {
        analogModel = reception->getAnalogModel();
        startTime = reception->getStartTime();
        endTime = reception->getEndTime();
    }
    else if (transmission != nullptr) {
        analogModel = transmission->getAnalogModel();
        startTime = transmission->getStartTime();
        endTime = transmission->getEndTime();
    }

    auto narrowbandAnalogModel = dynamic_cast<const physicallayer::INarrowbandSignalAnalogModel *>(analogModel);
    if (narrowbandAnalogModel != nullptr) {
        double frequencyMHz = narrowbandAnalogModel->getCenterFrequency().get() / 1E6;
        if (std::isfinite(frequencyMHz) && 0 < frequencyMHz && frequencyMHz <= UINT16_MAX) {
            setPresentBit(RADIOTAP_CHANNEL);
            appendPadding(bytes, 2);
            appendUint16(bytes, static_cast<uint16_t>(std::round(frequencyMHz)));
            uint16_t channelFlags = frequencyMHz < 3000 ? RADIOTAP_CHANNEL_2GHZ :
                    frequencyMHz < 6000 ? RADIOTAP_CHANNEL_5GHZ : 0;
            appendUint16(bytes, channelFlags);
        }

        auto power = narrowbandAnalogModel->computeMinPower(startTime, endTime);
        double powerMilliwatts = power.get<units::values::mW>();
        if (std::isfinite(powerMilliwatts) && 0 < powerMilliwatts &&
                (direction == DIRECTION_INBOUND || direction == DIRECTION_OUTBOUND)) {
            int powerDbm = static_cast<int>(std::round(math::mW2dBmW(powerMilliwatts)));
            powerDbm = std::clamp(powerDbm, -128, 127);
            if (direction == DIRECTION_INBOUND)
                setPresentBit(RADIOTAP_ANTENNA_SIGNAL);
            else if (direction == DIRECTION_OUTBOUND)
                setPresentBit(RADIOTAP_DBM_TX_POWER);
            bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(powerDbm)));
        }
    }
#endif

    if (direction == DIRECTION_INBOUND) {
        setPresentBit(RADIOTAP_RX_FLAGS);
        appendPadding(bytes, 2);
        appendUint16(bytes, 0);
    }
    else if (direction == DIRECTION_OUTBOUND) {
        setPresentBit(RADIOTAP_TX_FLAGS);
        appendPadding(bytes, 2);
        appendUint16(bytes, 0);
    }

#ifdef INET_WITH_IEEE80211
    // 8. RADIOTAP_MCS (19)
    if (isHt) {
        setPresentBit(RADIOTAP_MCS);
        bytes.push_back(mcsKnown);
        bytes.push_back(mcsFlags);
        bytes.push_back(mcsIndex);
    }

    // 9. RADIOTAP_AMPDU (20)
    if (isAmpdu) {
        setPresentBit(RADIOTAP_AMPDU);
        appendPadding(bytes, 4);
        appendUint32(bytes, ampduRef);
        uint16_t ampduFlags = 0x0004 | (isLastSubframe ? 0x0008 : 0);
        appendUint16(bytes, ampduFlags);
        bytes.push_back(0); // delimiter CRC
        bytes.push_back(0); // reserved
    }

    // 10. RADIOTAP_VHT (21)
    if (isVht) {
        setPresentBit(RADIOTAP_VHT);
        appendPadding(bytes, 2);
        appendUint16(bytes, vhtKnown);
        bytes.push_back(vhtFlags);
        bytes.push_back(vhtBandwidth);
        for (int i = 0; i < 4; ++i)
            bytes.push_back(vhtMcsNss[i]);
        bytes.push_back(vhtCoding);
        bytes.push_back(vhtGroupId);
        appendUint16(bytes, vhtPartialAid);
    }

    // 11. RADIOTAP_HE (23)
    if (isHe) {
        setPresentBit(RADIOTAP_HE);

        appendPadding(bytes, 2);
        uint16_t data1 = 0;
        uint16_t data2 = 0;
        uint16_t data3 = 0;
        uint16_t data4 = 0;
        uint16_t data5 = 0;
        uint16_t data6 = 0;

        if (heMuReq != nullptr) {
            auto ppduFormat = static_cast<physicallayer::Ieee80211HePpduFormat>(heMuReq->getPpduFormat());
            auto radiotapFormat = getRadiotapHeFormat(ppduFormat);
            data1 |= radiotapFormat | RADIOTAP_HE_DATA_MCS_KNOWN | RADIOTAP_HE_DATA_DCM_KNOWN |
                    RADIOTAP_HE_CODING_KNOWN | RADIOTAP_HE_SPATIAL_REUSE_KNOWN;
            if (ppduFormat == physicallayer::HE_MU_DOWNLINK || ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK) {
                data1 |= RADIOTAP_HE_UL_DL_KNOWN;
                if (ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK)
                    data3 |= 0x0080;
            }
            if (ppduFormat == physicallayer::HE_MU_DOWNLINK) {
                data1 |= RADIOTAP_HE_MU_STA_ID_KNOWN;
                data4 |= (heMuReq->getStaId() & 0x7ff) << 4;
            }
            data2 |= RADIOTAP_HE_GI_KNOWN;
            data3 |= (heMuReq->getMcs() & 0xf) << 8;
            data3 |= (heMuReq->getDcm() ? 1U : 0U) << 12;
            data3 |= (heMuReq->getCoding() & 0x1) << 13;
            data4 |= heMuReq->getSpatialReuse() & 0xf;
            auto ruAllocation = getRadiotapHeRuAllocation(heMuReq->getRuToneSize());
            if (ruAllocation >= 0) {
                data1 |= RADIOTAP_HE_BW_RU_ALLOC_KNOWN;
                data5 |= ruAllocation;
            }
            data5 |= (heMuReq->getGuardInterval() & 0x3) << 4;
            data6 |= std::clamp<int>(heMuReq->getNumberOfSpatialStreams(), 1, 15);
        }
        else if (heMuRx != nullptr) {
            auto ppduFormat = static_cast<physicallayer::Ieee80211HePpduFormat>(heMuRx->getPpduFormat());
            data1 |= getRadiotapHeFormat(ppduFormat) | RADIOTAP_HE_CODING_KNOWN;
            if (ppduFormat == physicallayer::HE_MU_DOWNLINK || ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK) {
                data1 |= RADIOTAP_HE_UL_DL_KNOWN;
                if (ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK)
                    data3 |= 0x0080;
            }
            data2 |= RADIOTAP_HE_GI_KNOWN;
            data3 |= (heMuRx->getCoding() & 0x1) << 13;
            data5 |= (heMuRx->getGuardInterval() & 0x3) << 4;

            const physicallayer::Ieee80211HeMuRxAllocationInfo *capturedAllocation = nullptr;
            for (size_t i = 0; i < heMuRx->getAllocationsArraySize(); ++i) {
                const auto& allocation = heMuRx->getAllocations(i);
                if (allocation.ruIndex == heMuRx->getRuIndex()) {
                    capturedAllocation = &allocation;
                    break;
                }
            }
            if (capturedAllocation == nullptr && heMuRx->getAllocationsArraySize() == 1)
                capturedAllocation = &heMuRx->getAllocations(0);
            if (capturedAllocation != nullptr) {
                data1 |= RADIOTAP_HE_DATA_MCS_KNOWN | RADIOTAP_HE_DATA_DCM_KNOWN;
                data3 |= (capturedAllocation->mcs & 0xf) << 8;
                data3 |= (capturedAllocation->dcm ? 1U : 0U) << 12;
                data6 |= std::clamp<int>(capturedAllocation->numberOfSpatialStreams, 1, 15);
                auto ruAllocation = getRadiotapHeRuAllocation(capturedAllocation->ruToneSize);
                if (ruAllocation >= 0) {
                    data1 |= RADIOTAP_HE_BW_RU_ALLOC_KNOWN;
                    data5 |= ruAllocation;
                }
                if (ppduFormat == physicallayer::HE_MU_DOWNLINK) {
                    data1 |= RADIOTAP_HE_MU_STA_ID_KNOWN;
                    data4 |= (capturedAllocation->staId & 0x7ff) << 4;
                }
            }
        }
        else if (heMode != nullptr) {
            auto preambleFormat = heMode->getPreambleMode()->getPreambleFormat();
            data1 |= preambleFormat == physicallayer::Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU ?
                    RADIOTAP_HE_FORMAT_EXT_SU : RADIOTAP_HE_FORMAT_SU;
            auto dm = heMode->getDataMode();
            if (dm != nullptr) {
                data1 |= RADIOTAP_HE_DATA_MCS_KNOWN | RADIOTAP_HE_CODING_KNOWN |
                        RADIOTAP_HE_BW_RU_ALLOC_KNOWN;
                data2 |= RADIOTAP_HE_GI_KNOWN;
                data3 |= (dm->getMcsIndex() & 0xf) << 8;
                data3 |= (dm->isLdpc() ? 1U : 0U) << 13;
                auto gi = dm->getGuardIntervalType();
                auto radiotapGi = gi == physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT ? 0 :
                        gi == physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM ? 1 : 2;
                // HE ER SU is modelled with the mandatory 242-tone allocation;
                // ordinary HE SU uses the channel-width encoding.
                data5 |= preambleFormat == physicallayer::Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU ?
                        getRadiotapHeRuAllocation(242) : getRadiotapHeBandwidth(dm->getBandwidth());
                data5 |= radiotapGi << 4;
                data6 |= std::clamp<int>(dm->getNumberOfSpatialStreams(), 1, 15);
            }
        }
        else if (heMuCommonReq != nullptr) {
            data1 |= RADIOTAP_HE_FORMAT_MU | RADIOTAP_HE_UL_DL_KNOWN | RADIOTAP_HE_CODING_KNOWN;
            data2 |= RADIOTAP_HE_GI_KNOWN;
            data3 |= (heMuCommonReq->getCoding() & 0x1) << 13;
            data5 |= (heMuCommonReq->getGuardInterval() & 0x3) << 4;
        }

        appendUint16(bytes, data1);
        appendUint16(bytes, data2);
        appendUint16(bytes, data3);
        appendUint16(bytes, data4);
        appendUint16(bytes, data5);
        appendUint16(bytes, data6);
    }

    // 13. RADIOTAP_0_LENGTH_PSDU (26)
    b recordedLength = packet->getDataLength() - frontOffset - backOffset;
    if (recordedLength == b(0)) {
        setPresentBit(RADIOTAP_0_LENGTH_PSDU);
        bytes.push_back(0); // sounding PPDU / zero-length PSDU
    }

    // 14. RADIOTAP_EHT (34)
    if (isEht) {
        setPresentBit(RADIOTAP_EHT);
        appendPadding(bytes, 4);
        appendUint32(bytes, 0); // EHT known: 0
    }
#endif

    // Set the extension flags for all intermediate present words
    for (size_t i = 0; i < presentWords.size(); i++) {
        if (i < presentWords.size() - 1) {
            presentWords[i] |= 1U << 31;
        }
    }

    // Insert the present words at index 4
    std::vector<uint8_t> presentBytes(4 * presentWords.size(), 0);
    for (size_t i = 0; i < presentWords.size(); i++) {
        uint32_t val = presentWords[i];
        presentBytes[4 * i] = val & 0xff;
        presentBytes[4 * i + 1] = (val >> 8) & 0xff;
        presentBytes[4 * i + 2] = (val >> 16) & 0xff;
        presentBytes[4 * i + 3] = (val >> 24) & 0xff;
    }
    bytes.insert(bytes.begin() + 4, presentBytes.begin(), presentBytes.end());

    // Update length
    setUint16(bytes, 2, bytes.size());
    return bytes;
}

} // namespace

// ----

Define_Module(PcapRecorder);

simsignal_t PcapRecorder::packetRecordedSignal = registerSignal("packetRecorded");

PcapRecorder::~PcapRecorder()
{
    delete pcapWriter;
    for (auto helper : helpers)
        delete helper;
}

PcapRecorder::PcapRecorder() : SimpleModule()
{
}

bool PcapRecorder::shouldDissectProtocolDataUnit(const Protocol *protocol)
{
    return !contains(dumpProtocols, protocol);
}

void PcapRecorder::startProtocolDataUnit(const Protocol *protocol)
{
    if (contains(dumpProtocols, protocol))
        dumpProtocol = protocol;
}

void PcapRecorder::visitChunk(const Ptr<const Chunk>& chunk, const Protocol *protocol)
{
    if (!contains(dumpProtocols, protocol)) {
        if (dumpProtocol == nullptr)
            frontOffset += chunk->getChunkLength();
        else
            backOffset += chunk->getChunkLength();
    }
    else
        dumpProtocol = protocol;
}

void PcapRecorder::initialize()
{
    verbose = par("verbose");
    recordEmptyPackets = par("recordEmptyPackets");
    enableConvertingPackets = par("enableConvertingPackets");
    snaplen = this->par("snaplen");
    dumpBadFrames = par("dumpBadFrames");
    signalList.clear();
    packetFilter.setExpression(par("packetFilter").objectValue());

    {
        cStringTokenizer signalTokenizer(par("sendingSignalNames"));

        while (signalTokenizer.hasMoreTokens())
            signalList[registerSignal(signalTokenizer.nextToken())] = DIRECTION_OUTBOUND;
    }

    {
        cStringTokenizer signalTokenizer(par("receivingSignalNames"));

        while (signalTokenizer.hasMoreTokens())
            signalList[registerSignal(signalTokenizer.nextToken())] = DIRECTION_INBOUND;
    }

    {
        cStringTokenizer protocolTokenizer(par("dumpProtocols"));

        while (protocolTokenizer.hasMoreTokens())
            dumpProtocols.push_back(Protocol::getProtocol(protocolTokenizer.nextToken()));
    }

    {
        cStringTokenizer protocolTokenizer(par("helpers"));

        while (protocolTokenizer.hasMoreTokens())
            helpers.push_back(check_and_cast<IHelper *>(createOne(protocolTokenizer.nextToken())));
    }

    const char *moduleNames = par("moduleNamePatterns");
    cStringTokenizer moduleTokenizer(moduleNames);

    while (moduleTokenizer.hasMoreTokens()) {
        bool found = false;
        std::string mname(moduleTokenizer.nextToken());
        bool isAllIndex = (mname.length() > 3) && mname.rfind("[*]") == mname.length() - 3;

        if (isAllIndex)
            mname.replace(mname.length() - 3, 3, "");

        if (mname[0] == '.') {
            for (auto& elem : signalList)
                getParentModule()->subscribe(elem.first, this);
            found = true;
        }
        else {
            for (cModule::SubmoduleIterator i(getParentModule()); !i.end(); i++) {
                cModule *submod = *i;
                if (0 == strcmp(isAllIndex ? submod->getName() : submod->getFullName(), mname.c_str())) {
                    found = true;

                    for (auto& elem : signalList) {
                        if (!submod->isSubscribed(elem.first, this)) {
                            submod->subscribe(elem.first, this);
                            EV_INFO << "Subscribing to " << submod->getFullPath() << ":" << getSignalName(elem.first) << EV_ENDL;
                        }
                    }
                }
            }
        }

        if (!found && !isAllIndex)
            EV_INFO << "The module " << mname << (isAllIndex ? "[*]" : "") << " not found" << EV_ENDL;
    }

    std::string fileName = getEnvir()->getConfig()->substituteVariables(par("pcapFile"));
    const char *fileFormat = par("fileFormat");
    int timePrecision = par("timePrecision");
    if (!strcmp(fileFormat, "pcap"))
        pcapWriter = new PcapWriter();
    else if (!strcmp(fileFormat, "pcapng"))
        pcapWriter = new PcapngWriter();
    else
        throw cRuntimeError("Unknown fileFormat parameter: '%s'", fileFormat);

    recordPcap = !fileName.empty();
    if (recordPcap) {
        pcapWriter->open(fileName.c_str(), snaplen, timePrecision);
        pcapWriter->setFlush(par("alwaysFlush"));
    }

    WATCH(recordPcap);
    WATCH(frontOffset);
    WATCH(backOffset);
    WATCH(numRecorded);
}

void PcapRecorder::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not handle messages");
}

std::string PcapRecorder::resolveDirective(char directive) const
{
    switch (directive) {
        case 'n':
            return std::to_string(numRecorded);
        default:
            return SimpleModule::resolveDirective(directive);   
    }
}

void PcapRecorder::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (pcapWriter->isOpen()) {
        auto i = signalList.find(signalID);
        ASSERT(i != signalList.end());
        Direction direction = i->second;
        if (false)
            ;
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
        else if (auto signal = dynamic_cast<const physicallayer::Signal *>(obj))
            recordPacket(signal->getEncapsulatedPacket(), direction, source);
#endif
        else if (auto packet = dynamic_cast<cPacket *>(obj))
            recordPacket(packet, direction, source);
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
        else if (auto transmission = dynamic_cast<const physicallayer::ITransmission *>(obj)) {
            physicalLayerTransmission = transmission;
            recordPacket(transmission->getPacket(), direction, source);
            physicalLayerTransmission = nullptr;
        }
        else if (auto reception = dynamic_cast<const physicallayer::IReception *>(obj)) {
            physicalLayerTransmission = reception->getTransmission();
            physicalLayerReception = reception;
            recordPacket(reception->getTransmission()->getPacket(), direction, source);
            physicalLayerTransmission = nullptr;
            physicalLayerReception = nullptr;
        }
#endif
    }
}

void PcapRecorder::writePacket(const Protocol *protocol, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface)
{
#ifdef INET_WITH_IEEE80211
    if (*protocol == Protocol::ieee80211Mac) {
        std::vector<MpduRange> mpduRanges;
        if (getIeee80211AmpduMpduRanges(packet, frontOffset, backOffset, mpduRanges)) {
            for (const auto& mpduRange : mpduRanges)
                writePacketRecord(protocol, packet, mpduRange.offset, packet->getDataLength() - mpduRange.offset - mpduRange.length, direction, networkInterface);
            return;
        }
    }
#endif
    writePacketRecord(protocol, packet, frontOffset, backOffset, direction, networkInterface);
}

void PcapRecorder::writePacketRecord(const Protocol *protocol, const Packet *packet, b frontOffset, b backOffset, Direction direction, NetworkInterface *networkInterface)
{
    auto pcapLinkType = protocolToLinkType(protocol);
    if (pcapLinkType == LINKTYPE_INVALID)
        throw cRuntimeError("Cannot determine the PCAP link type from protocol '%s'", protocol->getName());
    bool convertPacket = !matchesLinkType(pcapLinkType, protocol);
    if (convertPacket) {
        recordingDirection = direction;
        packet = tryConvertToLinkType(packet, frontOffset, backOffset, pcapLinkType, protocol);
        recordingDirection = DIRECTION_UNDEFINED;
        if (packet == nullptr)
            throw cRuntimeError("The protocol '%s' doesn't match PCAP link type %d", protocol->getName(), pcapLinkType);
        frontOffset = b(0);
        backOffset = b(0);
    }
    b recordedLength = packet->getDataLength() - frontOffset - backOffset;
    if (recordEmptyPackets || recordedLength != b(0)) {
        pcapWriter->writePacket(simTime(), packet, frontOffset, backOffset, direction, networkInterface, pcapLinkType);
        numRecorded++;
        emit(packetRecordedSignal, packet);
    }
    if (convertPacket)
        delete packet;
}

void PcapRecorder::recordPacket(const cPacket *cpacket, Direction direction, cComponent *source)
{
    if (auto packet = dynamic_cast<const Packet *>(cpacket)) {
        EV_INFO << "Recording packet" << EV_FIELD(source, source->getFullPath()) << EV_FIELD(direction, direction) << EV_FIELD(packet) << EV_ENDL;
        if (verbose)
            EV_DEBUG << "Dumping packet" << EV_FIELD(packet, packetPrinter.printPacketToString(const_cast<Packet *>(packet), "%i")) << EV_ENDL;
        if (recordPcap && packetFilter.matches(packet) && (dumpBadFrames || !packet->hasBitError())) {
            // get Direction
            if (direction == DIRECTION_UNDEFINED) {
                if (auto directionTag = packet->findTag<DirectionTag>())
                    direction = directionTag->getDirection();
            }

            // get NetworkInterface
            auto srcModule = check_and_cast<cModule *>(source);
            auto networkInterface = findContainingNicModule(srcModule);
            if (networkInterface == nullptr) {
                int ifaceId = -1;
                if (direction == DIRECTION_OUTBOUND) {
                    if (auto ifaceTag = packet->findTag<InterfaceReq>())
                        ifaceId = ifaceTag->getInterfaceId();
                }
                else if (direction == DIRECTION_INBOUND) {
                    if (auto ifaceTag = packet->findTag<InterfaceInd>())
                        ifaceId = ifaceTag->getInterfaceId();
                }
                if (ifaceId != -1) {
                    auto ift = check_and_cast_nullable<InterfaceTable *>(getContainingNode(srcModule)->getSubmodule("interfaceTable"));
                    networkInterface = ift->getInterfaceById(ifaceId);
                }
            }

            const auto& packetProtocolTag = packet->getTag<PacketProtocolTag>();
            auto protocol = packetProtocolTag->getProtocol();
            if (contains(dumpProtocols, protocol))
                writePacket(protocol, packet, packetProtocolTag->getFrontOffset(), packetProtocolTag->getBackOffset(), direction, networkInterface);
            else {
                frontOffset = b(0);
                backOffset = b(0);
                dumpProtocol = nullptr;
                Packet dissectedPacket(*packet);
                PacketDissector packetDissector(ProtocolDissectorRegistry::getInstance(), *this);
                packetDissector.dissectPacket(&dissectedPacket);
                if (dumpProtocol != nullptr)
                    writePacket(dumpProtocol, packet, frontOffset, backOffset, direction, networkInterface);
            }
        }
    }
}

void PcapRecorder::finish()
{
    pcapWriter->close();
}

bool PcapRecorder::matchesLinkType(PcapLinkType pcapLinkType, const Protocol *protocol) const
{
    if (protocol == nullptr)
        return false;
    else if (*protocol == Protocol::ethernetPhy)
        return pcapLinkType == LINKTYPE_ETHERNET_MPACKET;
    else if (*protocol == Protocol::ethernetMac)
        return pcapLinkType == LINKTYPE_ETHERNET;
    else if (*protocol == Protocol::ppp)
        return pcapLinkType == LINKTYPE_PPP_WITH_DIR;
    else if (*protocol == Protocol::ieee80211Mac)
        // A bare MAC frame only matches the non-Radiotap IEEE 802.11 link type.
        return pcapLinkType == LINKTYPE_IEEE802_11;
    else if (*protocol == Protocol::ipv4)
        return pcapLinkType == LINKTYPE_RAW || pcapLinkType == LINKTYPE_IPV4;
    else if (*protocol == Protocol::ipv6)
        return pcapLinkType == LINKTYPE_RAW || pcapLinkType == LINKTYPE_IPV6;
    else if (*protocol == Protocol::ieee802154)
        return pcapLinkType == LINKTYPE_IEEE802_15_4 || pcapLinkType == LINKTYPE_IEEE802_15_4_NOFCS;
    else {
        for (auto helper : helpers) {
            if (helper->matchesLinkType(pcapLinkType, protocol))
                return true;
        }
    }
    return false;
}

PcapLinkType PcapRecorder::protocolToLinkType(const Protocol *protocol) const
{
    if (*protocol == Protocol::ethernetPhy)
        return LINKTYPE_ETHERNET_MPACKET;
    else if (*protocol == Protocol::ethernetMac)
        return LINKTYPE_ETHERNET;
    else if (*protocol == Protocol::ppp)
        return LINKTYPE_PPP_WITH_DIR;
    else if (*protocol == Protocol::ieee80211Mac)
        return LINKTYPE_IEEE802_11_RADIOTAP;
    else if (*protocol == Protocol::ipv4 || *protocol == Protocol::ipv6)
        return LINKTYPE_RAW;
    else if (*protocol == Protocol::ieee802154)
        return LINKTYPE_IEEE802_15_4;
    else {
        for (auto helper : helpers) {
            auto lt = helper->protocolToLinkType(protocol);
            if (lt != LINKTYPE_INVALID)
                return lt;
        }
    }
    return LINKTYPE_INVALID;
}

Packet *PcapRecorder::tryConvertToLinkType(const Packet *packet, b frontOffset, b backOffset, PcapLinkType pcapLinkType, const Protocol *protocol) const
{
    if (enableConvertingPackets) {
        if (*protocol == Protocol::ieee80211Mac && pcapLinkType == LINKTYPE_IEEE802_11_RADIOTAP) {
            auto convertedPacket = new Packet(packet->getName());
            convertedPacket->insertAtBack(makeShared<BytesChunk>(makeRadiotapHeader(packet, frontOffset, backOffset, recordingDirection, physicalLayerTransmission, physicalLayerReception)));
            b dataLength = packet->getDataLength() - frontOffset - backOffset;
            if (dataLength != b(0))
                convertedPacket->insertAtBack(packet->peekDataAt(frontOffset, dataLength));
            convertedPacket->setBitError(packet->hasBitError());
            return convertedPacket;
        }
        for (IHelper *helper : helpers) {
            if (auto newPacket = helper->tryConvertToLinkType(packet, frontOffset, backOffset, pcapLinkType, protocol))
                return newPacket;
        }
    }
    return nullptr;
}

} // namespace inet
