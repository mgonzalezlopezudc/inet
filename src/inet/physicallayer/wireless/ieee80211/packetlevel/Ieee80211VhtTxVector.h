//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTTXVECTOR_H
#define __INET_IEEE80211VHTTXVECTOR_H

#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/common/TagBase.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

enum Ieee80211VhtPpduFormat {
    VHT_SU,
    VHT_NDP,
    VHT_MU
};

struct Ieee80211VhtMuUser
{
    uint16_t associationId = 0;
    uint8_t userPosition = 0;
    uint8_t numberOfSpatialStreams = 1;
    uint8_t mcs = 0;
    bool ldpcCoding = false;
    B psduLength = B(0);
    simtime_t duration = SIMTIME_ZERO;
    double beamformingGainDb = 0;
    double leakagePenaltyDb = 0;
};

struct Ieee80211VhtPsduBitRange
{
    size_t userIndex = 0;
    b startBitOffset = b(0);
    b bitLength = b(0);

    b getEndBitOffset() const { return startBitOffset + bitLength; }
};

inline uint8_t getIeee80211VhtBandwidthCode(Hz channelWidth)
{
    if (channelWidth == MHz(20)) return 0;
    if (channelWidth == MHz(40)) return 1;
    if (channelWidth == MHz(80)) return 2;
    // VHT-SIG-A uses the same BW code for contiguous 160 and 80+80 MHz.
    if (channelWidth == MHz(160)) return 3;
    throw cRuntimeError("Unsupported VHT channel width");
}

inline Hz getIeee80211VhtChannelWidth(uint8_t bandwidthCode)
{
    switch (bandwidthCode) {
        case 0: return MHz(20);
        case 1: return MHz(40);
        case 2: return MHz(80);
        case 3: return MHz(160);
        default: throw cRuntimeError("Invalid VHT-SIG-A bandwidth code");
    }
}

inline b getIeee80211VhtMuPhyHeaderLength(size_t userCount)
{
    if (userCount < 2 || userCount > 4)
        throw cRuntimeError("VHT MU user count must be in the range 2..4");
    return b(48 + 26 * userCount);
}

/**
 * Immutable packet-level authority for the supported VHT SU/NDP/MU subset.
 * IEEE Std 802.11-2024 Tables 21-11, 21-12, 21-14 and Figure 21-28.
 */
class INET_API Ieee80211VhtTxVector
{
  protected:
    const Ieee80211VhtPpduFormat ppduFormat;
    const Hz channelWidth;
    const B psduLength;
    const uint8_t groupId;
    const uint8_t numberOfSpaceTimeStreams;
    const uint8_t mcs;
    const bool ldpcCoding;
    const bool ldpcExtraOfdmSymbol;
    const uint16_t partialAid;
    const bool beamformed;
    const double beamformingGainDb;
    const std::vector<Ieee80211VhtMuUser> users;
    const std::vector<Ieee80211VhtPsduBitRange> psduBitRanges;
    const simtime_t preambleDuration;
    const simtime_t headerDuration;
    const simtime_t dataDuration;
    const simtime_t commonDuration;

    Ieee80211VhtTxVector(Hz channelWidth, uint8_t groupId,
            const std::vector<Ieee80211VhtMuUser>& users,
            const std::vector<Ieee80211VhtPsduBitRange>& psduBitRanges,
            simtime_t preambleDuration, simtime_t commonDuration) :
        ppduFormat(VHT_MU), channelWidth(channelWidth),
        psduLength(psduBitRanges.empty() ? B(0) :
                B(psduBitRanges.back().getEndBitOffset())),
        groupId(groupId), numberOfSpaceTimeStreams(users.size()), mcs(0),
        ldpcCoding(false), ldpcExtraOfdmSymbol(false), partialAid(0),
        beamformed(true), beamformingGainDb(0), users(users),
        psduBitRanges(psduBitRanges),
        // VHT signaling is part of the preamble; the packet-level header
        // interval is therefore empty.
        preambleDuration(preambleDuration), headerDuration(SIMTIME_ZERO),
        dataDuration(commonDuration), commonDuration(preambleDuration + dataDuration) {}

  public:
    Ieee80211VhtTxVector(Hz channelWidth, B psduLength, uint8_t groupId,
            uint8_t numberOfSpaceTimeStreams, uint8_t mcs, bool ldpcCoding,
            bool ldpcExtraOfdmSymbol, uint16_t partialAid, bool beamformed,
            double beamformingGainDb) :
        ppduFormat(psduLength == B(0) ? VHT_NDP : VHT_SU),
        channelWidth(channelWidth), psduLength(psduLength), groupId(groupId),
        numberOfSpaceTimeStreams(numberOfSpaceTimeStreams), mcs(mcs),
        ldpcCoding(ldpcCoding), ldpcExtraOfdmSymbol(ldpcExtraOfdmSymbol),
        partialAid(partialAid), beamformed(beamformed),
        beamformingGainDb(beamformingGainDb), preambleDuration(SIMTIME_ZERO),
        headerDuration(SIMTIME_ZERO), dataDuration(SIMTIME_ZERO),
        commonDuration(SIMTIME_ZERO)
    {
        getIeee80211VhtBandwidthCode(channelWidth);
        if (psduLength < B(0))
            throw cRuntimeError("VHT PSDU length must not be negative");
        if (groupId != 0 && groupId != 63)
            throw cRuntimeError("Packet-level VHT SU/NDP supports only Group ID 0 or 63");
        if (numberOfSpaceTimeStreams == 0 || numberOfSpaceTimeStreams > 8)
            throw cRuntimeError("VHT SU/NDP NSTS must be in the range 1..8");
        if (mcs > 9 || partialAid > 511)
            throw cRuntimeError("Invalid VHT SU TXVECTOR MCS or Partial AID");
        if (!ldpcCoding && ldpcExtraOfdmSymbol)
            throw cRuntimeError("VHT LDPC Extra OFDM Symbol requires LDPC coding");
        if (!std::isfinite(beamformingGainDb) || beamformingGainDb < 0 ||
                (!beamformed && beamformingGainDb != 0))
            throw cRuntimeError("Invalid VHT beamforming state");
        if (ppduFormat == VHT_NDP && (numberOfSpaceTimeStreams < 2 ||
                mcs != 0 || ldpcCoding || ldpcExtraOfdmSymbol || beamformed ||
                beamformingGainDb != 0))
            throw cRuntimeError("VHT NDP must use the fixed packet-level NDP signaling subset");
        if ((ppduFormat == VHT_NDP || beamformed) && channelWidth != MHz(20))
            throw cRuntimeError("Packet-level VHT sounding/beamforming currently supports only 20 MHz");
    }

