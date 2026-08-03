//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <tuple>

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211OfdmSignalField.h"

// IEEE 802.11 PHY header serializer, including HE headers.
//
// This file serializes/deserializes the logical L-SIG, RL-SIG, HE-SIG-A, and
// HE-SIG-B fields used by the packet-level model. Validated HE SU and HE ER SU
// headers use their exact 100 logical signaling bits. The represented HE MU
// and HE TB bit layouts follow
// IEEE 802.11-2024 Tables 27-21 (HE MU HE-SIG-A), 27-22 (HE TB HE-SIG-A) and
// Clause 27.3.11.8 (HE-SIG-B).
//
// Approximations / simplifications:
//   - Several reserved/constant fields are written as fixed values (e.g. TXOP
//     = 127, HE-SIG-B MCS = 0, DCM = false, Doppler = false) rather than being
//     derived from the actual transmission parameters.
//   - CRC, tail and padding bits in HE-SIG-A/B are written as zeros; the model
//     does not perform bit-level CRC verification on the preamble.
//   - HE-SIG-B user-specific fields are encoded with a simplified fixed-length
//     representation; the full User field format of Tables 27-29 and 27-30 is
//     approximated.

// This is a logical-field representation, not a BCC/interleaver/modulation
// bitstream. CRC/tail and deterministic logical padding are represented.

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211OfdmSignalField.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {

namespace  physicallayer {

namespace {

// IEEE Std 802.11-2024 Figure 17-5 and 17.3.4: SIGNAL bit 0 is RATE bit R1
// and the LSB is transmitted first; the shared field helper packs that order.

void writeUnsignedLogicalField(MemoryOutputStream& stream, uint32_t value, int width)
{
    for (int bit = 0; bit < width; bit++)
        stream.writeBit((value >> bit) & 1);
}

uint32_t readUnsignedLogicalField(MemoryInputStream& stream, int width)
{
    uint32_t value = 0;
    for (int bit = 0; bit < width; bit++)
        if (stream.readBit())
            value |= 1u << bit;
    return value;
}

class Ieee80211HeSuPhyHeaderSerializer : public Ieee80211HePhyHeaderSerializer
{
  public:
    Ieee80211HeSuPhyHeaderSerializer() : Ieee80211HePhyHeaderSerializer(HE_SINGLE_USER) {}
};

class Ieee80211HeErSuPhyHeaderSerializer : public Ieee80211HePhyHeaderSerializer
{
  public:
    Ieee80211HeErSuPhyHeaderSerializer() : Ieee80211HePhyHeaderSerializer(HE_EXTENDED_RANGE_SU) {}
};

class Ieee80211HeMuPhyHeaderSerializer : public Ieee80211HePhyHeaderSerializer
{
  public:
    Ieee80211HeMuPhyHeaderSerializer() : Ieee80211HePhyHeaderSerializer(HE_MU_DOWNLINK) {}
};

class Ieee80211HeTbPhyHeaderSerializer : public Ieee80211HePhyHeaderSerializer
{
  public:
    Ieee80211HeTbPhyHeaderSerializer() : Ieee80211HePhyHeaderSerializer(HE_TRIGGER_BASED_UPLINK) {}
};

std::vector<bool> readLogicalBits(MemoryInputStream& stream, size_t count)
{
    std::vector<bool> bits;
    bits.reserve(count);
    for (size_t i = 0; i < count; ++i)
        bits.push_back(stream.readBit());
    return bits;
}

Hz getHeBandwidth(uint8_t bandwidth)
{
    if (bandwidth == 0) return MHz(20);
    if (bandwidth == 1) return MHz(40);
    if (bandwidth == 2 || bandwidth == 4 || bandwidth == 5) return MHz(80);
    if (bandwidth == 3 || bandwidth == 6 || bandwidth == 7) return MHz(160);
    throw cRuntimeError("Reserved HE MU bandwidth field");
}

int getHeSigBDataBitsPerSymbol(uint8_t mcs, bool dcm)
{
    static const int noDcm[] = {26, 52, 78, 104, 156, 208};
    static const int withDcm[] = {13, 26, 0, 52, 78, 0};
    if (mcs > 5 || (dcm && withDcm[mcs] == 0))
        throw cRuntimeError("Reserved HE-SIG-B MCS/DCM combination");
    return dcm ? withDcm[mcs] : noDcm[mcs];
}

std::pair<uint8_t, bool> decodeHeMuPuncturing(uint8_t bandwidthCode,
        const Ieee80211HeSigBCommonField& commonField, Hz channelBandwidth)
{
    int subchannelCount = std::lround(channelBandwidth.get() / 20e6);
    std::vector<uint8_t> candidates;
    for (int mask = 0; mask < (1 << subchannelCount); ++mask) {
        std::vector<bool> punctured(subchannelCount, false);
        for (int subchannel = 0; subchannel < subchannelCount; ++subchannel)
            punctured[subchannel] = mask & (1 << subchannel);
        auto encoded = encodeHeMuBandwidth(channelBandwidth, punctured, false);
        if (!encoded || encoded.value != bandwidthCode)
            continue;
        bool commonCompatible = true;
        for (int subchannel = 0; subchannel < subchannelCount; ++subchannel) {
            if (!punctured[subchannel])
                continue;
            int channel = subchannel % 2;
            int field = subchannel / 2;
            uint8_t allocation = commonField.contentChannels[channel].ruAllocationSubfields[field];
            if (allocation != 113 && allocation != 114) {
                commonCompatible = false;
                break;
            }
        }
        if (commonCompatible)
            candidates.push_back(mask);
    }
    if (candidates.empty())
        throw cRuntimeError("HE MU bandwidth code and Common allocation have no legal puncturing interpretation");
    return candidates.size() == 1 ? std::make_pair(candidates.front(), true) :
            std::make_pair(uint8_t(0), false);
}

template<typename T>
void appendBits(std::vector<bool>& target, const T& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

std::vector<std::vector<Ieee80211HeMuUserInfo>> orderHeSigBUsers(
        const Ieee80211HePhyHeader& header, const Ieee80211HeSigBCommonField *commonField,
        Hz channelBandwidth, bool compression)
{
    int channelCount = channelBandwidth == MHz(20) ? 1 : 2;
    std::vector<std::vector<Ieee80211HeMuUserInfo>> channels(channelCount);
    std::map<std::pair<int, int>, std::vector<Ieee80211HeMuUserInfo>> usersByRu;
    for (unsigned int i = 0; i < header.getUsersArraySize(); ++i)
        usersByRu[{header.getUsers(i).ruToneSize, header.getUsers(i).ruToneOffset}].push_back(header.getUsers(i));
    for (auto& entry : usersByRu)
        std::sort(entry.second.begin(), entry.second.end(), [] (const auto& left, const auto& right) {
            return left.streamStartIndex < right.streamStartIndex;
        });
    if (compression) {
        if (channelBandwidth != MHz(160) || usersByRu.size() != 1 ||
                usersByRu.begin()->first.first != 1992)
            throw cRuntimeError("Compressed HE-SIG-B requires one full-bandwidth 2x996-tone RU");
        const auto& users = usersByRu.begin()->second;
        size_t channel1Count = (users.size() + 1) / 2;
        channels[0].insert(channels[0].end(), users.begin(), users.begin() + channel1Count);
        if (channelCount == 2)
            channels[1].insert(channels[1].end(), users.begin() + channel1Count, users.end());
        return channels;
    }

    auto subchannels = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    subchannels.erase(std::remove_if(subchannels.begin(), subchannels.end(),
            [] (const auto& ru) { return ru.toneSize != 242; }), subchannels.end());
    std::sort(subchannels.begin(), subchannels.end(), [] (const auto& left, const auto& right) {
        return left.toneOffset < right.toneOffset;
    });
    std::set<std::pair<int, int>> emittedWide[2];
    for (size_t s = 0; s < subchannels.size(); ++s) {
        int channel = s % 2;
        std::vector<Ieee80211HeRu> local;
        for (const auto& ru : commonField->rus) {
            bool belongs = ru.toneSize > 242 ?
                    ru.toneOffset <= subchannels[s].toneOffset &&
                    ru.toneOffset + ru.toneSize >= subchannels[s].toneOffset + 242 :
                    ru.toneOffset >= subchannels[s].toneOffset &&
                    ru.toneOffset + ru.toneSize <= subchannels[s].toneOffset + 242;
            if (belongs && std::none_of(local.begin(), local.end(), [&] (const auto& value) {
                    return value.toneSize == ru.toneSize && value.toneOffset == ru.toneOffset;
                }))
                local.push_back(ru);
        }
        std::sort(local.begin(), local.end(), [] (const auto& left, const auto& right) {
            return left.toneOffset < right.toneOffset;
        });
        for (const auto& ru : local) {
            const auto& users = usersByRu[{ru.toneSize, ru.toneOffset}];
            if (ru.toneSize > 242) {
                auto geometry = std::make_pair(ru.toneSize, ru.toneOffset);
                if (!emittedWide[channel].insert(geometry).second)
                    continue;
                size_t channel1Count = (users.size() + 1) / 2;
                auto first = channel == 0 ? users.begin() : users.begin() + channel1Count;
                auto last = channel == 0 ? users.begin() + channel1Count : users.end();
                channels[channel].insert(channels[channel].end(), first, last);
            }
            else
                channels[channel].insert(channels[channel].end(), users.begin(), users.end());
        }
    }
    for (const auto& entry : usersByRu) {
        if (entry.first == std::make_pair(26, 485))
            channels[0].insert(channels[0].end(), entry.second.begin(), entry.second.end());
        else if (entry.first == std::make_pair(26, 1481))
            channels[1].insert(channels[1].end(), entry.second.begin(), entry.second.end());
    }
    return channels;
}

struct HeSigBLogicalField
{
    std::vector<std::vector<bool>> channels;
    int numberOfSymbols = 0;
};

HeSigBLogicalField buildHeSigBLogicalField(const Ieee80211HePhyHeader& header,
        const Ieee80211HeMuSignalingFields& signaling)
{
    HeSigBLogicalField result;
    Hz channelBandwidth = getHeBandwidth(signaling.bandwidth);
    int channelCount = channelBandwidth == MHz(20) ? 1 : 2;
    result.channels.resize(channelCount);
    std::vector<Ieee80211HeRu> rus;
    for (unsigned int i = 0; i < header.getUsersArraySize(); ++i) {
        Ieee80211HeRu ru;
        ru.toneSize = header.getUsers(i).ruToneSize;
        ru.toneOffset = header.getUsers(i).ruToneOffset;
        rus.push_back(ru);
    }
    std::vector<bool> punctured(std::lround(channelBandwidth.get() / 20e6), false);
    for (size_t i = 0; i < punctured.size(); ++i)
        punctured[i] = header.getPuncturedSubchannelMask() & (uint8_t(1) << i);
    auto bandwidth = encodeHeMuBandwidth(channelBandwidth, punctured, signaling.heSigBCompression);
    if (!bandwidth || bandwidth.value != signaling.bandwidth)
        throw cRuntimeError("HE MU bandwidth/puncturing signaling is inconsistent");
    Ieee80211HeSigBCommonFieldResult common;
    if (!signaling.heSigBCompression) {
        common = encodeHeSigBCommonField(rus, channelBandwidth, punctured);
        if (!common)
            throw cRuntimeError("Cannot serialize HE-SIG-B RU allocation: %s", common.error.c_str());
        for (int channel = 0; channel < channelCount; ++channel) {
            Ieee80211HeSigBCommonBlock block;
            block.ruAllocationSubfields = common.commonField.contentChannels[channel].ruAllocationSubfields;
            block.center26ToneRuBitPresent = channelBandwidth > MHz(40);
            block.hasCenter26ToneRu = common.commonField.contentChannels[channel].hasCenterRu;
            auto encoded = encodeHeSigBCommonBlock(block);
            if (!encoded)
                throw cRuntimeError("Cannot serialize HE-SIG-B Common block: %s", encoded.error.c_str());
            appendBits(result.channels[channel], encoded.bits);
        }
    }
    auto orderedUsers = orderHeSigBUsers(header,
            signaling.heSigBCompression ? nullptr : &common.commonField,
            channelBandwidth, signaling.heSigBCompression);
    if (!signaling.heSigBCompression) {
        std::map<std::pair<int, int>, std::vector<Ieee80211HeMuUserInfo>> usersByRu;
        std::map<std::pair<int, int>, size_t> nextUser;
        for (unsigned int i = 0; i < header.getUsersArraySize(); ++i)
            usersByRu[{header.getUsers(i).ruToneSize, header.getUsers(i).ruToneOffset}].push_back(header.getUsers(i));
        for (auto& entry : usersByRu)
            std::sort(entry.second.begin(), entry.second.end(), [] (const auto& left, const auto& right) {
                return left.streamStartIndex < right.streamStartIndex;
            });
        orderedUsers.assign(channelCount, {});
        for (int channel = 0; channel < channelCount; ++channel) {
            for (const auto& planned : common.commonField.contentChannels[channel].plannedUsers) {
                auto geometry = std::make_pair(planned.ru.toneSize, planned.ru.toneOffset);
                if (planned.unallocated) {
                    Ieee80211HeMuUserInfo placeholder;
                    placeholder.ruIndex = planned.ru.index;
                    placeholder.ruToneSize = planned.ru.toneSize;
                    placeholder.ruToneOffset = planned.ru.toneOffset;
                    placeholder.staId = 2046;
                    orderedUsers[channel].push_back(placeholder);
                }
                else {
                    auto users = usersByRu.find(geometry);
                    if (users == usersByRu.end() || nextUser[geometry] >= users->second.size())
                        throw cRuntimeError("HE-SIG-B plan has no canonical user for an allocated RU");
                    orderedUsers[channel].push_back(users->second[nextUser[geometry]++]);
                }
            }
        }
        for (const auto& entry : usersByRu)
            if (nextUser[entry.first] != entry.second.size())
                throw cRuntimeError("HE-SIG-B plan did not consume every canonical scheduled user");
    }
    for (int channel = 0; channel < channelCount; ++channel) {
        std::vector<std::vector<bool>> pending;
        for (const auto& user : orderedUsers[channel]) {
            Ieee80211HeSigBBitsResult encoded;
            if (user.muMimo || (signaling.heSigBCompression && header.getUsersArraySize() > 1)) {
                Ieee80211HeSigBMuMimoUser value;
                value.staId = user.staId;
                value.spatialConfiguration = user.spatialConfiguration;
                value.mcs = user.mcs;
                value.ldpcCoding = user.coding == HE_CODING_LDPC;
                encoded = encodeHeSigBMuMimoUser(value);
            }
            else {
                Ieee80211HeSigBNonMuMimoUser value;
                value.staId = user.staId;
                value.numberOfSpaceTimeStreams = user.numberOfSpatialStreams;
                value.mcs = user.mcs;
                value.dcm = user.dcm;
                value.ldpcCoding = user.coding == HE_CODING_LDPC;
                encoded = encodeHeSigBNonMuMimoUser(value);
            }
            if (!encoded)
                throw cRuntimeError("Cannot serialize HE-SIG-B User field: %s", encoded.error.c_str());
            pending.push_back(encoded.bits);
            if (pending.size() == 2) {
                auto block = encodeHeSigBUserBlock(pending);
                appendBits(result.channels[channel], block.bits);
                pending.clear();
            }
        }
        if (!pending.empty()) {
            auto block = encodeHeSigBUserBlock(pending);
            appendBits(result.channels[channel], block.bits);
        }
    }
    int dataBitsPerSymbol = getHeSigBDataBitsPerSymbol(signaling.heSigBMcs, signaling.heSigBDcm);
    size_t maximumBits = 0;
    for (const auto& channel : result.channels)
        maximumBits = std::max(maximumBits, channel.size());
    result.numberOfSymbols = std::max<int>(1, (maximumBits + dataBitsPerSymbol - 1) / dataBitsPerSymbol);
    if (!signaling.heSigBCompression &&
            (signaling.numberOfHeSigBSymbols != result.numberOfSymbols ||
             signaling.numberOfHeSigBSymbolsIsSaturated != (result.numberOfSymbols == 16)))
        throw cRuntimeError("HE-SIG-A HE-SIG-B symbol count disagrees with represented logical blocks");
    if (signaling.heSigBCompression && signaling.numberOfMuMimoUsers != header.getUsersArraySize())
        throw cRuntimeError("Compressed HE-SIG-A user count disagrees with HE-SIG-B User fields");
    for (auto& channel : result.channels)
        channel.resize(result.numberOfSymbols * dataBitsPerSymbol, false);
    return result;
}

struct HeSigBAllocation
{
    Ieee80211HeRu ru;
    int userCount = 0;
};

std::vector<std::vector<HeSigBAllocation>> decodeHeSigBAllocations(
        const Ieee80211HeSigBCommonField& commonField,
        const Ieee80211HeSigBCommonField& decodedField, Hz channelBandwidth)
{
    std::vector<std::vector<HeSigBAllocation>> channels(channelBandwidth == MHz(20) ? 1 : 2);
    auto subchannels = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    subchannels.erase(std::remove_if(subchannels.begin(), subchannels.end(),
            [] (const auto& ru) { return ru.toneSize != 242; }), subchannels.end());
    std::sort(subchannels.begin(), subchannels.end(), [] (const auto& left, const auto& right) {
        return left.toneOffset < right.toneOffset;
    });
    std::map<std::pair<int, int>, int> wideCounts;
    std::set<std::pair<int, int>> countedWide[2];
    for (size_t s = 0; s < subchannels.size(); ++s) {
        int channel = s % 2;
        int field = s / 2;
        uint8_t code = commonField.contentChannels[channel].ruAllocationSubfields[field];
        if (code == 115)
            continue;
        std::vector<std::pair<int, int>> localRus;
        std::vector<int> counts;
        if (!decodeTable27_27(code, localRus, counts))
            throw cRuntimeError("Reserved HE-SIG-B RU allocation code");
        for (size_t i = 0; i < localRus.size(); ++i) {
            if (localRus[i].first <= 242)
                continue;
            auto ru = std::find_if(decodedField.rus.begin(), decodedField.rus.end(), [&] (const auto& value) {
                return value.toneSize == localRus[i].first &&
                        value.toneOffset <= subchannels[s].toneOffset &&
                        value.toneOffset + value.toneSize >= subchannels[s].toneOffset + 242;
            });
            if (ru != decodedField.rus.end() && countedWide[channel].insert({ru->toneSize, ru->toneOffset}).second)
                wideCounts[{ru->toneSize, ru->toneOffset}] += counts[i];
        }
    }
    std::set<std::pair<int, int>> emittedWide[2];
    for (size_t s = 0; s < subchannels.size(); ++s) {
        int channel = s % 2;
        std::vector<Ieee80211HeRu> local;
        for (const auto& ru : decodedField.rus) {
            bool belongs = ru.toneSize > 242 ?
                    ru.toneOffset <= subchannels[s].toneOffset &&
                    ru.toneOffset + ru.toneSize >= subchannels[s].toneOffset + 242 :
                    ru.toneOffset >= subchannels[s].toneOffset &&
                    ru.toneOffset + ru.toneSize <= subchannels[s].toneOffset + 242;
            if (belongs && std::none_of(local.begin(), local.end(), [&] (const auto& value) {
                    return value.toneSize == ru.toneSize && value.toneOffset == ru.toneOffset;
                }))
                local.push_back(ru);
        }
        std::sort(local.begin(), local.end(), [] (const auto& left, const auto& right) {
            return left.toneOffset < right.toneOffset;
        });
        for (const auto& ru : local) {
            if (ru.toneSize > 242) {
                auto geometry = std::make_pair(ru.toneSize, ru.toneOffset);
                if (!emittedWide[channel].insert(geometry).second)
                    continue;
                int total = wideCounts[geometry];
                int count = channel == 0 ? (total + 1) / 2 : total / 2;
                if (count)
                    channels[channel].push_back({ru, count});
            }
            else {
                int count = std::count_if(decodedField.rus.begin(), decodedField.rus.end(), [&] (const auto& value) {
                    return value.toneSize == ru.toneSize && value.toneOffset == ru.toneOffset;
                });
                channels[channel].push_back({ru, count});
            }
        }
    }
    for (const auto& ru : decodedField.rus) {
        if (ru.toneSize == 26 && ru.toneOffset == 485)
            channels[0].push_back({ru, 1});
        else if (ru.toneSize == 26 && ru.toneOffset == 1481)
            channels[1].push_back({ru, 1});
    }
    return channels;
}

std::vector<int> decodeHeMuSpatialConfiguration(int userCount, uint8_t code)
{
    static const std::vector<std::vector<std::vector<int>>> configurations = {
        {}, {},
        {{1,1},{2,1},{3,1},{4,1},{2,2},{3,2},{4,2},{3,3},{4,3},{4,4}},
        {{1,1,1},{2,1,1},{3,1,1},{4,1,1},{2,2,1},{3,2,1},{4,2,1},{3,3,1},{4,3,1},{2,2,2},{3,2,2},{4,2,2},{3,3,2}},
        {{1,1,1,1},{2,1,1,1},{3,1,1,1},{4,1,1,1},{2,2,1,1},{3,2,1,1},{4,2,1,1},{3,3,1,1},{2,2,2,1},{3,2,2,1},{2,2,2,2}},
        {{1,1,1,1,1},{2,1,1,1,1},{3,1,1,1,1},{4,1,1,1,1},{2,2,1,1,1},{3,2,1,1,1},{2,2,2,1,1}},
        {{1,1,1,1,1,1},{2,1,1,1,1,1},{3,1,1,1,1,1},{2,2,1,1,1,1}},
        {{1,1,1,1,1,1,1},{2,1,1,1,1,1,1}},
        {{1,1,1,1,1,1,1,1}}
    };
    if (userCount < 2 || userCount > 8 || code >= configurations[userCount].size())
        throw cRuntimeError("Reserved HE-SIG-B MU-MIMO spatial configuration");
    return configurations[userCount][code];
}

template<typename SigA>
SigA makeHeSuErSigA(const Ieee80211HeSuErSignalingFields& fields)
{
    SigA sigA;
    sigA.beamChange = fields.beamChange;
    sigA.uplink = fields.uplink;
    sigA.mcs = fields.mcs;
    sigA.dcm = fields.dcm;
    sigA.bssColor = fields.bssColor;
    sigA.spatialReuse = fields.spatialReuse;
    sigA.bandwidth = fields.bandwidth;
    sigA.giLtfSize = fields.giLtfSize;
    sigA.numberOfSpaceTimeStreams = fields.numberOfSpaceTimeStreams;
    sigA.midamblePeriodicity = fields.midamblePeriodicity;
    sigA.txop = fields.txop;
    sigA.ldpcCoding = fields.ldpcCoding;
    sigA.ldpcExtraSymbolSegment = fields.ldpcExtraSymbolSegment;
    sigA.stbc = fields.stbc;
    sigA.beamformed = fields.beamformed;
    sigA.preFecPaddingFactor = fields.preFecPaddingFactor;
    sigA.peDisambiguity = fields.peDisambiguity;
    sigA.doppler = fields.doppler;
    return sigA;
}

template<typename SigA>
Ieee80211HeSuErSignalingFields makeHeSuErSignalingFields(uint16_t lSigLength, const SigA& sigA)
{
    Ieee80211HeSuErSignalingFields fields;
    fields.signalingValid = true;
    fields.lSigLength = lSigLength;
    fields.beamChange = sigA.beamChange;
    fields.uplink = sigA.uplink;
    fields.mcs = sigA.mcs;
    fields.dcm = sigA.dcm;
    fields.bssColor = sigA.bssColor;
    fields.spatialReuse = sigA.spatialReuse;
    fields.bandwidth = sigA.bandwidth;
    fields.giLtfSize = sigA.giLtfSize;
    fields.numberOfSpaceTimeStreams = sigA.numberOfSpaceTimeStreams;
    fields.midamblePeriodicity = sigA.midamblePeriodicity;
    fields.txop = sigA.txop;
    fields.ldpcCoding = sigA.ldpcCoding;
    fields.ldpcExtraSymbolSegment = sigA.ldpcExtraSymbolSegment;
    fields.stbc = sigA.stbc;
    fields.beamformed = sigA.beamformed;
    fields.preFecPaddingFactor = sigA.preFecPaddingFactor;
    fields.peDisambiguity = sigA.peDisambiguity;
    fields.doppler = sigA.doppler;
    return fields;
}

bool serializeExactHeSuEr(MemoryOutputStream& stream, const Ptr<const Ieee80211HePhyHeader>& header,
        Ieee80211HePpduFormat ppduFormat)
{
    const Ieee80211HeSuErSignalingFields *fields = nullptr;
    Ieee80211HeSigFormat sigFormat;
    Ieee80211HeSigABitsResult sigA;
    if (ppduFormat == HE_SINGLE_USER) {
        fields = &dynamicPtrCast<const Ieee80211HeSuPhyHeader>(header)->getSignaling();
        if (!fields->signalingValid)
            return false;
        sigFormat = Ieee80211HeSigFormat::SU;
        sigA = encodeHeSuSigA(makeHeSuErSigA<Ieee80211HeSuSigA>(*fields));
    }
    else if (ppduFormat == HE_EXTENDED_RANGE_SU) {
        fields = &dynamicPtrCast<const Ieee80211HeErSuPhyHeader>(header)->getSignaling();
        if (!fields->signalingValid)
            return false;
        sigFormat = Ieee80211HeSigFormat::ER_SU;
        sigA = encodeHeErSuSigA(makeHeSuErSigA<Ieee80211HeErSuSigA>(*fields));
    }
    else
        return false;
    Ieee80211HeLSig lSig;
    lSig.length = fields->lSigLength;
    auto lSigBits = encodeHeLSig(lSig, sigFormat);
    auto rlSigBits = encodeHeRlSig(lSig, sigFormat);
    if (!lSigBits)
        throw cRuntimeError("Cannot serialize HE SU/ER L-SIG: %s", lSigBits.error.c_str());
    if (!rlSigBits)
        throw cRuntimeError("Cannot serialize HE SU/ER RL-SIG: %s", rlSigBits.error.c_str());
    if (!sigA)
        throw cRuntimeError("Cannot serialize HE SU/ER HE-SIG-A: %s", sigA.error.c_str());
    stream.writeBits(lSigBits.bits);
    stream.writeBits(rlSigBits.bits);
    stream.writeBits(sigA.bits);
    return true;
}

Ptr<Ieee80211HePhyHeader> deserializeExactHeSuEr(MemoryInputStream& stream,
        const std::optional<Ieee80211HePpduFormat>& expectedPpduFormat)
{
    auto startPosition = stream.getPosition();
    if (stream.getRemainingLength() < b(100))
        return nullptr;
    auto lSigBits = readLogicalBits(stream, 24);
    auto suLSig = decodeHeLSig(lSigBits, Ieee80211HeSigFormat::SU);
    auto erSuLSig = decodeHeLSig(lSigBits, Ieee80211HeSigFormat::ER_SU);
    if (!suLSig && !erSuLSig) {
        stream.seek(startPosition);
        return nullptr;
    }
    auto sigFormat = suLSig ? Ieee80211HeSigFormat::SU : Ieee80211HeSigFormat::ER_SU;
    auto ppduFormat = suLSig ? HE_SINGLE_USER : HE_EXTENDED_RANGE_SU;
    auto lSigLength = suLSig ? suLSig.value.length : erSuLSig.value.length;
    auto rlSigBits = readLogicalBits(stream, 24);
    auto sigABits = readLogicalBits(stream, 52);
    if (!sigABits[0]) {
        stream.seek(startPosition);
        return nullptr;
    }
    auto rlSig = decodeHeRlSigRepeat(lSigBits, rlSigBits, sigFormat);
    if (!rlSig)
        throw cRuntimeError("Cannot deserialize HE SU/ER RL-SIG: %s", rlSig.error.c_str());
    if (expectedPpduFormat.has_value() && ppduFormat != *expectedPpduFormat)
        throw cRuntimeError("Logical HE SU/ER signaling does not match requested header type");

    auto header = createIeee80211HePhyHeader(ppduFormat);
    Ieee80211HeSuErSignalingFields fields;
    if (ppduFormat == HE_SINGLE_USER) {
        auto sigA = decodeHeSuSigA(sigABits);
        if (!sigA)
            throw cRuntimeError("Cannot deserialize HE SU HE-SIG-A: %s", sigA.error.c_str());
        fields = makeHeSuErSignalingFields(lSigLength, sigA.value);
        dynamicPtrCast<Ieee80211HeSuPhyHeader>(header)->setSignaling(fields);
    }
    else {
        auto sigA = decodeHeErSuSigA(sigABits);
        if (!sigA)
            throw cRuntimeError("Cannot deserialize HE ER SU HE-SIG-A: %s", sigA.error.c_str());
        fields = makeHeSuErSignalingFields(lSigLength, sigA.value);
        dynamicPtrCast<Ieee80211HeErSuPhyHeader>(header)->setSignaling(fields);
    }
    header->setBssColor(fields.bssColor);
    header->setSpatialReuse(fields.spatialReuse);
    header->setGuardInterval(fields.giLtfSize == 3 ? 2 : fields.giLtfSize == 2 ? 1 : 0);
    header->setCoding(fields.ldpcCoding ? HE_CODING_LDPC : HE_CODING_BCC);
    return header;
}

void writeOfdmSignal(MemoryOutputStream& stream, uint8_t rate, bool reserved, uint16_t length, bool parity, uint8_t tail)
{
    auto signal = packIeee80211OfdmSignalField(rate, reserved, length, parity, tail);
    stream.writeByte(signal & 0xFF);
    stream.writeByte((signal >> 8) & 0xFF);
    stream.writeByte((signal >> 16) & 0xFF);
}

void readOfdmSignal(MemoryInputStream& stream, uint8_t& rate, bool& reserved, uint16_t& length, bool& parity, uint8_t& tail)
{
    uint32_t signal = stream.readByte();
    signal |= stream.readByte() << 8;
    signal |= stream.readByte() << 16;
    auto field = unpackIeee80211OfdmSignalField(signal);
    rate = field.rate;
    reserved = field.reserved;
    length = field.length;
    parity = field.parity;
    tail = field.tail;
}

} // namespace

Register_Serializer(Ieee80211FhssPhyHeader, Ieee80211FhssPhyHeaderSerializer);
Register_Serializer(Ieee80211IrPhyHeader, Ieee80211IrPhyHeaderSerializer);
Register_Serializer(Ieee80211DsssPhyHeader, Ieee80211DsssPhyHeaderSerializer);
Register_Serializer(Ieee80211HrDsssPhyHeader, Ieee80211HrDsssPhyHeaderSerializer);
Register_Serializer(Ieee80211OfdmPhyHeader, Ieee80211OfdmPhyHeaderSerializer);
Register_Serializer(Ieee80211ErpOfdmPhyHeader, Ieee80211ErpOfdmPhyHeaderSerializer);
Register_Serializer(Ieee80211HtPhyHeader, Ieee80211HtPhyHeaderSerializer);
Register_Serializer(Ieee80211VhtPhyHeader, Ieee80211VhtPhyHeaderSerializer);
Register_Serializer(Ieee80211HePhyHeader, Ieee80211HePhyHeaderSerializer);
Register_Serializer(Ieee80211HeSuPhyHeader, Ieee80211HeSuPhyHeaderSerializer);
Register_Serializer(Ieee80211HeErSuPhyHeader, Ieee80211HeErSuPhyHeaderSerializer);
Register_Serializer(Ieee80211HeMuPhyHeader, Ieee80211HeMuPhyHeaderSerializer);
Register_Serializer(Ieee80211HeTbPhyHeader, Ieee80211HeTbPhyHeaderSerializer);
Register_Serializer(Ieee80211EhtPhyHeader, Ieee80211EhtPhyHeaderSerializer);
Register_Serializer(Ieee80211EhtRuPayloadHeader, Ieee80211EhtRuPayloadHeaderSerializer);

/**
 * FHSS
 */
void Ieee80211FhssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto fhssPhyHeader = dynamicPtrCast<const Ieee80211FhssPhyHeader>(chunk);
    stream.writeNBitsOfUint64Be(fhssPhyHeader->getPlw(), 12);
    stream.writeUint4(fhssPhyHeader->getPsf());
    stream.writeUint16Be(fhssPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211FhssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto fhssPhyHeader = makeShared<Ieee80211FhssPhyHeader>();
    fhssPhyHeader->setPlw(stream.readNBitsToUint64Be(12));
    fhssPhyHeader->setPsf(stream.readUint4());
    fhssPhyHeader->setFcs(stream.readUint16Be());
    fhssPhyHeader->setFcsMode(FCS_COMPUTED);
    return fhssPhyHeader;
}

/**
 * IR
 */
void Ieee80211IrPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto irPhyHeader = dynamicPtrCast<const Ieee80211IrPhyHeader>(chunk);
    stream.writeUint16Be(irPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211IrPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto irPhyHeader = makeShared<Ieee80211IrPhyHeader>();
    irPhyHeader->setFcs(stream.readUint16Be());
    irPhyHeader->setFcsMode(FCS_COMPUTED);
    return irPhyHeader;
}

/**
 * DSSS
 */
void Ieee80211DsssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto dsssPhyHeader = dynamicPtrCast<const Ieee80211DsssPhyHeader>(chunk);
    // IEEE Std 802.11-2024 15.3.3.4..15.3.3.7: SIGNAL, SERVICE, LENGTH, CRC.
    // NOTE: the standard PHY LENGTH field is in microseconds, converted from
    // TXVECTOR LENGTH octets in 15.3.6. INET's lengthField is packet-level PSDU
    // octets, so this serializer is not bit-exact for DSSS PLCP LENGTH.
    stream.writeByte(dsssPhyHeader->getSignal());
    stream.writeByte(dsssPhyHeader->getService());
    stream.writeUint16Le(dsssPhyHeader->getLengthField().get<B>());
    stream.writeUint16Le(dsssPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211DsssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto dsssPhyHeader = makeShared<Ieee80211DsssPhyHeader>();
    // See the serialize-side note: the decoded LENGTH value is preserved in
    // lengthField, but the packet-level receiver expects PSDU octets.
    dsssPhyHeader->setSignal(stream.readByte());
    dsssPhyHeader->setService(stream.readByte());
    dsssPhyHeader->setLengthField(B(stream.readUint16Le()));
    dsssPhyHeader->setFcs(stream.readUint16Le());
    dsssPhyHeader->setFcsMode(FCS_COMPUTED);
    return dsssPhyHeader;
}

/**
 * HR/DSSS
 */
void Ieee80211HrDsssPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto hrDsssPhyHeader = dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(chunk);
    // IEEE Std 802.11-2024 16.2.3.4..16.2.3.7 and 16.2.3.11..16.2.3.14:
    // long and short HR/DSSS headers carry SIGNAL, SERVICE, LENGTH, and CRC.
    // The standard LENGTH field is in microseconds and may need the SERVICE
    // length-extension bit (16.2.3.6); INET stores PSDU octets here.
    stream.writeByte(hrDsssPhyHeader->getSignal());
    stream.writeByte(hrDsssPhyHeader->getService());
    stream.writeUint16Le(hrDsssPhyHeader->getLengthField().get<B>());
    stream.writeUint16Le(hrDsssPhyHeader->getFcs());
}

const Ptr<Chunk> Ieee80211HrDsssPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto hrDsssPhyHeader = makeShared<Ieee80211HrDsssPhyHeader>();
    // See the serialize-side note: full HR/DSSS LENGTH/SERVICE conversion is
    // outside this packet-level serializer.
    hrDsssPhyHeader->setSignal(stream.readByte());
    hrDsssPhyHeader->setService(stream.readByte());
    hrDsssPhyHeader->setLengthField(B(stream.readUint16Le()));
    hrDsssPhyHeader->setFcs(stream.readUint16Le());
    hrDsssPhyHeader->setFcsMode(FCS_COMPUTED);
    return hrDsssPhyHeader;
}

/**
 * OFDM
 */
void Ieee80211OfdmPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto ofdmPhyHeader = dynamicPtrCast<const Ieee80211OfdmPhyHeader>(chunk);
    // IEEE Std 802.11-2024 17.3.4: RATE/reserved/LENGTH/parity/tail are the
    // 24-bit SIGNAL field; the 16-bit SERVICE field starts the DATA field
    // (17.3.5.2) but is kept in this header chunk in the packet-level model.
    writeOfdmSignal(stream, ofdmPhyHeader->getRate(), ofdmPhyHeader->getReserved(), ofdmPhyHeader->getLengthField().get<B>(), ofdmPhyHeader->getParity(), ofdmPhyHeader->getTail());
    stream.writeUint16Le(ofdmPhyHeader->getService());
}

