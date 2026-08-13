//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEE80211HEULSCHEDULER_H
#define __INET_IIEE80211HEULSCHEDULER_H

#include <array>
#include <cmath>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211HeBsr.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Interface for schedulers of IEEE 802.11ax trigger-based uplink OFDMA.
 *
 * Given AP-side buffer reports and channel constraints, implementations choose
 * scheduled and random-access RUs and a common HE-TB transmission duration.
 */
class INET_API IIeee80211HeUlScheduler
{
  public:
    struct NfrpResponseResource {
        bool scheduled = false;
        uint8_t toneSetIndex = 0;
        uint8_t startingStsNumber = 0;
    };

    static int getNfrpToneSetsPerSpatialStream(Hz channelBandwidth)
    {
        if (channelBandwidth != MHz(20) && channelBandwidth != MHz(40) &&
                channelBandwidth != MHz(80) && channelBandwidth != MHz(160))
            throw cRuntimeError("Unsupported NFRP channel bandwidth");
        return 18 * std::lround(channelBandwidth.get() / 20e6);
    }

    static int getNfrpScheduledStaCount(Hz channelBandwidth, bool multiplexingFlag)
    {
        return getNfrpToneSetsPerSpatialStream(channelBandwidth) * (multiplexingFlag ? 2 : 1);
    }

    static NfrpResponseResource getNfrpResponseResource(uint16_t startingAid,
            uint16_t aid, Hz channelBandwidth, bool multiplexingFlag)
    {
        NfrpResponseResource resource;
        const int toneSets = getNfrpToneSetsPerSpatialStream(channelBandwidth);
        const int offset = aid - startingAid;
        if (offset < 0 || offset >= getNfrpScheduledStaCount(channelBandwidth, multiplexingFlag))
            return resource;
        resource.scheduled = true;
        resource.toneSetIndex = 1 + offset % toneSets;
        resource.startingStsNumber = offset / toneSets;
        return resource;
    }

    /** Latest AP-side traffic and link information for one associated STA. */
    struct CandidateInfo {
        MacAddress staAddress;
        uint16_t associationId = 0;
        std::array<int64_t, 4> backlogBytes = {};
        std::array<Ieee80211HeQueueSizeEstimate, 4> backlogEstimates;
        bool hasTypedBacklogEstimates = false;
        AccessCategory selectedAccessCategory = AC_BE;
        uint8_t selectedTid = 0;
        simtime_t reportAge = SIMTIME_MAX;
        bool hasFreshReport = false;
        bool retryPending = false;
        bool anchor = false;
        double pathLossDb = NaN;
        bool hasFreshPathLoss = false;
        bool hasNegotiatedHeCapabilities = false;
        Ieee80211NegotiatedHeCapabilities negotiatedHeCapabilities;
        bool ulMuDisabled = false;
        simtime_t lastService = SIMTIME_ZERO;
        physicallayer::Ieee80211HeCoding coding = physicallayer::HE_CODING_BCC;

        int64_t getSelectedBacklogBytes() const {
            return hasTypedBacklogEstimates ?
                    std::min<uint64_t>(backlogEstimates[selectedAccessCategory].getConservativeBytes(),
                            std::numeric_limits<int64_t>::max()) :
                    backlogBytes[selectedAccessCategory];
        }

        bool isUnknownProbe() const {
            return hasTypedBacklogEstimates &&
                    backlogEstimates[selectedAccessCategory].kind ==
                            Ieee80211HeQueueSizeKind::UNKNOWN;
        }
    };

    /** Channel, TXOP, receiver, and random-access observations for one Trigger decision. */
    struct ScheduleContext {
        std::vector<CandidateInfo> candidates;
        Hz channelCenterFrequency = Hz(NaN);
        Hz channelBandwidth = Hz(NaN);
        simtime_t txopLimit = SIMTIME_ZERO;
        simtime_t requestedDuration = SimTime(1, SIMTIME_MS);
        physicallayer::Ieee80211HeTbCapacityBoundary finalizedBoundary;
        double apSensitivityDbm = -85;
        double targetRssiMarginDb = 10;
        int estimatedRandomAccessContenders = 0;
        double recentRandomAccessCollisionRate = 0;
        double recentRandomAccessIdleRate = 0;
        bool useUlMuMimoPolicy = false;
    };