    static std::shared_ptr<const Ieee80211VhtTxVector> createMu(
            Hz channelWidth, uint8_t groupId,
            const std::vector<Ieee80211VhtMuUser>& users,
            simtime_t preambleDuration)
    {
        if (channelWidth != MHz(20) || groupId != 1 ||
                users.size() < 2 || users.size() > 4 ||
                preambleDuration <= SIMTIME_ZERO)
            throw cRuntimeError("Constrained VHT MU requires 20 MHz, GID 1, and 2..4 users");
        std::set<int> positions;
        std::vector<Ieee80211VhtPsduBitRange> ranges;
        b offset = b(0);
        simtime_t commonDuration = SIMTIME_ZERO;
        for (size_t i = 0; i < users.size(); ++i) {
            const auto& user = users[i];
            if (user.associationId == 0 ||
                    user.userPosition != i || !positions.insert(user.userPosition).second ||
                    user.numberOfSpatialStreams != 1 || user.mcs > 9 ||
                    user.psduLength <= B(0) || user.psduLength.get<B>() % 4 != 0 ||
                    user.duration <= SIMTIME_ZERO || !std::isfinite(user.beamformingGainDb) ||
                    user.beamformingGainDb < 0 || !std::isfinite(user.leakagePenaltyDb) ||
                    user.leakagePenaltyDb < 0)
                throw cRuntimeError("Invalid constrained VHT MU user at position %zu", i);
            ranges.push_back({i, offset, b(user.psduLength)});
            offset += b(user.psduLength);
            commonDuration = std::max(commonDuration, user.duration);
        }
        return std::shared_ptr<const Ieee80211VhtTxVector>(new Ieee80211VhtTxVector(
                channelWidth, groupId, users, ranges, preambleDuration, commonDuration));
    }

    Ieee80211VhtPpduFormat getPpduFormat() const { return ppduFormat; }
    Hz getChannelWidth() const { return channelWidth; }
    B getPsduLength() const { return psduLength; }
    uint8_t getGroupId() const { return groupId; }
    uint8_t getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
    uint8_t getMcs() const { return mcs; }
    bool isLdpcCoding() const { return ldpcCoding; }
    bool hasLdpcExtraOfdmSymbol() const { return ldpcExtraOfdmSymbol; }
    uint16_t getPartialAid() const { return partialAid; }
    bool isBeamformed() const { return beamformed; }
    double getBeamformingGainDb() const { return beamformingGainDb; }
    bool isNdp() const { return ppduFormat == VHT_NDP; }
    bool isMu() const { return ppduFormat == VHT_MU; }
    const std::vector<Ieee80211VhtMuUser>& getUsers() const { return users; }
    const std::vector<Ieee80211VhtPsduBitRange>& getPsduBitRanges() const { return psduBitRanges; }
    simtime_t getCommonDuration() const { return commonDuration; }
    simtime_t getPreambleDuration() const { return preambleDuration; }
    simtime_t getHeaderDuration() const { return headerDuration; }
    simtime_t getDataDuration() const { return dataDuration; }
    const Ieee80211VhtMuUser *findMuUser(uint8_t userPosition) const
    {
        const Ieee80211VhtMuUser *result = nullptr;
        for (const auto& user : users)
            if (user.userPosition == userPosition) {
                if (result != nullptr)
                    return nullptr;
                result = &user;
            }
        return result;
    }
};

class INET_API Ieee80211VhtTxVectorReq final : public TagBase
{
  protected:
    std::shared_ptr<const Ieee80211VhtTxVector> txVector;

  public:
    Ieee80211VhtTxVectorReq() = default;
    Ieee80211VhtTxVectorReq(const Ieee80211VhtTxVectorReq&) = default;
    virtual Ieee80211VhtTxVectorReq *dup() const override { return new Ieee80211VhtTxVectorReq(*this); }
    const std::shared_ptr<const Ieee80211VhtTxVector>& getTxVector() const { return txVector; }
    void setTxVector(const std::shared_ptr<const Ieee80211VhtTxVector>& value)
    {
        if (value == nullptr || !value->isMu())
            throw cRuntimeError("VHT MU handoff requires a canonical MU TXVECTOR");
        txVector = value;
    }
};

} // namespace physicallayer
} // namespace inet

#endif