const Ptr<Chunk> Ieee80211OfdmPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto ofdmPhyHeader = makeShared<Ieee80211OfdmPhyHeader>();
    uint8_t rate;
    bool reserved;
    uint16_t length;
    bool parity;
    uint8_t tail;
    readOfdmSignal(stream, rate, reserved, length, parity, tail);
    ofdmPhyHeader->setRate(rate);
    ofdmPhyHeader->setReserved(reserved);
    ofdmPhyHeader->setLengthField(B(length));
    ofdmPhyHeader->setParity(parity);
    ofdmPhyHeader->setTail(tail);
    ofdmPhyHeader->setService(stream.readUint16Le());
    return ofdmPhyHeader;
}

/**
 * ERP OFDM
 */
void Ieee80211ErpOfdmPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto erpOfdmPhyHeader = dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(chunk);
    // IEEE Std 802.11-2024 18.3.2.4: ERP-OFDM uses the Clause 17 OFDM SIGNAL
    // format; the ERP signal extension is a timing property, not serialized here.
    writeOfdmSignal(stream, erpOfdmPhyHeader->getRate(), erpOfdmPhyHeader->getReserved(), erpOfdmPhyHeader->getLengthField().get<B>(), erpOfdmPhyHeader->getParity(), erpOfdmPhyHeader->getTail());
    stream.writeUint16Le(erpOfdmPhyHeader->getService());
}

