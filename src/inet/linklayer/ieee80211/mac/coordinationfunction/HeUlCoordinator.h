//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULCOORDINATOR_H
#define __INET_HEULCOORDINATOR_H

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

class INET_API HeUlTriggerDecisionEvent : public cObject
{
  public:
    enum Reason {
        BACKLOG_REPORTED,
        REPORT_REFRESH_NEEDED,
        NDP_FEEDBACK_ENABLED,
    };

    struct UserInfo {
        uint16_t associationId = 0;
        bool retryPending = false;
        int64_t backlogBytes = 0;
        int64_t reportedBytes = 0;
        int64_t plannedBytes = 0;
        // Compatibility alias for pre-capacity-aware listeners.
        int64_t selectedBytes = 0;
        uint8_t tid = 0;
        AccessCategory accessCategory = AC_BE;
        bool selected = false;
        int ruIndex = -1;
        int ruToneSize = 0;
        int ruToneOffset = 0;
    };

    IIeee80211HeUlTriggerPolicy::TriggerType triggerType =
            IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    uint32_t triggerId = 0;
    Reason reason = REPORT_REFRESH_NEEDED;
    std::vector<UserInfo> users;
};

/**
 * AP-side coordinator for HE trigger-based uplink OFDMA.
 *
 * It caches per-AID buffer-status reports, obtains Trigger and RU decisions
 * from the installed policies, and maintains UORA contention state. HeHcf
 * owns this module and uses it to construct an HeUlMuTxOpFs exchange.
 */
class INET_API HeUlCoordinator : public SimpleModule
{
  public:
    struct PreparedRandomAccessSelection {
        AccessCategory accessCategory = AC_BE;
        int randomAccessRuCount = 0;
        int originalBackoff = 0;
        int resultingBackoff = 0;
        int selectedRu = -1;
        bool attempt = false;
    };

    /** Most recent backlog information reported by one associated STA. */
    struct BufferStatus {
        MacAddress stationAddress;
        std::array<int64_t, 4> backlogBytes = {};
        std::array<Ieee80211HeQueueSizeEstimate, 4> backlogEstimates;
        std::array<uint8_t, 4> tid = {};
        simtime_t updateTime = SIMTIME_ZERO;
        // Retained for source compatibility; coordinator-owned values stay false.
        std::array<bool, 4> retryPending = {};
        simtime_t lastService = SIMTIME_ZERO;
        std::array<int64_t, 4> scheduledBytes = {};
    };

  protected:
    bool enabled = false;
    simtime_t reportMaxAge;
    int ocwMin = 7;
    int ocwMax = 31;
    int ofdmaContentionWindow = 0;
    int ofdmaBackoff = 0;
    simtime_t lastTriggerTime = SIMTIME_ZERO;
    bool hasSentTrigger = false;
    std::map<uint16_t, BufferStatus> bufferStatusByAid;
    IIeee80211HeUlScheduler *scheduler = nullptr;
    IIeee80211HeUlTriggerPolicy *triggerPolicy = nullptr;
    std::vector<HeUlTriggerDecisionEvent::UserInfo> committedBasicTriggerUsers;
    simsignal_t basicTriggerSentSignal;
    simsignal_t bsrpTriggerSentSignal;
    simsignal_t bufferStatusUpdatedSignal;
    simsignal_t bufferStatusReportedBytesSignal;
    simsignal_t bufferStatusScheduledBytesSignal;
    simsignal_t staleReportSignal;
    simsignal_t scheduledUsersSignal;
    simsignal_t randomAccessRusSignal;
    simsignal_t randomAccessAttemptSignal;
    simsignal_t randomAccessSuccessSignal;
    simsignal_t triggerDecisionCommittedSignal;
    simsignal_t triggerDecisionIdSignal;

  protected:
    virtual void initialize(int stage) override;
    virtual int getFreshReportCount() const;
    virtual int getBackloggedReportCount() const;
    virtual std::string getBufferStatusSummary() const;

  public:
    bool isEnabled() const { return enabled; }
    simtime_t getReportMaxAge() const { return reportMaxAge; }
    void updateBufferStatus(uint16_t aid, const MacAddress& stationAddress,
            AccessCategory ac, uint8_t tid,
            int64_t backlogBytes);
    void updateBufferStatus(uint16_t aid, const MacAddress& stationAddress,
            AccessCategory ac, uint8_t tid,
            const Ieee80211HeQueueSizeEstimate& estimate);
    void updateBufferStatus(uint16_t aid, const MacAddress& stationAddress,
            AccessCategory ac, uint8_t tid,
            int64_t backlogBytes, bool receivedRetry);
    void clearStation(const MacAddress& stationAddress);
    void invalidatePeer(const MacAddress& stationAddress);
    IIeee80211HeUlTriggerPolicy::TriggerType selectTrigger(const Ieee80211Mib *mib) const;
    AccessCategory getPreferredAccessCategory() const;
    IIeee80211HeUlScheduler::Schedule prepareSchedule(const Ieee80211Mib *mib,
            const IIeee80211HeLinkPhyContext& linkPhyContext, simtime_t maximumLinkEstimateAge,
            Hz centerFrequency, Hz bandwidth, simtime_t txopLimit, simtime_t requestedDuration,
            double sensitivityDbm, double targetRssiMarginDb,
            int estimatedRaContenders, double collisionRate, double idleRate,
            const std::function<bool(const MacAddress&)>& isUlMuDisabled = {},
            IIeee80211HeUlScheduler::ScheduleContext *preparedContext = nullptr,
            const physicallayer::Ieee80211HeTbCapacityBoundary *finalizedBoundary = nullptr,
            bool useUlMuMimoPolicy = false);
    IIeee80211HeUlScheduler::Schedule prepareSchedule(
            const IIeee80211HeUlScheduler::ScheduleContext& context,
            const std::vector<uint16_t>& staleReportAids);
    void commitSchedule(const IIeee80211HeUlScheduler::ScheduleContext& context,
            const IIeee80211HeUlScheduler::Schedule& schedule);
    uint32_t allocateTriggerId();
    void noteTriggerSent(IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
            uint32_t triggerId);
    PreparedRandomAccessSelection prepareRandomAccessRu(
            AccessCategory ac, int randomAccessRuCount);
    int commitRandomAccessRu(const PreparedRandomAccessSelection& selection);
    int selectRandomAccessRu(AccessCategory ac, int randomAccessRuCount);
    int selectRandomAccessRu(int randomAccessRuCount) { return selectRandomAccessRu(AC_BE, randomAccessRuCount); }
    int getRandomAccessBackoff() const { return ofdmaBackoff; }
    void reportRandomAccessResult(AccessCategory ac, bool success);
    void reportRandomAccessResult(bool success) { reportRandomAccessResult(AC_BE, success); }
    const std::map<uint16_t, BufferStatus>& getBufferStatus() const { return bufferStatusByAid; }
    std::optional<BufferStatus> getBufferStatusSnapshot(uint16_t associationId,
            const MacAddress& stationAddress) const;
};

inline std::ostream& operator<<(std::ostream& os, const HeUlCoordinator::BufferStatus& status)
{
    os << "backlog=[" << status.backlogBytes[0] << "," << status.backlogBytes[1] << "," 
       << status.backlogBytes[2] << "," << status.backlogBytes[3] << "]"
       << " update=" << status.updateTime;
    return os;
}

} // namespace ieee80211
} // namespace inet

#endif
