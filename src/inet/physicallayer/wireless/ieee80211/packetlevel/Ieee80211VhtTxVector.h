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

inline int getIeee80211VhtMuSigBLogicalBitsPerUser(Hz channelWidth)
{
    // IEEE Std 802.11-2024, Table 21-14: VHT-SIG-B MU logical field widths.
    if (channelWidth == MHz(20)) return 26;
    if (channelWidth == MHz(40)) return 27;
    if (channelWidth == MHz(80) || channelWidth == MHz(160)) return 29;
    throw cRuntimeError("VHT MU SIG-B requires a 20, 40, 80, or 160 MHz channel");
}

inline b getIeee80211VhtMuPhyHeaderLength(Hz channelWidth, size_t userCount)
{
    if (userCount < 2 || userCount > 4)
        throw cRuntimeError("VHT MU user count must be in the range 2..4");
    return b(48 + getIeee80211VhtMuSigBLogicalBitsPerUser(channelWidth) * userCount);
}

/**
 * Immutable packet-level authority for VHT SU/NDP/MU signaling and timing.
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
    const bool shortGuardInterval;
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
            simtime_t preambleDuration, simtime_t commonDuration,
            bool shortGuardInterval, bool ldpcExtraOfdmSymbol) :
        ppduFormat(VHT_MU), channelWidth(channelWidth),
        psduLength(psduBitRanges.empty() ? B(0) :
                B(psduBitRanges.back().getEndBitOffset())),
        groupId(groupId), numberOfSpaceTimeStreams([&users] {
            unsigned int total = 0;
            for (const auto& user : users)
                total += user.numberOfSpatialStreams;
            return static_cast<uint8_t>(total);
        }()), mcs(0), ldpcCoding(false),
        ldpcExtraOfdmSymbol(ldpcExtraOfdmSymbol),
        shortGuardInterval(shortGuardInterval), partialAid(0),
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
        shortGuardInterval(false),
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
    }

    static std::shared_ptr<const Ieee80211VhtTxVector> createMu(
            Hz channelWidth, uint8_t groupId,
            const std::vector<Ieee80211VhtMuUser>& users,
            simtime_t preambleDuration, bool shortGuardInterval = false,
            bool ldpcExtraOfdmSymbol = false)
    {
        // IEEE Std 802.11-2024, Table 21-1: VHT MU uses GID 1..62 and 2..4
        // active users; Table 21-12 carries four independently populated positions.
        getIeee80211VhtBandwidthCode(channelWidth);
        if (groupId < 1 || groupId > 62 || users.size() < 2 || users.size() > 4 ||
                preambleDuration <= SIMTIME_ZERO)
            throw cRuntimeError("VHT MU requires GID 1..62, 2..4 users, and a positive preamble duration");
        std::set<int> positions;
        std::vector<Ieee80211VhtPsduBitRange> ranges;
        b offset = b(0);
        simtime_t commonDuration = SIMTIME_ZERO;
        unsigned int totalNsts = 0;
        bool hasLdpcUser = false;
        for (size_t i = 0; i < users.size(); ++i) {
            const auto& user = users[i];
            if (user.associationId == 0 ||
                    user.userPosition > 3 || !positions.insert(user.userPosition).second ||
                    user.numberOfSpatialStreams < 1 || user.numberOfSpatialStreams > 4 || user.mcs > 9 ||
                    user.psduLength <= B(0) || user.psduLength.get<B>() % 4 != 0 ||
                    user.psduLength > B(1048572) ||
                    user.duration <= SIMTIME_ZERO || !std::isfinite(user.beamformingGainDb) ||
                    user.beamformingGainDb < 0 || !std::isfinite(user.leakagePenaltyDb) ||
                    user.leakagePenaltyDb < 0)
                throw cRuntimeError("Invalid VHT MU user at index %zu", i);
            ranges.push_back({i, offset, b(user.psduLength)});
            offset += b(user.psduLength);
            commonDuration = std::max(commonDuration, user.duration);
            totalNsts += user.numberOfSpatialStreams;
            hasLdpcUser |= user.ldpcCoding;
        }
        // IEEE Std 802.11-2024, 21.1.1 and Table 21-1: at most 4 NSTS per
        // user and 8 NSTS summed over all users.
        if (totalNsts == 0 || totalNsts > 8)
            throw cRuntimeError("VHT MU total NSTS must be in the range 1..8");
        if (ldpcExtraOfdmSymbol && !hasLdpcUser)
            throw cRuntimeError("VHT MU LDPC Extra OFDM Symbol requires an LDPC-coded user");
        return std::shared_ptr<const Ieee80211VhtTxVector>(new Ieee80211VhtTxVector(
                channelWidth, groupId, users, ranges, preambleDuration, commonDuration,
                shortGuardInterval, ldpcExtraOfdmSymbol));
    }

    Ieee80211VhtPpduFormat getPpduFormat() const { return ppduFormat; }
    Hz getChannelWidth() const { return channelWidth; }
    B getPsduLength() const { return psduLength; }
    uint8_t getGroupId() const { return groupId; }
    uint8_t getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
    uint8_t getMcs() const { return mcs; }
    bool isLdpcCoding() const { return ldpcCoding; }
    bool hasLdpcExtraOfdmSymbol() const { return ldpcExtraOfdmSymbol; }
    bool hasShortGuardInterval() const { return shortGuardInterval; }
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
    const Ieee80211VhtPsduBitRange *findMuPsduBitRange(uint8_t userPosition) const
    {
        for (size_t i = 0; i < users.size(); ++i)
            if (users[i].userPosition == userPosition)
                return &psduBitRanges.at(i);
        return nullptr;
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