const Ptr<Chunk> Ieee80211ErpOfdmPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto erpOfdmPhyHeader = makeShared<Ieee80211ErpOfdmPhyHeader>();
    uint8_t rate;
    bool reserved;
    uint16_t length;
    bool parity;
    uint8_t tail;
    readOfdmSignal(stream, rate, reserved, length, parity, tail);
    erpOfdmPhyHeader->setRate(rate);
    erpOfdmPhyHeader->setReserved(reserved);
    erpOfdmPhyHeader->setLengthField(B(length));
    erpOfdmPhyHeader->setParity(parity);
    erpOfdmPhyHeader->setTail(tail);
    erpOfdmPhyHeader->setService(stream.readUint16Le());
    return erpOfdmPhyHeader;
}

/**
 * HT
 */
void Ieee80211HtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto htPhyHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(chunk);
    // IEEE Std 802.11-2024 19.3.9.4.3 defines HT-SIG. INET currently models
    // HT signaling as packet metadata only; no HT-SIG bits are serialized here.
}

const Ptr<Chunk> Ieee80211HtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto htPhyHeader = makeShared<Ieee80211HtPhyHeader>();
    return htPhyHeader;
}

/**
 * VHT
 */
void Ieee80211VhtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto vhtPhyHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(chunk);
    if (!vhtPhyHeader->getSignalingValid())
        throw cRuntimeError("VHT serialization requires validated logical signaling");
    if (vhtPhyHeader->getBandwidth() > 3)
        throw cRuntimeError("Invalid VHT-SIG-A bandwidth code");
    if (vhtPhyHeader->getCoding() > 1)
        throw cRuntimeError("VHT coding must be 0 for BCC or 1 for LDPC");
    if (vhtPhyHeader->getCoding() == 0 && vhtPhyHeader->getLdpcExtraOfdmSymbol())
        throw cRuntimeError("VHT LDPC Extra OFDM Symbol cannot be set for BCC coding");
    const bool mu = vhtPhyHeader->getGroupId() >= 1 && vhtPhyHeader->getGroupId() <= 62;
    if (mu) {
        if (vhtPhyHeader->getChunkLength() != b(100) || vhtPhyHeader->getBandwidth() != 0 ||
                vhtPhyHeader->getGroupId() != 1 || vhtPhyHeader->getStbc() ||
                vhtPhyHeader->getNdp() || !vhtPhyHeader->getBeamformed() ||
                vhtPhyHeader->getNumberOfSpaceTimeStreams() != 2 ||
                vhtPhyHeader->getUsersArraySize() != 2)
            throw cRuntimeError("Unsupported constrained packet-level VHT MU signaling");
        // IEEE Std 802.11-2024 Table 21-12: VHT-SIG-A1 BW, reserved,
        // STBC=0, GID=1, and MU NSTS[1,1,0,0].
        writeUnsignedLogicalField(stream, 0, 2);
        stream.writeBit(true);
        stream.writeBit(false);
        writeUnsignedLogicalField(stream, 1, 6);
        for (size_t position = 0; position < 4; ++position)
            writeUnsignedLogicalField(stream, position < 2 ? 1 : 0, 3);
        stream.writeBit(true); // TXOP_PS_NOT_ALLOWED
        stream.writeBit(true); // reserved
        // Table 21-12 VHT-SIG-A2. Coding for unused positions is reserved 1.
        stream.writeBit(false); // Short GI
        stream.writeBit(false); // Short GI NSYM disambiguation
        for (size_t position = 0; position < 4; ++position) {
            if (position < 2) {
                const auto& user = vhtPhyHeader->getUsers(position);
                if (user.userPosition != position || user.numberOfSpatialStreams != 1 ||
                        user.mcs > 9 || user.coding > 1 || user.psduLength <= B(0) ||
                        user.psduLength.get<B>() % 4 != 0 || user.psduLength > B(262140))
                    throw cRuntimeError("Invalid constrained VHT MU user signaling");
                stream.writeBit(user.coding != 0);
            }
            else
                stream.writeBit(true);
        }
        stream.writeBit(true); // B7 reserved
        stream.writeBit(true); // MU Beamformed field reserved 1
        stream.writeBit(true); // B9 reserved
        stream.writeBitRepeatedly(false, 8); // packet-level logical CRC placeholder
        stream.writeBitRepeatedly(false, 6); // tail
        // Table 21-14: each 20 MHz MU user has Length[16], VHT-MCS[4], Tail[6].
        for (size_t i = 0; i < 2; ++i) {
            const auto& user = vhtPhyHeader->getUsers(i);
            writeUnsignedLogicalField(stream, user.psduLength.get<B>() / 4, 16);
            writeUnsignedLogicalField(stream, user.mcs, 4);
            stream.writeBitRepeatedly(false, 6);
        }
        return;
    }
    if (vhtPhyHeader->getChunkLength() != b(50))
        throw cRuntimeError("VHT packet-level SU/NDP PHY header must be exactly 50 bits");
    if (vhtPhyHeader->getStbc() ||
             (vhtPhyHeader->getGroupId() != 0 && vhtPhyHeader->getGroupId() != 63) ||
             vhtPhyHeader->getNumberOfSpaceTimeStreams() == 0 ||
             vhtPhyHeader->getNumberOfSpaceTimeStreams() > 8 ||
             vhtPhyHeader->getPartialAid() > 511 || vhtPhyHeader->getMcs() > 9)
        throw cRuntimeError("Unsupported or invalid packet-level VHT SU signaling");
    if (vhtPhyHeader->getNdp() &&
            (vhtPhyHeader->getNumberOfSpaceTimeStreams() < 2 ||
             vhtPhyHeader->getMcs() != 0 || vhtPhyHeader->getCoding() != 0 ||
             vhtPhyHeader->getLdpcExtraOfdmSymbol() ||
             vhtPhyHeader->getBeamformed()))
        throw cRuntimeError("Malformed packet-level VHT NDP signaling");
    // IEEE Std 802.11-2024 21.3.2, 21.3.8, and 21.3.9 define VHT-SIG fields.
    // Preserve the two modeled logical VHT-SIG-A2 fields at B2/B3 (Table
    // 21-12). Scrambling, convolutional encoding, interleaving, and modulation
    // of VHT-SIG-A remain outside this packet-level serializer.
    auto startPosition = stream.getLength();
    // Table 21-11: BW, reserved B2=1, STBC=0,
    // Group ID=0/63, SU NSTS, and Partial AID. NDP is determined by a zero
    // APEP length in the canonical TXVECTOR, never by Group ID.
    writeUnsignedLogicalField(stream, vhtPhyHeader->getBandwidth(), 2);
    stream.writeBit(true); // normative reserved B2
    stream.writeBit(vhtPhyHeader->getStbc());
    writeUnsignedLogicalField(stream, vhtPhyHeader->getGroupId(), 6);
    writeUnsignedLogicalField(stream, vhtPhyHeader->getNumberOfSpaceTimeStreams() - 1, 3);
    writeUnsignedLogicalField(stream, vhtPhyHeader->getPartialAid(), 9);
    stream.writeBit(true); // TXOP_PS_NOT_ALLOWED for this constrained subset
    stream.writeBit(true); // reserved
    stream.writeBit(false); // Short GI
    stream.writeBit(false); // Short GI NSYM disambiguation
    stream.writeBit(vhtPhyHeader->getCoding() != 0);
    stream.writeBit(vhtPhyHeader->getLdpcExtraOfdmSymbol());
    writeUnsignedLogicalField(stream, vhtPhyHeader->getMcs(), 4);
    stream.writeBit(vhtPhyHeader->getBeamformed());
    stream.writeBit(true); // reserved
    b remainder = vhtPhyHeader->getChunkLength() - (stream.getLength() - startPosition);
    if (remainder < b(0))
        throw cRuntimeError("VHT PHY header is too short for modeled signaling fields");
    stream.writeBitRepeatedly(false, remainder.get<b>());
}

