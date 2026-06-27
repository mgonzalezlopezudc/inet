//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULCOORDINATOR_H
#define __INET_HEULCOORDINATOR_H

#include <array>
#include <map>
#include <ostream>
#include <string>

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

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
    /** Most recent backlog and retry information reported by one associated STA. */
    struct BufferStatus {
        std::array<int64_t, 4> backlogBytes = {};
        std::array<uint8_t, 4> tid = {};
        simtime_t updateTime = SIMTIME_ZERO;
        bool retryPending = false;
        simtime_t lastService = SIMTIME_ZERO;
        std::array<int64_t, 4> scheduledBytes = {};
    };

  protected:
    bool enabled = false;
    simtime_t reportMaxAge;
    int ocwMin = 7;
    int ocwMax = 31;
    int ofdmaContentionWindow = 7;
    int ofdmaBackoff = 0;
    uint32_t nextTriggerId = 1;
    simtime_t lastTriggerTime = SIMTIME_ZERO;
    bool hasSentTrigger = false;
    std::map<uint16_t, BufferStatus> bufferStatusByAid;
    IIeee80211HeUlScheduler *scheduler = nullptr;
    IIeee80211HeUlTriggerPolicy *triggerPolicy = nullptr;
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

  protected:
    virtual void initialize(int stage) override;
    virtual int getFreshReportCount() const;
    virtual int getBackloggedReportCount() const;
    virtual std::string getBufferStatusSummary() const;

  public:
    bool isEnabled() const { return enabled; }
    simtime_t getReportMaxAge() const { return reportMaxAge; }
    void updateBufferStatus(uint16_t aid, AccessCategory ac, uint8_t tid,
            int64_t backlogBytes, bool retryPending);
    void clearStation(uint16_t aid);
    IIeee80211HeUlTriggerPolicy::TriggerType selectTrigger(const Ieee80211Mib *mib) const;
    AccessCategory getPreferredAccessCategory() const;
    IIeee80211HeUlScheduler::Schedule createSchedule(const Ieee80211Mib *mib,
            Hz centerFrequency, Hz bandwidth, simtime_t txopLimit,
            double sensitivityDbm, double targetRssiMarginDb,
            int estimatedRaContenders, double collisionRate, double idleRate);
    uint32_t allocateTriggerId();
    void noteTriggerSent(IIeee80211HeUlTriggerPolicy::TriggerType triggerType);
    int selectRandomAccessRu(int randomAccessRuCount);
    void reportRandomAccessResult(bool success);
    const std::map<uint16_t, BufferStatus>& getBufferStatus() const { return bufferStatusByAid; }
};

inline std::ostream& operator<<(std::ostream& os, const HeUlCoordinator::BufferStatus& status)
{
    os << "backlog=[" << status.backlogBytes[0] << "," << status.backlogBytes[1] << "," 
       << status.backlogBytes[2] << "," << status.backlogBytes[3] << "]"
       << " update=" << status.updateTime << " retry=" << (status.retryPending ? "yes" : "no");
    return os;
}

} // namespace ieee80211
} // namespace inet

#endif
