//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEE80211HEDLSCHEDULER_H
#define __INET_IIEE80211HEDLSCHEDULER_H

#include <ostream>
#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

class HeMuMimoCsiManager;

using namespace inet::units::values;

/**
 * Abstract interface for IEEE 802.11ax Downlink OFDMA resource unit schedulers.
 * Given a list of candidate STAs and the channel parameters, returns a per-STA
 * RU assignment.
 */
class INET_API IIeee80211HeDlScheduler
{
  public:
    /** Queue, link-estimate, and negotiated-capability data for one DL candidate STA. */
    struct CandidateInfo {
        MacAddress staAddress;
        AccessCategory accessCategory = AC_BE;
        bool anchor = false;
        int64_t backlogBytes = 0;
        int64_t holPacketBytes = 0;
        simtime_t holEnqueueTime = SIMTIME_ZERO;
        simtime_t holDelay = SIMTIME_ZERO;
        double pathLossDb = NaN;
        bool hasFreshPathLoss = false;
        queueing::IPacketQueue *sourceQueue = nullptr;
        const Ieee80211NegotiatedHeCapabilities *negotiatedHeCapabilities = nullptr;
    };

    /** Shared AP, channel, TXOP, and PHY constraints presented to a DL scheduler. */
    struct ScheduleContext {
        std::vector<CandidateInfo> candidates;
        MacAddress anchorSta;
        Hz channelCenterFrequency = Hz(NaN);
        Hz channelBandwidth = Hz(NaN);
        int channelNumber = -1;
        simtime_t txopLimit = SIMTIME_ZERO;
        int maxAmpduMpduCount = 16;
        W totalTransmitPower = W(NaN);
        W receiverSensitivity = W(NaN);
        double noiseFigureDb = 0;
        physicallayer::Ieee80211HeGuardInterval guardInterval = physicallayer::HE_GI_3_2_US;
        physicallayer::Ieee80211HeCoding coding = physicallayer::HE_CODING_BCC;
        int packetExtensionDurationUs = 0;
        uint8_t puncturedSubchannelMask = 0;
        std::vector<bool> puncturedSubchannels;
        const HeMuMimoCsiManager *csiManager = nullptr;
        int numApAntennas = 1;
    };

    /** One scheduled STA's RU and selected PHY parameters. */
    struct RuAllocation {
        MacAddress staAddress;
        physicallayer::Ieee80211HeRu ru;
        int mcs = 0;
        int numberOfSpatialStreams = 1;
        bool dcm = false;
        double estimatedSnrDb = NaN;
        simtime_t estimatedDuration = SIMTIME_ZERO;
    };

    virtual ~IIeee80211HeDlScheduler() {}

    /**
     * Schedule RU allocations for the given candidate STAs.
     *
     * @param candidates       Ordered list of destination MAC addresses.
     * @param channelCenterFrequency  Center frequency of the primary channel.
     * @param channelBandwidth        Bandwidth of the primary channel.
     * @return  One RuAllocation per selected STA. May return fewer entries
     *          than candidates if maxMuStations is exceeded.
     */
    virtual std::vector<RuAllocation> schedule(const std::vector<MacAddress>& candidates,
            Hz channelCenterFrequency, Hz channelBandwidth) = 0;

    virtual std::vector<RuAllocation> schedule(const ScheduleContext& context)
    {
        std::vector<MacAddress> candidates;
        for (const auto& candidate : context.candidates)
            candidates.push_back(candidate.staAddress);
        return schedule(candidates, context.channelCenterFrequency, context.channelBandwidth);
    }
};

inline std::ostream& operator<<(std::ostream& os, const IIeee80211HeDlScheduler::CandidateInfo& candidate)
{
    os << "sta=" << candidate.staAddress
       << " ac=" << (int)candidate.accessCategory
       << " anchor=" << (candidate.anchor ? "yes" : "no")
       << " backlog=" << candidate.backlogBytes
       << " holBytes=" << candidate.holPacketBytes
       << " holDelay=" << candidate.holDelay
       << " pathLoss=" << (candidate.hasFreshPathLoss ? candidate.pathLossDb : NaN);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const IIeee80211HeDlScheduler::RuAllocation& allocation)
{
    os << "sta=" << allocation.staAddress
       << " ru={" << allocation.ru << "}"
       << " mcs=" << allocation.mcs
       << " nss=" << allocation.numberOfSpatialStreams
       << " dcm=" << (allocation.dcm ? "yes" : "no")
       << " snr=" << allocation.estimatedSnrDb
       << " duration=" << allocation.estimatedDuration;
    return os;
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_IIEE80211HEDLSCHEDULER_H