const Ptr<Chunk> Ieee80211VhtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    constexpr int64_t suSerializedLength = 50;
    if (stream.getRemainingLength() < b(suSerializedLength))
        throw cRuntimeError("VHT PHY header is too short for modeled signaling fields");
    auto vhtPhyHeader = makeShared<Ieee80211VhtPhyHeader>();
    auto bandwidth = readUnsignedLogicalField(stream, 2);
    auto reservedB2 = stream.readBit();
    auto stbc = stream.readBit();
    auto groupId = readUnsignedLogicalField(stream, 6);
    if (groupId >= 1 && groupId <= 62) {
        if (stream.getRemainingLength() < b(90))
            throw cRuntimeError("VHT MU PHY header is too short for VHT-SIG-A/B");
        std::array<uint8_t, 4> nsts;
        for (auto& value : nsts)
            value = readUnsignedLogicalField(stream, 3);
        auto txopPsNotAllowed = stream.readBit();
        auto reservedA1 = stream.readBit();
        auto shortGi = stream.readBit();
        auto shortGiDisambiguation = stream.readBit();
        std::array<bool, 4> coding;
        for (auto& value : coding)
            value = stream.readBit();
        auto reservedA2B7 = stream.readBit();
        auto beamformedReserved = stream.readBit();
        auto reservedA2B9 = stream.readBit();
        for (int i = 0; i < 8; ++i)
            (void)stream.readBit(); // logical CRC is not evaluated by this packet-level codec
        for (int i = 0; i < 6; ++i)
            if (stream.readBit())
                throw cRuntimeError("Malformed VHT-SIG-A tail");
        if (!reservedB2 || stbc || groupId != 1 || nsts != std::array<uint8_t, 4>{1, 1, 0, 0} ||
                !txopPsNotAllowed || !reservedA1 || shortGi || shortGiDisambiguation ||
                !coding[2] || !coding[3] || !reservedA2B7 || !beamformedReserved ||
                !reservedA2B9)
            throw cRuntimeError("Unsupported or malformed constrained VHT MU SIG-A");
        vhtPhyHeader->setUsersArraySize(2);
        B totalLength(0);
        for (size_t i = 0; i < 2; ++i) {
            Ieee80211VhtMuUserInfo user;
            user.userPosition = i;
            user.numberOfSpatialStreams = 1;
            user.coding = coding[i] ? 1 : 0;
            user.psduLength = B(4 * readUnsignedLogicalField(stream, 16));
            user.mcs = readUnsignedLogicalField(stream, 4);
            for (int bit = 0; bit < 6; ++bit)
                if (stream.readBit())
                    throw cRuntimeError("Malformed VHT-SIG-B tail");
            if (user.psduLength <= B(0) || user.mcs > 9)
                throw cRuntimeError("Malformed constrained VHT MU SIG-B user");
            totalLength += user.psduLength;
            vhtPhyHeader->setUsers(i, user);
        }
        vhtPhyHeader->setSignalingValid(true);
        vhtPhyHeader->setBandwidth(bandwidth);
        vhtPhyHeader->setGroupId(groupId);
        vhtPhyHeader->setStbc(false);
        vhtPhyHeader->setNumberOfSpaceTimeStreams(2);
        vhtPhyHeader->setBeamformed(true);
        vhtPhyHeader->setLengthField(totalLength);
        vhtPhyHeader->setChunkLength(b(100));
        return vhtPhyHeader;
    }
    auto nsts = readUnsignedLogicalField(stream, 3) + 1;
    auto partialAid = readUnsignedLogicalField(stream, 9);
    auto txopPsNotAllowed = stream.readBit();
    auto reservedA1 = stream.readBit();
    auto shortGi = stream.readBit();
    auto shortGiDisambiguation = stream.readBit();
    vhtPhyHeader->setCoding(stream.readBit() ? 1 : 0);
    vhtPhyHeader->setLdpcExtraOfdmSymbol(stream.readBit());
    vhtPhyHeader->setMcs(readUnsignedLogicalField(stream, 4));
    vhtPhyHeader->setBeamformed(stream.readBit());
    auto reservedA2 = stream.readBit();
    vhtPhyHeader->setSignalingValid(true);
    vhtPhyHeader->setBandwidth(bandwidth);
    vhtPhyHeader->setGroupId(groupId);
    vhtPhyHeader->setStbc(stbc);
    vhtPhyHeader->setNumberOfSpaceTimeStreams(nsts);
    vhtPhyHeader->setPartialAid(partialAid);
    if (!reservedB2 || stbc ||
            (groupId != 0 && groupId != 63) ||
            !txopPsNotAllowed || !reservedA1 || shortGi ||
            shortGiDisambiguation || !reservedA2)
        throw cRuntimeError("Unsupported or malformed packet-level VHT SU signaling");
    if (vhtPhyHeader->getCoding() == 0 && vhtPhyHeader->getLdpcExtraOfdmSymbol())
        throw cRuntimeError("VHT LDPC Extra OFDM Symbol cannot be set for BCC coding");
    for (int64_t i = 34; i < suSerializedLength; i++)
        if (stream.readBit())
            throw cRuntimeError("Unsupported nonzero packet-level VHT signaling remainder");
    vhtPhyHeader->setChunkLength(b(suSerializedLength));
    return vhtPhyHeader;
}

