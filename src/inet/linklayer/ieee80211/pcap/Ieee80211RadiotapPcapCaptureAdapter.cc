//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/pcap/Ieee80211RadiotapPcapCaptureAdapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "inet/common/INETMath.h"
#include "inet/common/packet/recorder/PcapCaptureAdapterRegistry.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReception.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

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
    RADIOTAP_U_SIG = 33,
    RADIOTAP_EHT = 34,
};

enum RadiotapFlags {
    RADIOTAP_F_FCS = 0x10,
    RADIOTAP_F_BADFCS = 0x40,
};

enum RadiotapVhtKnown {
    RADIOTAP_VHT_STBC_KNOWN = 1U << 0,
    RADIOTAP_VHT_TXOP_PS_NOT_ALLOWED_KNOWN = 1U << 1,
    RADIOTAP_VHT_GI_KNOWN = 1U << 2,
    RADIOTAP_VHT_SGI_NSYM_DISAMBIG_KNOWN = 1U << 3,
    RADIOTAP_VHT_LDPC_EXTRA_OFDM_SYM_KNOWN = 1U << 4,
    RADIOTAP_VHT_BEAMFORMED_KNOWN = 1U << 5,
    RADIOTAP_VHT_BANDWIDTH_KNOWN = 1U << 6,
};