    /** One scheduled or random-access RU in a trigger-based uplink PPDU. */
    struct RuAllocation {
        MacAddress staAddress;
        uint16_t associationId = 0;
        uint8_t tid = 0;
        AccessCategory accessCategory = AC_BE;
        physicallayer::Ieee80211HeRu ru;
        bool randomAccess = false;
        int mcs = 0;
        int numberOfSpatialStreams = 1;
        int streamStartIndex = 0;
        bool muMimo = false;
        physicallayer::Ieee80211HeCoding coding = physicallayer::HE_CODING_BCC;
        int targetRssiDbm = -75;
        bool useMaximumTransmitPower = false;
        simtime_t estimatedDuration = SIMTIME_ZERO;
        int64_t plannedBytes = 0;
    };

    /** Complete Trigger allocation plus PHY parameters common to all HE-TB users. */
    struct Schedule {
        std::vector<RuAllocation> allocations;
        // Coordinator-owned observation metadata. This is model-only state,
        // is overwritten after the extension scheduler returns, and is
        // emitted only when the validated schedule commits.
        std::vector<uint16_t> staleReportAids;
        // NFRP Trigger type 7 uses one range User Info record, not the
        // ordinary per-STA RU allocations above.
        uint16_t nfrpStartingAid = 0;
        uint8_t nfrpFeedbackType = 0;
        int nfrpTargetRssiDbm = -75;
        bool nfrpUseMaximumTransmitPower = false;
        bool nfrpMultiplexingFlag = false;
        Hz channelBandwidth = Hz(NaN);
        uint16_t ulLength = 0;
        simtime_t commonDuration = SIMTIME_ZERO;
        bool commonDurationExact = false;
        bool noSignalExtension = false;
        physicallayer::Ieee80211HeGuardInterval guardInterval = physicallayer::HE_GI_3_2_US;
        physicallayer::Ieee80211HeLtfType ltfType = physicallayer::HE_LTF_4X;
        physicallayer::Ieee80211HeCoding coding = physicallayer::HE_CODING_BCC;
        bool ldpcExtraSymbolSegment = false;
        int preFecPaddingFactor = 4;
        bool peDisambiguity = false;
        int numberOfHeLtfSymbols = 1;
        int apTxPowerDbm = 0;
        int packetExtensionDurationUs = 0;
        uint8_t puncturedSubchannelMask = 0;
        bool exactOptimization = false;
        std::string decisionReason;
        int64_t totalPlannedBytes = 0;
    };

    /**
     * Bytes added to a queued MAC packet when it is placed into an HE-TB
     * A-MPDU. The BSR backlog is measured with Packet::getByteLength(), so
     * the queued packet's MAC header and trailer are already included. Only
     * the A-MPDU delimiter and, when the queued header has not yet acquired
     * HT Control, the four-byte HT Control field are additional here.
     */
    static constexpr int64_t getHeTbQueuedPacketOverheadBytes(bool hasBufferStatus)
    {
        return 4 + (hasBufferStatus ? 0 : 4);
    }

    virtual ~IIeee80211HeUlScheduler() {}
    virtual Schedule schedule(const ScheduleContext& context) = 0;

    /** Records a schedule only after its validated Trigger plan commits. */
    virtual void commitSchedule(const ScheduleContext& context,
            const Schedule& schedule) {}

    /**
     * Discards scheduler-owned state derived from a peer association epoch.
     * Stateless extension schedulers may retain the default no-op.
     */
    virtual void invalidatePeer(const MacAddress& peer) {}
};

inline std::ostream& operator<<(std::ostream& os, const IIeee80211HeUlScheduler::CandidateInfo& candidate)
{
    os << "aid=" << candidate.associationId
       << " sta=" << candidate.staAddress
       << " ac=" << (int)candidate.selectedAccessCategory
       << " tid=" << (int)candidate.selectedTid
       << " backlog=" << candidate.getSelectedBacklogBytes()
       << " reportAge=" << candidate.reportAge
       << " retry=" << (candidate.retryPending ? "yes" : "no")
       << " anchor=" << (candidate.anchor ? "yes" : "no");
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const IIeee80211HeUlScheduler::RuAllocation& allocation)
{
    os << (allocation.randomAccess ? "RA" : "scheduled")
       << " aid=" << allocation.associationId
       << " sta=" << allocation.staAddress
       << " tid=" << (int)allocation.tid
       << " ac=" << (int)allocation.accessCategory
       << " ru={" << allocation.ru << "}"
       << " mcs=" << allocation.mcs
       << " nss=" << allocation.numberOfSpatialStreams
       << " stream=" << allocation.streamStartIndex
       << " targetRssi=" << allocation.targetRssiDbm
       << " duration=" << allocation.estimatedDuration;
    os << " coding=" << (allocation.coding == physicallayer::HE_CODING_LDPC ? "LDPC" : "BCC")
       << " plannedBytes=" << allocation.plannedBytes;
    return os;
}

} // namespace ieee80211
} // namespace inet

#endif