/**
 * HE
 */
void Ieee80211HePhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto hePhyHeader = dynamicPtrCast<const Ieee80211HePhyHeader>(chunk);
    auto ppduFormat = getIeee80211HePpduFormat(*hePhyHeader);
    if (serializeExactHeSuEr(stream, hePhyHeader, ppduFormat))
        return;
    Ieee80211HeLSig lSig;
    Ieee80211HeSigABitsResult sigA;
    HeSigBLogicalField sigB;
    if (ppduFormat == HE_MU_DOWNLINK) {
        const auto& fields = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(hePhyHeader)->getSignaling();
        if (!fields.signalingValid)
            throw cRuntimeError("HE MU serialization requires validated logical signaling fields");
        lSig.length = fields.lSigLength;
        Ieee80211HeMuSigA value;
        value.heSigBMcs = fields.heSigBMcs;
        value.heSigBDcm = fields.heSigBDcm;
        value.bssColor = hePhyHeader->getBssColor();
        value.spatialReuse = hePhyHeader->getSpatialReuse();
        value.bandwidth = fields.bandwidth;
        value.heSigBCompression = fields.heSigBCompression;
        value.numberOfHeSigBSymbols = fields.numberOfHeSigBSymbols;
        value.numberOfHeSigBSymbolsIsSaturated = fields.numberOfHeSigBSymbolsIsSaturated;
        value.numberOfMuMimoUsers = fields.numberOfMuMimoUsers;
        value.giLtfSize = fields.giLtfSize;
        value.doppler = fields.doppler;
        value.txop = fields.txop;
        value.numberOfHeLtfSymbols = fields.numberOfHeLtfSymbols;
        value.midamblePeriodicity = fields.midamblePeriodicity;
        value.ldpcExtraSymbolSegment = fields.ldpcExtraSymbolSegment;
        value.stbc = fields.stbc;
        value.preFecPaddingFactor = fields.preFecPaddingFactor;
        value.peDisambiguity = fields.peDisambiguity;
        sigA = encodeHeMuSigA(value);
        sigB = buildHeSigBLogicalField(*hePhyHeader, fields);
    }
    else if (ppduFormat == HE_TRIGGER_BASED_UPLINK) {
        const auto& fields = dynamicPtrCast<const Ieee80211HeTbPhyHeader>(hePhyHeader)->getSignaling();
        if (!fields.signalingValid)
            throw cRuntimeError("HE TB serialization requires Trigger-derived logical signaling fields");
        lSig.length = fields.lSigLength;
        Ieee80211HeTbSigA value;
        value.bssColor = hePhyHeader->getBssColor();
        value.spatialReuse = {{fields.spatialReuse1, fields.spatialReuse2,
                fields.spatialReuse3, fields.spatialReuse4}};
        value.bandwidth = fields.bandwidth;
        value.txop = fields.txop;
        value.triggerReserved = fields.triggerReserved;
        sigA = encodeHeTbSigA(value);
    }
    else
        throw cRuntimeError("HE serializer received an unsupported concrete header");
    auto sigFormat = ppduFormat == HE_MU_DOWNLINK ? Ieee80211HeSigFormat::MU : Ieee80211HeSigFormat::TB;
    auto lSigBits = encodeHeLSig(lSig, sigFormat);
    auto rlSigBits = encodeHeRlSig(lSig, sigFormat);
    if (!lSigBits)
        throw cRuntimeError("Cannot serialize HE MU/TB L-SIG: %s", lSigBits.error.c_str());
    if (!rlSigBits)
        throw cRuntimeError("Cannot serialize HE MU/TB RL-SIG: %s", rlSigBits.error.c_str());
    if (!sigA)
        throw cRuntimeError("Cannot serialize HE MU/TB HE-SIG-A: %s", sigA.error.c_str());
    stream.writeBits(lSigBits.bits);
    stream.writeBits(rlSigBits.bits);
    stream.writeBits(sigA.bits);
    for (const auto& channel : sigB.channels)
        stream.writeBits(channel);
}