enum RadiotapVhtFlags {
    RADIOTAP_VHT_BEAMFORMED = 1U << 5,
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
    RADIOTAP_HE_BSS_COLOR_KNOWN = 0x0004,
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

enum RadiotapUsigCommon {
    RADIOTAP_U_SIG_PHY_VERSION_KNOWN = 0x00000001,
    RADIOTAP_U_SIG_BANDWIDTH_KNOWN = 0x00000002,
};

enum RadiotapEhtKnown {
    RADIOTAP_EHT_GI_KNOWN = 0x00000004,
    RADIOTAP_EHT_NUMBER_NON_OFDMA_USERS_KNOWN = 0x00080000,
};

enum RadiotapEhtUserInfo {
    RADIOTAP_EHT_USER_MCS_KNOWN = 0x00000002,
    RADIOTAP_EHT_USER_CODING_KNOWN = 0x00000004,
    RADIOTAP_EHT_USER_NSS_KNOWN = 0x00000010,
    RADIOTAP_EHT_USER_DATA_FOR_USER = 0x00000080,
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

uint8_t getRadiotapVhtBandwidth(Hz bandwidth)
{
    auto value = bandwidth.get();
    if (value < 30e6)
        return 0;
    if (value < 60e6)
        return 1;
    if (value < 100e6)
        return 4;
    if (value < 180e6)
        return 11;
    throw cRuntimeError("Unsupported VHT radiotap channel width: %g Hz", value);
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
    int muUserIndex = -1;
};

struct RadiotapRecordMetadata
{
    int muUserIndex = -1;
    bool isAmpdu = false;
    bool isLastSubframe = false;
    uint32_t ampduReference = 0;
    bool zeroLength = false;
};

struct RadiotapHeFields
{
    std::array<uint16_t, 6> data = {};
};

struct RadiotapPpduFields
{
    Direction direction = DIRECTION_UNDEFINED;
    uint8_t flags = 0;
    bool hasRate = false;
    uint8_t rate = 0;
    bool hasChannel = false;
    uint16_t channelFrequency = 0;
    uint16_t channelFlags = 0;
    bool hasPower = false;
    int8_t power = 0;
    bool isHt = false;
    std::array<uint8_t, 3> mcs = {};
    bool isVht = false;
    uint16_t vhtKnown = 0;
    uint8_t vhtFlags = 0;
    uint8_t vhtBandwidth = 0;
    std::array<uint8_t, 4> vhtMcsNss = {};
    uint8_t vhtCoding = 0;
    uint8_t vhtGroupId = 0;
    uint16_t vhtPartialAid = 0;
    bool isHe = false;
    RadiotapHeFields he;
    std::vector<RadiotapHeFields> heMuUsers;
    bool hasHeMu = false;
    std::array<uint8_t, 4> heMuRuChannel1 = {};
    bool isEht = false;
    uint32_t uSigCommon = 0;
    uint32_t ehtKnown = 0;
    std::array<uint32_t, 9> ehtData = {};
    std::vector<uint32_t> ehtUserInfo;
};

uint32_t makeAmpduReference(const Packet *packet, int muUserIndex)
{
    auto treeId = static_cast<uint64_t>(packet->getTreeId());
    uint32_t reference = static_cast<uint32_t>(treeId) ^ static_cast<uint32_t>(treeId >> 32);
    if (muUserIndex >= 0)
        reference ^= static_cast<uint32_t>(muUserIndex + 1) * 0x9E3779B9U;
    return reference;
}

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

bool getIeee80211HeMuAmpduMpduRanges(const Packet *packet, b frontOffset, b backOffset, std::vector<MpduRange>& mpduRanges)
{
    auto heTxVectorReq = packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    if (heTxVectorReq == nullptr || heTxVectorReq->getPpduLayout() == nullptr)
        return false;
    const auto& ppduLayout = heTxVectorReq->getPpduLayout();
    const auto& psduBitRanges = ppduLayout->getPsduBitRanges();
    if (ppduLayout->getPpduFormat() != physicallayer::HE_MU_DOWNLINK || psduBitRanges.size() < 2)
        return false;

    auto packetLength = packet->getDataLength();
    auto dataLength = packetLength - frontOffset - backOffset;
    if (psduBitRanges.back().getEndBitOffset() != dataLength)
        return false;

    std::vector<MpduRange> allMpduRanges;
    for (const auto& psduBitRange : psduBitRanges) {
        if (psduBitRange.getBitLength() == b(0))
            continue;
        auto psduFrontOffset = frontOffset + psduBitRange.getStartBitOffset();
        auto psduBackOffset = packetLength - frontOffset - psduBitRange.getEndBitOffset();
        std::vector<MpduRange> psduMpduRanges;
        if (!getIeee80211AmpduMpduRanges(packet, psduFrontOffset, psduBackOffset, psduMpduRanges))
            return false;
        for (auto& mpduRange : psduMpduRanges)
            mpduRange.muUserIndex = static_cast<int>(psduBitRange.getUserIndex());
        allMpduRanges.insert(allMpduRanges.end(), psduMpduRanges.begin(), psduMpduRanges.end());
    }
    if (allMpduRanges.empty())
        return false;
    mpduRanges = std::move(allMpduRanges);
    return true;
}

bool getIeee80211VhtMuAmpduMpduRanges(const Packet *packet, b frontOffset, b backOffset, std::vector<MpduRange>& mpduRanges)
{
    auto vhtTxVectorReq = packet->findTag<physicallayer::Ieee80211VhtTxVectorReq>();
    if (vhtTxVectorReq == nullptr || vhtTxVectorReq->getTxVector() == nullptr)
        return false;
    const auto& txVector = vhtTxVectorReq->getTxVector();
    const auto& psduBitRanges = txVector->getPsduBitRanges();
    if (!txVector->isMu() || psduBitRanges.size() < 2)
        return false;

    auto packetLength = packet->getDataLength();
    auto dataLength = packetLength - frontOffset - backOffset;
    if (psduBitRanges.back().getEndBitOffset() != dataLength)
        return false;

    const int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    std::vector<MpduRange> allMpduRanges;
    for (const auto& psduBitRange : psduBitRanges) {
        if (psduBitRange.startBitOffset.get<b>() % 8 != 0 || psduBitRange.bitLength.get<b>() % 8 != 0)
            return false;
        auto psduFrontOffset = frontOffset + psduBitRange.startBitOffset;
        auto psduEndOffset = frontOffset + psduBitRange.getEndBitOffset();
        auto offset = psduFrontOffset;
        bool foundMpdu = false;
        try {
            while (offset < psduEndOffset) {
                auto delimiter = dynamicPtrCast<const ieee80211::Ieee80211MpduSubframeHeader>(
                        packet->peekDataAt(offset, b(-1), parsingFlags));
                if (delimiter == nullptr || delimiter->getLength() <= 0)
                    return false;
                auto mpduOffset = offset + delimiter->getChunkLength();
                auto mpduLength = B(delimiter->getLength());
                if (mpduOffset + mpduLength > psduEndOffset)
                    return false;
                allMpduRanges.push_back({mpduOffset, mpduLength, static_cast<int>(psduBitRange.userIndex)});
                foundMpdu = true;
                offset = mpduOffset + mpduLength;
                if (offset == psduEndOffset)
                    break;
                auto paddingLength = B((4 - (delimiter->getChunkLength() + mpduLength).get<B>() % 4) % 4);
                if (offset + paddingLength > psduEndOffset)
                    return false;
                offset += paddingLength;
            }
        }
        catch (cRuntimeError&) {
            return false;
        }
        if (!foundMpdu)
            return false;
    }
    if (allMpduRanges.empty())
        return false;
    mpduRanges = std::move(allMpduRanges);
    return true;
}

bool findIeee80211VhtMuAmpduMpduRanges(const Packet *packet, b mpduOffset, std::vector<MpduRange>& mpduRanges)
{
    auto vhtTxVectorReq = packet->findTag<physicallayer::Ieee80211VhtTxVectorReq>();
    if (vhtTxVectorReq == nullptr || vhtTxVectorReq->getTxVector() == nullptr)
        return false;
    const auto& psduBitRanges = vhtTxVectorReq->getTxVector()->getPsduBitRanges();
    if (psduBitRanges.empty())
        return false;

    auto packetLength = packet->getDataLength();
    auto dataLength = psduBitRanges.back().getEndBitOffset();
    auto dataFrontOffsetResidue = b(mpduOffset.get<b>() % 8);
    for (b dataFrontOffset = dataFrontOffsetResidue; dataFrontOffset <= mpduOffset; dataFrontOffset += b(8)) {
        auto dataBackOffset = packetLength - dataFrontOffset - dataLength;
        if (dataBackOffset < b(0))
            continue;
        std::vector<MpduRange> tempRanges;
        if (!getIeee80211VhtMuAmpduMpduRanges(packet, dataFrontOffset, dataBackOffset, tempRanges))
            continue;
        for (const auto& mpduRange : tempRanges) {
            if (mpduRange.offset == mpduOffset) {
                mpduRanges = std::move(tempRanges);
                return true;
            }
        }
    }
    return false;
}

bool findIeee80211HeMuAmpduMpduRanges(const Packet *packet, b mpduOffset, std::vector<MpduRange>& mpduRanges)
{
    auto heTxVectorReq = packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    if (heTxVectorReq == nullptr || heTxVectorReq->getPpduLayout() == nullptr)
        return false;
    const auto& psduBitRanges = heTxVectorReq->getPpduLayout()->getPsduBitRanges();
    if (psduBitRanges.empty())
        return false;

    auto packetLength = packet->getDataLength();
    auto dataLength = psduBitRanges.back().getEndBitOffset();
    auto dataFrontOffsetResidue = b(mpduOffset.get<b>() % 8);
    for (b dataFrontOffset = dataFrontOffsetResidue; dataFrontOffset <= mpduOffset; dataFrontOffset += b(8)) {
        auto dataBackOffset = packetLength - dataFrontOffset - dataLength;
        if (dataBackOffset < b(0))
            continue;
        std::vector<MpduRange> tempRanges;
        if (!getIeee80211HeMuAmpduMpduRanges(packet, dataFrontOffset, dataBackOffset, tempRanges))
            continue;
        for (const auto& mpduRange : tempRanges) {
            if (mpduRange.offset == mpduOffset) {
                mpduRanges = std::move(tempRanges);
                return true;
            }
        }
    }
    return false;
}

#endif

RadiotapPpduFields extractRadiotapPpduFields(const Packet *packet, Direction direction, const physicallayer::ITransmission *transmission,
        const physicallayer::IReception *reception)
{
    RadiotapPpduFields fields;
    fields.direction = direction;

    auto& isHe = fields.isHe;
    auto& isEht = fields.isEht;
    auto& isVht = fields.isVht;
    auto& isHt = fields.isHt;
    auto& hasRate = fields.hasRate;
    auto& radiotapRate = fields.rate;

    auto& mcsKnown = fields.mcs[0];
    auto& mcsFlags = fields.mcs[1];
    auto& mcsIndex = fields.mcs[2];

    auto& vhtKnown = fields.vhtKnown;
    auto& vhtFlags = fields.vhtFlags;
    auto& vhtBandwidth = fields.vhtBandwidth;
    auto& vhtMcsNss = fields.vhtMcsNss;
    auto& vhtCoding = fields.vhtCoding;
    auto& vhtGroupId = fields.vhtGroupId;
    auto& vhtPartialAid = fields.vhtPartialAid;

    auto& uSigCommon = fields.uSigCommon;
    auto& ehtKnown = fields.ehtKnown;
    auto& ehtData = fields.ehtData;
    auto& ehtUserInfo = fields.ehtUserInfo;
    double ehtBandwidthHz = NAN;

    int selectedMuUserIndex = -1;

    Ptr<const physicallayer::Ieee80211HeTxVectorReq> heTxVectorReq;
    Ptr<const physicallayer::Ieee80211HeRxVectorInd> heRxVectorInd;
    Ptr<const physicallayer::Ieee80211HeTbRecipientContextInd> heTbRecipientContext;
    const physicallayer::Ieee80211HeMode *heMode = nullptr;
    const physicallayer::Ieee80211EhtMode *ehtMode = nullptr;
    const physicallayer::Ieee80211VhtMode *vhtMode = nullptr;
    const physicallayer::Ieee80211VhtTxVector *vhtTxVector = nullptr;
    const physicallayer::Ieee80211HtMode *htMode = nullptr;

#ifdef INET_WITH_IEEE80211
    heTxVectorReq = packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    heRxVectorInd = packet->findTag<physicallayer::Ieee80211HeRxVectorInd>();
    heTbRecipientContext = packet->findTag<physicallayer::Ieee80211HeTbRecipientContextInd>();
    if (heTxVectorReq != nullptr || heRxVectorInd != nullptr) {
        isHe = true;
    }
    else {
        const physicallayer::IIeee80211Mode *mode = nullptr;
        auto vhtTxVectorReq = packet->findTag<physicallayer::Ieee80211VhtTxVectorReq>();
        if (vhtTxVectorReq != nullptr && vhtTxVectorReq->getTxVector() != nullptr)
            vhtTxVector = vhtTxVectorReq->getTxVector().get();
        auto ieee80211Transmission = dynamic_cast<const physicallayer::Ieee80211Transmission *>(transmission);
        if (ieee80211Transmission != nullptr) {
            mode = ieee80211Transmission->getMode();
            auto txVector = ieee80211Transmission->getVhtTxVector();
            if (txVector != nullptr)
                vhtTxVector = txVector.get();
        }
        else {
            auto modeReq = packet->findTag<physicallayer::Ieee80211ModeReq>();
            if (modeReq != nullptr)
                mode = modeReq->getMode();
            else {
                auto modeInd = packet->findTag<physicallayer::Ieee80211ModeInd>();
                if (modeInd != nullptr)
                    mode = modeInd->getMode();
            }
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
                    // The current EHT mode is SU and owns these PHY facts.
                    // Radiotap EHT does not carry channel bandwidth, so export
                    // that independently in U-SIG when its value is
                    // unambiguous. The two 320 MHz encodings depend on primary
                    // channel placement, which the mode does not expose.
                    uSigCommon = RADIOTAP_U_SIG_PHY_VERSION_KNOWN;
                    auto ehtDm = ehtMode->getDataMode();
                    if (ehtDm != nullptr) {
                        auto bandwidth = ehtDm->getBandwidth().get();
                        ehtBandwidthHz = bandwidth;
                        int radiotapBandwidth =
                                bandwidth == 20e6 ? 0 :
                                bandwidth == 40e6 ? 1 :
                                bandwidth == 80e6 ? 2 :
                                bandwidth == 160e6 ? 3 : -1;
                        if (radiotapBandwidth >= 0) {
                            uSigCommon |= RADIOTAP_U_SIG_BANDWIDTH_KNOWN;
                            uSigCommon |= radiotapBandwidth << 15;
                        }

                        // The currently implemented EHT mode represents one
                        // non-OFDMA SU user. Radiotap encodes N users as N-1,
                        // so data[7] remains zero while the known bit is set.
                        ehtKnown |= RADIOTAP_EHT_NUMBER_NON_OFDMA_USERS_KNOWN;
                        auto guardInterval = ehtDm->getGuardIntervalType();
                        int radiotapGi =
                                guardInterval == physicallayer::Ieee80211EhtModeBase::EHT_GUARD_INTERVAL_SHORT ? 0 :
                                guardInterval == physicallayer::Ieee80211EhtModeBase::EHT_GUARD_INTERVAL_MEDIUM ? 1 :
                                guardInterval == physicallayer::Ieee80211EhtModeBase::EHT_GUARD_INTERVAL_LONG ? 2 : -1;
                        if (radiotapGi >= 0) {
                            ehtKnown |= RADIOTAP_EHT_GI_KNOWN;
                            ehtData[0] |= radiotapGi << 7;
                        }

                        uint32_t userInfo = RADIOTAP_EHT_USER_DATA_FOR_USER;
                        auto modulationAndCodingScheme = ehtDm->getModulationAndCodingScheme();
                        if (modulationAndCodingScheme != nullptr) {
                            auto mcs = modulationAndCodingScheme->getMcsIndex();
                            if (mcs <= 13) {
                                userInfo |= RADIOTAP_EHT_USER_MCS_KNOWN;
                                userInfo |= mcs << 20;
                            }
                        }
                        auto numberOfSpatialStreams = ehtDm->getNumberOfSpatialStreams();
                        if (numberOfSpatialStreams >= 1 && numberOfSpatialStreams <= 16) {
                            userInfo |= RADIOTAP_EHT_USER_NSS_KNOWN;
                            userInfo |= (numberOfSpatialStreams - 1) << 24;
                        }
                        auto code = ehtDm->getCode();
                        if (code != nullptr) {
                            userInfo |= RADIOTAP_EHT_USER_CODING_KNOWN;
                            if (code->isLdpc())
                                userInfo |= 1U << 19;
                        }
                        ehtUserInfo.push_back(userInfo);
                    }
                }
                else {
                    vhtMode = dynamic_cast<const physicallayer::Ieee80211VhtMode *>(mode);
                    if (vhtMode != nullptr && dm != nullptr) {
                        isVht = true;
                        auto vhtDm = dynamic_cast<const physicallayer::Ieee80211VhtDataMode *>(dm);
                        if (vhtDm != nullptr) {
                            vhtKnown = RADIOTAP_VHT_STBC_KNOWN | RADIOTAP_VHT_GI_KNOWN |
                                    RADIOTAP_VHT_BANDWIDTH_KNOWN;
                            vhtFlags = (vhtDm->getGuardIntervalType() == physicallayer::Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT ? 0x04 : 0);
                            vhtBandwidth = getRadiotapVhtBandwidth(vhtDm->getBandwidth());
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

        // The packet-level VHT TXVECTOR is authoritative for MU user
        // signaling. The mode describes only the common PHY mode, while the
        // VHT radiotap field has one MCS/NSS byte and one coding bit per user.
        if (vhtTxVector != nullptr) {
            isVht = true;
            vhtBandwidth = getRadiotapVhtBandwidth(vhtTxVector->getChannelWidth());
            vhtGroupId = vhtTxVector->getGroupId();
            vhtPartialAid = vhtTxVector->getPartialAid();
            if (vhtTxVector->isMu()) {
                vhtKnown |= RADIOTAP_VHT_BEAMFORMED_KNOWN;
                if (vhtTxVector->isBeamformed())
                    vhtFlags |= RADIOTAP_VHT_BEAMFORMED;
                for (const auto& user : vhtTxVector->getUsers()) {
                    if (user.userPosition >= 4)
                        throw cRuntimeError("VHT MU user position cannot be represented by radiotap");
                    if (user.mcs > 9 || user.numberOfSpatialStreams == 0 || user.numberOfSpatialStreams > 8)
                        throw cRuntimeError("Invalid VHT MU user MCS/NSS for radiotap");
                    vhtMcsNss[user.userPosition] = (user.mcs << 4) | user.numberOfSpatialStreams;
                    if (user.ldpcCoding)
                        vhtCoding |= 1U << user.userPosition;
                }
            }
        }
    }

#endif

    // IEEE 802.11 packets recorded by PcapRecorder contain the MAC trailer.
    uint8_t flags = RADIOTAP_F_FCS;
    if (packet->hasBitError())
        flags |= RADIOTAP_F_BADFCS;
    fields.flags = flags;

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
            fields.hasChannel = true;
            fields.channelFrequency = static_cast<uint16_t>(std::round(frequencyMHz));
            fields.channelFlags = frequencyMHz < 3000 ? RADIOTAP_CHANNEL_2GHZ :
                    frequencyMHz < 6000 ? RADIOTAP_CHANNEL_5GHZ : 0;

            // IEEE 802.11be-2024 Table 36-28 and 36.3.24.2 distinguish two
            // 320 MHz U-SIG bandwidth encodings by channel-center sequence.
            // The selected analog-model center frequency supplies the missing
            // placement fact; leave bandwidth unknown for any non-enumerated
            // center instead of choosing a variant.
            if (isEht && ehtBandwidthHz == 320e6) {
                auto channelNumber = static_cast<int>(
                        std::round((frequencyMHz - 5950) / 5));
                auto expectedFrequencyMHz = 5950 + channelNumber * 5;
                int radiotapBandwidth = -1;
                if (std::abs(frequencyMHz - expectedFrequencyMHz) < 1e-6) {
                    if (channelNumber == 31 || channelNumber == 95 || channelNumber == 159)
                        radiotapBandwidth = 4;
                    else if (channelNumber == 63 || channelNumber == 127 || channelNumber == 191)
                        radiotapBandwidth = 5;
                }
                if (radiotapBandwidth >= 0) {
                    uSigCommon |= RADIOTAP_U_SIG_BANDWIDTH_KNOWN;
                    uSigCommon |= radiotapBandwidth << 15;
                }
            }
        }

        auto power = narrowbandAnalogModel->computeMinPower(startTime, endTime);
        double powerMilliwatts = power.get<units::values::mW>();
        if (std::isfinite(powerMilliwatts) && 0 < powerMilliwatts &&
                (direction == DIRECTION_INBOUND || direction == DIRECTION_OUTBOUND)) {
            int powerDbm = static_cast<int>(std::round(math::mW2dBmW(powerMilliwatts)));
            powerDbm = std::clamp(powerDbm, -128, 127);
            fields.hasPower = true;
            fields.power = static_cast<int8_t>(powerDbm);
        }
    }
#endif

#ifdef INET_WITH_IEEE80211
    if (isHe) {
        uint16_t data1 = 0;
        uint16_t data2 = 0;
        uint16_t data3 = 0;
        uint16_t data4 = 0;
        uint16_t data5 = 0;
        uint16_t data6 = 0;

        if (heTxVectorReq != nullptr && heTxVectorReq->getTxVector() != nullptr) {
            const auto& txVector = *heTxVectorReq->getTxVector();
            const auto& common = txVector.getCommon().getParameters();
            auto ppduFormat = common.ppduFormat;
            auto radiotapFormat = getRadiotapHeFormat(ppduFormat);
            data1 |= radiotapFormat | RADIOTAP_HE_BSS_COLOR_KNOWN |
                    RADIOTAP_HE_SPATIAL_REUSE_KNOWN;
            data3 |= common.sigA.bssColor & 0x3f;
            if (ppduFormat == physicallayer::HE_MU_DOWNLINK || ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK) {
                data1 |= RADIOTAP_HE_UL_DL_KNOWN;
                if (ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK)
                    data3 |= 0x0080;
            }
            data2 |= RADIOTAP_HE_GI_KNOWN;
            data4 |= common.sigA.spatialReuse.front() & 0xf;
            data5 |= (common.guardInterval & 0x3) << 4;
            const physicallayer::Ieee80211HeUserPhyParameters *selectedUser = nullptr;
            if (selectedMuUserIndex >= 0 &&
                    selectedMuUserIndex < static_cast<int>(txVector.getUsers().size()))
                selectedUser = &txVector.getUsers()[selectedMuUserIndex].getParameters();
            else if (ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK && !txVector.getUsers().empty())
                selectedUser = &txVector.getUsers().front().getParameters();
            else if (txVector.getUsers().size() == 1)
                selectedUser = &txVector.getUsers().front().getParameters();
            if (selectedUser != nullptr) {
                const auto& user = *selectedUser;
                data1 |= RADIOTAP_HE_DATA_MCS_KNOWN | RADIOTAP_HE_DATA_DCM_KNOWN |
                        RADIOTAP_HE_CODING_KNOWN;
                if (ppduFormat == physicallayer::HE_MU_DOWNLINK) {
                    data1 |= RADIOTAP_HE_MU_STA_ID_KNOWN;
                    data4 |= (user.staId & 0x7ff) << 4;
                }
                data3 |= (user.mcs & 0xf) << 8;
                data3 |= (user.dcm ? 1U : 0U) << 12;
                data3 |= (user.coding & 0x1) << 13;
                auto bandwidthOrRu = ppduFormat == physicallayer::HE_SINGLE_USER ?
                        getRadiotapHeBandwidth(common.channelBandwidth) :
                        getRadiotapHeRuAllocation(user.ru.toneSize);
                if (bandwidthOrRu >= 0) {
                    data1 |= RADIOTAP_HE_BW_RU_ALLOC_KNOWN;
                    data5 |= bandwidthOrRu;
                }
                data6 |= std::clamp<int>(user.numberOfSpatialStreams, 1, 15);
                if (ppduFormat == physicallayer::HE_MU_DOWNLINK) {
                    data6 |= (std::clamp<int>(user.streamStartIndex, 0, 7) & 0x7) << 5;
                }
            }
        }
        else if (heRxVectorInd != nullptr && heRxVectorInd->getRxVector() != nullptr) {
            const auto& rxVector = *heRxVectorInd->getRxVector();
            const auto& common = rxVector.getCommon();
            const auto& user = rxVector.getUser();
            auto ppduFormat = common.getPpduFormat();
            data1 |= getRadiotapHeFormat(ppduFormat) |
                    RADIOTAP_HE_BSS_COLOR_KNOWN | RADIOTAP_HE_SPATIAL_REUSE_KNOWN;
            data3 |= common.getBssColor() & 0x3f;
            if (ppduFormat == physicallayer::HE_MU_DOWNLINK || ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK) {
                data1 |= RADIOTAP_HE_UL_DL_KNOWN;
                if (ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK)
                    data3 |= 0x0080;
            }
            data2 |= RADIOTAP_HE_GI_KNOWN;
            data4 |= common.getSpatialReuse().front() & 0xf;
            data5 |= (common.getGuardInterval() & 0x3) << 4;

            const physicallayer::Ieee80211HeUserPhyParameters *recipientParameters = nullptr;
            if (ppduFormat == physicallayer::HE_TRIGGER_BASED_UPLINK &&
                    heTbRecipientContext != nullptr &&
                    heTbRecipientContext->getRecipientParameters() != nullptr)
                recipientParameters = heTbRecipientContext->getRecipientParameters().get();

            auto mcs = user.getMcs();
            auto dcm = user.getDcm();
            auto coding = user.getCoding();
            auto numberOfSpaceTimeStreams = user.getNumberOfSpaceTimeStreams();
            auto ruAllocation = user.getRuAllocation();
            if (recipientParameters != nullptr) {
                mcs = recipientParameters->mcs;
                dcm = recipientParameters->dcm;
                coding = recipientParameters->coding;
                numberOfSpaceTimeStreams = recipientParameters->numberOfSpatialStreams;
                ruAllocation = recipientParameters->ru;
            }
            if (mcs.has_value()) {
                data1 |= RADIOTAP_HE_DATA_MCS_KNOWN;
                data3 |= (*mcs & 0xf) << 8;
            }
            if (dcm.has_value()) {
                data1 |= RADIOTAP_HE_DATA_DCM_KNOWN;
                data3 |= (*dcm ? 1U : 0U) << 12;
            }
            if (coding.has_value()) {
                data1 |= RADIOTAP_HE_CODING_KNOWN;
                data3 |= (*coding & 0x1) << 13;
            }
            if (numberOfSpaceTimeStreams.has_value())
                data6 |= std::clamp<int>(*numberOfSpaceTimeStreams, 1, 15);
            auto bandwidthOrRu = ppduFormat == physicallayer::HE_SINGLE_USER &&
                    common.getChannelBandwidth().has_value() ?
                    getRadiotapHeBandwidth(*common.getChannelBandwidth()) :
                    (ruAllocation.has_value() ? getRadiotapHeRuAllocation(ruAllocation->toneSize) : -1);
            if (bandwidthOrRu >= 0) {
                data1 |= RADIOTAP_HE_BW_RU_ALLOC_KNOWN;
                data5 |= bandwidthOrRu;
            }
            if (ppduFormat == physicallayer::HE_MU_DOWNLINK && user.getStaId().has_value()) {
                data1 |= RADIOTAP_HE_MU_STA_ID_KNOWN;
                data4 |= (*user.getStaId() & 0x7ff) << 4;
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
        fields.he.data = {data1, data2, data3, data4, data5, data6};
        if (heTxVectorReq != nullptr && heTxVectorReq->getTxVector() != nullptr) {
            const auto& txVector = *heTxVectorReq->getTxVector();
            const auto& common = txVector.getCommon().getParameters();
            if (common.ppduFormat == physicallayer::HE_MU_DOWNLINK) {
                fields.heMuUsers.reserve(txVector.getUsers().size());
                for (const auto& txUser : txVector.getUsers()) {
                    auto userFields = fields.he;
                    const auto& user = txUser.getParameters();
                    userFields.data[0] |= RADIOTAP_HE_DATA_MCS_KNOWN | RADIOTAP_HE_DATA_DCM_KNOWN |
                            RADIOTAP_HE_CODING_KNOWN | RADIOTAP_HE_MU_STA_ID_KNOWN;
                    userFields.data[2] |= (user.mcs & 0xf) << 8;
                    userFields.data[2] |= (user.dcm ? 1U : 0U) << 12;
                    userFields.data[2] |= (user.coding & 0x1) << 13;
                    userFields.data[3] |= (user.staId & 0x7ff) << 4;
                    auto ruAllocation = getRadiotapHeRuAllocation(user.ru.toneSize);
                    if (ruAllocation >= 0) {
                        userFields.data[0] |= RADIOTAP_HE_BW_RU_ALLOC_KNOWN;
                        userFields.data[4] |= ruAllocation;
                    }
                    userFields.data[5] |= std::clamp<int>(user.numberOfSpatialStreams, 1, 15);
                    userFields.data[5] |= (std::clamp<int>(user.streamStartIndex, 0, 7) & 0x7) << 5;
                    fields.heMuUsers.push_back(userFields);
                }
            }
        }
    }

    // 12. RADIOTAP_HE_MU (24)
    if (isHe && heTxVectorReq != nullptr && heTxVectorReq->getTxVector() != nullptr) {
        const auto& txVector = *heTxVectorReq->getTxVector();
        const auto& common = txVector.getCommon().getParameters();
        if (common.ppduFormat == physicallayer::HE_MU_DOWNLINK) {
            fields.hasHeMu = true;
            uint8_t ruChannel1[4] = {0};

            if (!txVector.getUsers().empty()) {
                const auto& firstUser = txVector.getUsers().front().getParameters();
                int ruAlloc = getRadiotapHeRuAllocation(firstUser.ru.toneSize);
                if (ruAlloc >= 0)
                    ruChannel1[0] = static_cast<uint8_t>(ruAlloc);
            }
            std::copy(std::begin(ruChannel1), std::end(ruChannel1), fields.heMuRuChannel1.begin());
        }
    }
#endif
    return fields;
}

std::vector<uint8_t> serializeRadiotapHeader(const RadiotapPpduFields& fields, const RadiotapRecordMetadata& metadata)
{
    std::vector<uint32_t> presentWords;
    auto setPresentBit = [&](int bitIndex) {
        auto wordIndex = bitIndex / 32;
        auto bitOffset = bitIndex % 32;
        ASSERT(bitOffset != 31);
        if (wordIndex >= static_cast<int>(presentWords.size()))
            presentWords.resize(wordIndex + 1, 0);
        presentWords[wordIndex] |= 1U << bitOffset;
    };
    std::vector<uint8_t> bytes(4, 0);

    setPresentBit(RADIOTAP_FLAGS);
    bytes.push_back(fields.flags);
    if (fields.hasRate) {
        setPresentBit(RADIOTAP_RATE);
        bytes.push_back(fields.rate);
    }
    if (fields.hasChannel) {
        setPresentBit(RADIOTAP_CHANNEL);
        appendPadding(bytes, 2);
        appendUint16(bytes, fields.channelFrequency);
        appendUint16(bytes, fields.channelFlags);
    }
    if (fields.hasPower) {
        setPresentBit(fields.direction == DIRECTION_INBOUND ? RADIOTAP_ANTENNA_SIGNAL : RADIOTAP_DBM_TX_POWER);
        bytes.push_back(static_cast<uint8_t>(fields.power));
    }
    if (fields.direction == DIRECTION_INBOUND) {
        setPresentBit(RADIOTAP_RX_FLAGS);
        appendPadding(bytes, 2);
        appendUint16(bytes, 0);
    }
    else if (fields.direction == DIRECTION_OUTBOUND) {
        setPresentBit(RADIOTAP_TX_FLAGS);
        appendPadding(bytes, 2);
        appendUint16(bytes, 0);
    }
#ifdef INET_WITH_IEEE80211
    if (fields.isHt) {
        setPresentBit(RADIOTAP_MCS);
        bytes.insert(bytes.end(), fields.mcs.begin(), fields.mcs.end());
    }
    if (metadata.isAmpdu) {
        setPresentBit(RADIOTAP_AMPDU);
        appendPadding(bytes, 4);
        appendUint32(bytes, metadata.ampduReference);
        appendUint16(bytes, 0x0004 | (metadata.isLastSubframe ? 0x0008 : 0));
        bytes.push_back(0);
        bytes.push_back(0);
    }
    if (fields.isVht) {
        setPresentBit(RADIOTAP_VHT);
        appendPadding(bytes, 2);
        appendUint16(bytes, fields.vhtKnown);
        bytes.push_back(fields.vhtFlags);
        bytes.push_back(fields.vhtBandwidth);
        bytes.insert(bytes.end(), fields.vhtMcsNss.begin(), fields.vhtMcsNss.end());
        bytes.push_back(fields.vhtCoding);
        bytes.push_back(fields.vhtGroupId);
        appendUint16(bytes, fields.vhtPartialAid);
    }
    if (fields.isHe) {
        setPresentBit(RADIOTAP_HE);
        appendPadding(bytes, 2);
        const auto *he = &fields.he;
        if (metadata.muUserIndex >= 0 && metadata.muUserIndex < static_cast<int>(fields.heMuUsers.size()))
            he = &fields.heMuUsers[metadata.muUserIndex];
        for (auto value : he->data)
            appendUint16(bytes, value);
    }
    if (fields.hasHeMu) {
        setPresentBit(RADIOTAP_HE_MU);
        appendPadding(bytes, 2);
        appendUint16(bytes, 0x0001);
        appendUint16(bytes, 0x0001);
        bytes.insert(bytes.end(), fields.heMuRuChannel1.begin(), fields.heMuRuChannel1.end());
        bytes.insert(bytes.end(), 4, 0);
    }
    if (metadata.zeroLength) {
        setPresentBit(RADIOTAP_0_LENGTH_PSDU);
        bytes.push_back(0);
    }
    if (fields.isEht && fields.uSigCommon != 0) {
        setPresentBit(RADIOTAP_U_SIG);
        appendPadding(bytes, 4);
        appendUint32(bytes, fields.uSigCommon);
        appendUint32(bytes, 0);
        appendUint32(bytes, 0);
    }
    if (fields.isEht) {
        setPresentBit(RADIOTAP_EHT);
        appendPadding(bytes, 4);
        appendUint32(bytes, fields.ehtKnown);
        for (auto value : fields.ehtData)
            appendUint32(bytes, value);
        for (auto value : fields.ehtUserInfo)
            appendUint32(bytes, value);
    }
#endif
    for (size_t i = 0; i + 1 < presentWords.size(); i++)
        presentWords[i] |= 1U << 31;
    std::vector<uint8_t> presentBytes(4 * presentWords.size());
    for (size_t i = 0; i < presentWords.size(); i++)
        setUint32(presentBytes, 4 * i, presentWords[i]);
    bytes.insert(bytes.begin() + 4, presentBytes.begin(), presentBytes.end());
    setUint16(bytes, 2, bytes.size());
    return bytes;
}

} // namespace

namespace ieee80211 {

Register_Pcap_Capture_Adapter(&Protocol::ieee80211Mac, Ieee80211RadiotapPcapCaptureAdapter);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211FhssPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211IrPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211DsssPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211HrDsssPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211OfdmPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211ErpOfdmPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211HtPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211VhtPhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211HePhy, &Protocol::ieee80211Mac);
Register_Pcap_Capture_Protocol_Resolver(&Protocol::ieee80211EhtPhy, &Protocol::ieee80211Mac);

std::optional<std::pair<b, b>> Ieee80211RadiotapPcapCaptureAdapter::tryResolvePacket(const Packet *packet, b frontOffset, b backOffset) const
{
    const int parsingFlags = Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    Packet packetCopy(*packet);
    packetCopy.setFrontOffset(packetCopy.getFrontOffset() + frontOffset);
    packetCopy.setBackOffset(packetCopy.getBackOffset() - backOffset);
    try {
        auto header = physicallayer::Ieee80211Radio::popIeee80211PhyHeaderAtFront(&packetCopy, b(-1), parsingFlags);
        if (header->isIncorrect() || header->isIncomplete() || header->isImproperlyRepresented() ||
                b(header->getLengthField()) < header->getChunkLength())
            return std::nullopt;
        auto resolvedFrontOffset = frontOffset + header->getChunkLength();
        auto payloadLength = b(header->getLengthField());
        auto availablePayloadLength = packet->getDataLength() - resolvedFrontOffset - backOffset;
        if (payloadLength > availablePayloadLength)
            return std::nullopt;
        auto resolvedBackOffset = packet->getDataLength() - resolvedFrontOffset - payloadLength;
        return std::pair<b, b>(resolvedFrontOffset, resolvedBackOffset);
    }
    catch (cRuntimeError&) {
        return std::nullopt;
    }
}

std::vector<PcapCaptureRecord> Ieee80211RadiotapPcapCaptureAdapter::createRecords(const PcapCaptureObservation& observation, b frontOffset, b backOffset) const
{
    auto packet = observation.packet;
    auto transmission = dynamic_cast<const physicallayer::ITransmission *>(observation.transmission);
    auto reception = dynamic_cast<const physicallayer::IReception *>(observation.reception);
    const auto ppduFields = extractRadiotapPpduFields(packet, observation.direction, transmission, reception);
    std::vector<MpduRange> mpduRanges;
    if (getIeee80211VhtMuAmpduMpduRanges(packet, frontOffset, backOffset, mpduRanges) ||
            getIeee80211HeMuAmpduMpduRanges(packet, frontOffset, backOffset, mpduRanges) ||
            getIeee80211AmpduMpduRanges(packet, frontOffset, backOffset, mpduRanges)) {
        std::vector<PcapCaptureRecord> records;
        records.reserve(mpduRanges.size());
        for (size_t i = 0; i < mpduRanges.size(); i++) {
            const auto& mpduRange = mpduRanges[i];
            auto recordBackOffset = packet->getDataLength() - mpduRange.offset - mpduRange.length;
            RadiotapRecordMetadata metadata;
            metadata.muUserIndex = mpduRange.muUserIndex;
            metadata.isAmpdu = true;
            metadata.isLastSubframe = i == mpduRanges.size() - 1 || mpduRanges[i + 1].muUserIndex != mpduRange.muUserIndex;
            metadata.ampduReference = makeAmpduReference(packet, metadata.muUserIndex);
            records.emplace_back(mpduRange.offset, recordBackOffset,
                    serializeRadiotapHeader(ppduFields, metadata), true);
        }
        return records;
    }

    RadiotapRecordMetadata metadata;
    auto matchContainingRange = [&](const std::vector<MpduRange>& ranges) {
        for (size_t i = 0; i < ranges.size(); i++) {
            if (ranges[i].offset == frontOffset) {
                metadata.muUserIndex = ranges[i].muUserIndex;
                metadata.isAmpdu = true;
                metadata.isLastSubframe = i == ranges.size() - 1 || ranges[i + 1].muUserIndex != ranges[i].muUserIndex;
                metadata.ampduReference = makeAmpduReference(packet, metadata.muUserIndex);
                return true;
            }
        }
        return false;
    };
    std::vector<MpduRange> containingRanges;
    if (findIeee80211VhtMuAmpduMpduRanges(packet, frontOffset, containingRanges))
        matchContainingRange(containingRanges);
    if (!metadata.isAmpdu) {
        containingRanges.clear();
        if (findIeee80211HeMuAmpduMpduRanges(packet, frontOffset, containingRanges))
            matchContainingRange(containingRanges);
    }
    if (!metadata.isAmpdu) {
        auto startOffsetResidue = b(frontOffset.get<b>() % 8);
        auto backOffsetResidue = b(backOffset.get<b>() % 8);
        for (b startOffset = startOffsetResidue; startOffset <= frontOffset && !metadata.isAmpdu; startOffset += b(8)) {
            for (b candidateBackOffset = backOffsetResidue; candidateBackOffset <= backOffset; candidateBackOffset += b(8)) {
                containingRanges.clear();
                if (getIeee80211AmpduMpduRanges(packet, startOffset, candidateBackOffset, containingRanges) && matchContainingRange(containingRanges))
                    break;
            }
        }
    }
    metadata.zeroLength = packet->getDataLength() - frontOffset - backOffset == b(0);
    return {PcapCaptureRecord(frontOffset, backOffset,
            serializeRadiotapHeader(ppduFields, metadata))};
}

} // namespace ieee80211
} // namespace inet