const Ptr<Chunk> Ieee80211HePhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    if (!expectedPpduFormat)
        throw cRuntimeError("Raw HE PHY decoding requires a concrete header type because HE-SIG-A constellation metadata is not represented");
    auto ppduFormat = *expectedPpduFormat;
    if (ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU) {
        auto header = deserializeExactHeSuEr(stream, expectedPpduFormat);
        if (!header)
            throw cRuntimeError("Raw HE SU/ER signaling does not match the requested concrete header type");
        return header;
    }
    auto lSigBits = readLogicalBits(stream, 24);
    auto rlSigBits = readLogicalBits(stream, 24);
    auto sigABits = readLogicalBits(stream, 52);
    auto sigFormat = ppduFormat == HE_MU_DOWNLINK ? Ieee80211HeSigFormat::MU : Ieee80211HeSigFormat::TB;
    auto lSig = decodeHeLSig(lSigBits, sigFormat);
    auto rlSig = decodeHeRlSigRepeat(lSigBits, rlSigBits, sigFormat);
    if (!lSig || !rlSig)
        throw cRuntimeError("Malformed HE MU/TB L-SIG or RL-SIG");

    if (ppduFormat == HE_TRIGGER_BASED_UPLINK) {
        auto sigA = decodeHeTbSigA(sigABits);
        if (!sigA)
            throw cRuntimeError("Cannot deserialize HE TB HE-SIG-A: %s", sigA.error.c_str());
        auto header = makeShared<Ieee80211HeTbPhyHeader>();
        Ieee80211HeTbSignalingFields fields;
        fields.signalingValid = true;
        fields.lSigLength = lSig.value.length;
        fields.spatialReuse1 = sigA.value.spatialReuse[0];
        fields.spatialReuse2 = sigA.value.spatialReuse[1];
        fields.spatialReuse3 = sigA.value.spatialReuse[2];
        fields.spatialReuse4 = sigA.value.spatialReuse[3];
        fields.bandwidth = sigA.value.bandwidth;
        fields.txop = sigA.value.txop;
        fields.triggerReserved = sigA.value.triggerReserved;
        header->setSignaling(fields);
        header->setBssColor(sigA.value.bssColor);
        header->setSpatialReuse(sigA.value.spatialReuse[0]);
        header->setChunkLength(b(100));
        return header;
    }
    auto sigA = decodeHeMuSigA(sigABits);
    if (!sigA)
        throw cRuntimeError("Cannot deserialize HE MU HE-SIG-A: %s", sigA.error.c_str());
    if (!sigA.value.heSigBCompression && sigA.value.numberOfHeSigBSymbolsIsSaturated)
        throw cRuntimeError("Cannot deserialize saturated HE-SIG-B symbol count without resolved RXVECTOR length");
    Hz channelBandwidth = getHeBandwidth(sigA.value.bandwidth);
    int channelCount = channelBandwidth == MHz(20) ? 1 : 2;
    int bitsPerChannel = getHeSigBDataBitsPerSymbol(sigA.value.heSigBMcs,
            sigA.value.heSigBDcm) * (sigA.value.heSigBCompression ?
            getHeSigBSymbolCount(channelBandwidth, sigA.value.numberOfMuMimoUsers, true,
                    sigA.value.heSigBMcs, sigA.value.heSigBDcm) : sigA.value.numberOfHeSigBSymbols);
    std::vector<std::vector<bool>> channels;
    for (int channel = 0; channel < channelCount; ++channel)
        channels.push_back(readLogicalBits(stream, bitsPerChannel));

    Ieee80211HeSigBCommonField commonField;
    std::vector<std::vector<HeSigBAllocation>> allocations(channelCount);
    size_t commonBits = 0;
    uint8_t puncturedSubchannelMask = 0;
    bool puncturedSubchannelMaskKnown = true;
    if (sigA.value.heSigBCompression) {
        auto fullRu = getHeEqualRuLayout(Hz(0), channelBandwidth, 1).front();
        if (fullRu.toneSize != 1992)
            throw cRuntimeError("This packet-level compressed HE-SIG-B contract requires a 2x996-tone RU");
        int count1 = (sigA.value.numberOfMuMimoUsers + 1) / 2;
        allocations[0].push_back({fullRu, count1});
        if (channelCount == 2 && sigA.value.numberOfMuMimoUsers > count1)
            allocations[1].push_back({fullRu, sigA.value.numberOfMuMimoUsers - count1});
    }
    else {
        int allocationCount = channelBandwidth >= MHz(160) ? 4 : channelBandwidth >= MHz(80) ? 2 : 1;
        commonBits = allocationCount * 8 + (channelBandwidth > MHz(40) ? 1 : 0) + 10;
        commonField.contentChannels.resize(channelCount);
        for (int channel = 0; channel < channelCount; ++channel) {
            auto block = decodeHeSigBCommonBlock(std::vector<bool>(channels[channel].begin(),
                    channels[channel].begin() + commonBits));
            if (!block)
                throw cRuntimeError("Cannot deserialize HE-SIG-B Common block: %s", block.error.c_str());
            commonField.contentChannels[channel].ruAllocationSubfields = block.value.ruAllocationSubfields;
            commonField.contentChannels[channel].hasCenterRu = block.value.hasCenter26ToneRu;
        }
        auto decoded = decodeHeSigBCommonField(commonField, Hz(0), channelBandwidth);
        if (!decoded)
            throw cRuntimeError("Cannot deserialize HE-SIG-B RU allocation: %s", decoded.error.c_str());
        allocations = decodeHeSigBAllocations(commonField, decoded.commonField, channelBandwidth);
        std::tie(puncturedSubchannelMask, puncturedSubchannelMaskKnown) =
                decodeHeMuPuncturing(sigA.value.bandwidth, commonField, channelBandwidth);
    }

    std::map<std::pair<int, int>, int> totalUsers;
    for (const auto& channel : allocations)
        for (const auto& allocation : channel)
            totalUsers[{allocation.ru.toneSize, allocation.ru.toneOffset}] += allocation.userCount;
    std::map<std::pair<int, int>, int> decodedUsers;
    std::map<std::pair<int, int>, uint8_t> spatialConfigurations;
    bool codingInitialized = false;
    bool commonCoding = false;
    auto header = makeShared<Ieee80211HeMuPhyHeader>();
    for (int channel = 0; channel < channelCount; ++channel) {
        std::vector<Ieee80211HeRu> expanded;
        for (const auto& allocation : allocations[channel])
            for (int i = 0; i < allocation.userCount; ++i)
                expanded.push_back(allocation.ru);
        size_t bitOffset = commonBits;
        size_t userOffset = 0;
        while (userOffset < expanded.size()) {
            size_t blockUsers = std::min<size_t>(2, expanded.size() - userOffset);
            size_t blockBits = blockUsers * 21 + 10;
            if (bitOffset + blockBits > channels[channel].size())
                throw cRuntimeError("HE-SIG-B User blocks exceed the signaled symbol count");
            auto block = decodeHeSigBUserBlock(std::vector<bool>(channels[channel].begin() + bitOffset,
                    channels[channel].begin() + bitOffset + blockBits));
            if (!block)
                throw cRuntimeError("Cannot deserialize HE-SIG-B User block: %s", block.error.c_str());
            for (size_t i = 0; i < blockUsers; ++i) {
                const auto& ru = expanded[userOffset + i];
                auto geometry = std::make_pair(ru.toneSize, ru.toneOffset);
                int groupSize = totalUsers[geometry];
                Ieee80211HeMuUserInfo user;
                bool userCoding;
                if (groupSize > 1) {
                    auto decoded = decodeHeSigBMuMimoUser(block.userFields[i]);
                    if (!decoded)
                        throw cRuntimeError("Cannot deserialize HE-SIG-B MU-MIMO User field: %s", decoded.error.c_str());
                    auto nsts = decodeHeMuSpatialConfiguration(groupSize, decoded.value.spatialConfiguration);
                    int position = decodedUsers[geometry]++;
                    user.staId = decoded.value.staId;
                    user.mcs = decoded.value.mcs;
                    user.numberOfSpatialStreams = nsts[position];
                    user.streamStartIndex = std::accumulate(nsts.begin(), nsts.begin() + position, 0);
                    user.muMimo = true;
                    user.spatialConfiguration = decoded.value.spatialConfiguration;
                    userCoding = decoded.value.ldpcCoding;
                    auto previous = spatialConfigurations.find(geometry);
                    if (previous != spatialConfigurations.end() && previous->second != decoded.value.spatialConfiguration)
                        throw cRuntimeError("HE-SIG-B MU-MIMO users disagree on spatial configuration");
                    spatialConfigurations[geometry] = decoded.value.spatialConfiguration;
                }
                else {
                    auto decoded = decodeHeSigBNonMuMimoUser(block.userFields[i]);
                    if (!decoded)
                        throw cRuntimeError("Cannot deserialize HE-SIG-B User field: %s", decoded.error.c_str());
                    user.staId = decoded.value.staId;
                    user.mcs = decoded.value.mcs;
                    user.numberOfSpatialStreams = decoded.value.numberOfSpaceTimeStreams;
                    user.dcm = decoded.value.dcm;
                    userCoding = decoded.value.ldpcCoding;
                }
                if (user.staId == 2046)
                    continue;
                user.coding = userCoding ? HE_CODING_LDPC : HE_CODING_BCC;
                if (!codingInitialized) {
                    codingInitialized = true;
                    commonCoding = userCoding;
                }
                user.ruIndex = ru.index;
                user.ruToneSize = ru.toneSize;
                user.ruToneOffset = ru.toneOffset;
                header->appendUsers(user);
            }
            userOffset += blockUsers;
            bitOffset += blockBits;
        }
        if (std::any_of(channels[channel].begin() + bitOffset, channels[channel].end(),
                [] (bool bit) { return bit; }))
            throw cRuntimeError("HE-SIG-B padding bits must be zero");
    }
    Ieee80211HeMuSignalingFields fields;
    fields.signalingValid = true;
    fields.lSigLength = lSig.value.length;
    fields.heSigBMcs = sigA.value.heSigBMcs;
    fields.heSigBDcm = sigA.value.heSigBDcm;
    fields.bandwidth = sigA.value.bandwidth;
    fields.heSigBCompression = sigA.value.heSigBCompression;
    fields.numberOfHeSigBSymbols = sigA.value.numberOfHeSigBSymbols;
    fields.numberOfHeSigBSymbolsIsSaturated = sigA.value.numberOfHeSigBSymbolsIsSaturated;
    fields.numberOfMuMimoUsers = sigA.value.numberOfMuMimoUsers;
    fields.giLtfSize = sigA.value.giLtfSize;
    fields.doppler = sigA.value.doppler;
    fields.midamblePeriodicity = sigA.value.midamblePeriodicity;
    fields.txop = sigA.value.txop;
    fields.numberOfHeLtfSymbols = sigA.value.numberOfHeLtfSymbols;
    fields.ldpcExtraSymbolSegment = sigA.value.ldpcExtraSymbolSegment;
    fields.stbc = sigA.value.stbc;
    fields.preFecPaddingFactor = sigA.value.preFecPaddingFactor;
    fields.peDisambiguity = sigA.value.peDisambiguity;
    header->setSignaling(fields);
    header->setBssColor(sigA.value.bssColor);
    header->setSpatialReuse(sigA.value.spatialReuse);
    header->setCoding(commonCoding ? HE_CODING_LDPC : HE_CODING_BCC);
    header->setPuncturedSubchannelMask(puncturedSubchannelMask);
    header->setPuncturedSubchannelMaskKnown(puncturedSubchannelMaskKnown);
    header->setChunkLength(b(100 + channelCount * bitsPerChannel));
    return header;
}

/**
 * EHT PHY Header
 *
 * Fixed 12-byte packet-level representation for the currently modeled
 * single-user EHT MU-format path. The PPDU format, U-SIG SU indication,
 * coding, puncturing, and selected MCS/NSS survive byte conversion.
 */
void Ieee80211EhtPhyHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto header = dynamicPtrCast<const Ieee80211EhtPhyHeader>(chunk);
    if (header->getUsersArraySize() != 1)
        throw cRuntimeError("The packet-level EHT PHY header serializer requires exactly one user");
    const auto& uSig = header->getUSig();
    uint8_t common = (header->getPpduFormat() & 0x01) |
            ((header->getGuardInterval() & 0x03) << 1) |
            ((header->getCoding() & 0x01) << 3) |
            ((uSig.signalingValid ? 1 : 0) << 4) |
            ((uSig.ppduTypeAndCompressionMode & 0x03) << 5);
    const auto& user = header->getUsers(0);
    uint8_t mcsNss = (user.mcs & 0x0f) | (((user.numberOfSpatialStreams - 1) & 0x07) << 4);
    stream.writeUint16Le(header->getLengthField().get<B>());
    stream.writeByte(common);
    stream.writeByte(header->getBssColor());
    stream.writeUint16Le(header->getPuncturedSubchannelMask());
    stream.writeByte(mcsNss);
    stream.writeUint16Le(user.mruToneSize);
    stream.writeUint16Le(user.psduLength.get<B>());
    stream.writeByte(0);
}

const Ptr<Chunk> Ieee80211EhtPhyHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto header = makeShared<Ieee80211EhtPhyHeader>();
    header->setLengthField(B(stream.readUint16Le()));
    uint8_t common = stream.readByte();
    header->setPpduFormat(common & 0x01);
    header->setGuardInterval((common >> 1) & 0x03);
    header->setCoding((common >> 3) & 0x01);
    Ieee80211EhtUSigFields uSig;
    uSig.signalingValid = (common >> 4) & 0x01;
    uSig.ppduTypeAndCompressionMode = (common >> 5) & 0x03;
    header->setUSig(uSig);
    header->setBssColor(stream.readByte());
    header->setPuncturedSubchannelMask(stream.readUint16Le());
    uint8_t mcsNss = stream.readByte();
    Ieee80211EhtUserInfo user;
    user.mcs = mcsNss & 0x0f;
    user.numberOfSpatialStreams = ((mcsNss >> 4) & 0x07) + 1;
    user.mruToneSize = stream.readUint16Le();
    user.psduLength = B(stream.readUint16Le());
    stream.readByte();
    header->appendUsers(user);
    header->setChunkLength(B(12));
    return header;
}

/**
 * EHT MU RU Payload Header
 *
 * Fixed 12-byte layout for the EHT packet-level model.
 */
void Ieee80211EhtRuPayloadHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    auto h = dynamicPtrCast<const Ieee80211EhtRuPayloadHeader>(chunk);
    stream.writeUint32Le((uint32_t)h->getMruIndex());
    stream.writeUint16Le(h->getMruToneSize());
    stream.writeUint16Le(h->getMruToneOffset());
    stream.writeUint16Le(h->getStaId());
    uint8_t mcsnss = (h->getMcs() & 0x0F) | (((h->getNumberOfSpatialStreams() - 1) & 0x07) << 4);
    stream.writeByte(mcsnss);
    uint8_t flags = (h->getMuMimo() ? 0x02 : 0x00);
    stream.writeByte(flags);
}

const Ptr<Chunk> Ieee80211EhtRuPayloadHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto h = makeShared<Ieee80211EhtRuPayloadHeader>();
    h->setMruIndex((int32_t)stream.readUint32Le());
    h->setMruToneSize(stream.readUint16Le());
    h->setMruToneOffset(stream.readUint16Le());
    h->setStaId(stream.readUint16Le());
    uint8_t mcsnss = stream.readByte();
    h->setMcs(mcsnss & 0x0F);
    h->setNumberOfSpatialStreams(((mcsnss >> 4) & 0x07) + 1);
    uint8_t flags = stream.readByte();
    h->setMuMimo(flags & 0x02);
    return h;
}

} // namespace physicallayer

} // namespace inet
