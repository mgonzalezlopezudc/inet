//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULTRIGGERSERVICE_H
#define __INET_HEULTRIGGERSERVICE_H

#include "inet/linklayer/ieee80211/mac/contract/IHeUlMuExchangeCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IHeUlMuSnapshotSource.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"

namespace inet {
namespace ieee80211 {

/** Validated result of resolving a scheduler policy into Trigger wire timing. */
struct INET_API HeUlScheduleFinalizationResult
{
    bool valid = false;
    IIeee80211HeUlScheduler::Schedule schedule;
    simtime_t resolvedTxTime = SIMTIME_ZERO;
    std::string error;

    explicit operator bool() const { return valid; }
};

struct INET_API HeUlCandidatePreparation
{
    IIeee80211HeUlScheduler::ScheduleContext context;
    std::vector<uint16_t> staleReportAids;
    int fullBandwidthMuMimoCandidates = 0;
    int maximumOfdmaUserNss = 1;
    int totalUlMuMimoNss = 0;
};

/** Owns AP-side periodic Trigger consideration and committed scheduling state. */
class INET_API HeUlTriggerService
{
  public:
    struct PreparedStart {
        AccessCategory accessCategory;
        IIeee80211HeUlTriggerPolicy::TriggerType triggerType;
        HeUlMuPlan plan;
        std::optional<IIeee80211HeUlScheduler::ScheduleContext> scheduleContext;

        PreparedStart(AccessCategory accessCategory,
                IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
                const HeUlMuPlan& plan,
                const std::optional<IIeee80211HeUlScheduler::ScheduleContext>& scheduleContext) :
            accessCategory(accessCategory), triggerType(triggerType), plan(plan),
            scheduleContext(scheduleContext) {}
    };

    class INET_API IActions
    {
      public:
        virtual ~IActions() {}
        virtual bool canRequestHeUlTrigger() const = 0;
        virtual bool isNdpFeedbackReportEnabled() const = 0;
        virtual const Ieee80211Mib *getHeUlMib() const = 0;
        virtual void requestHeUlChannelAccess(AccessCategory accessCategory) = 0;
        virtual void configureHeUlMuProtection(AccessCategory accessCategory) = 0;
        virtual void startHeUlMuExchange(AccessCategory accessCategory,
                const HeUlMuPlan& plan, IHeUlMuExchangeCallback *callback) = 0;
        virtual uint16_t getHeUlAssociationId(const MacAddress& address) const = 0;
        virtual const Ptr<Ieee80211CompressedBlockAck> processHeUlTriggeredBlockAckReq(
                Packet *packet, const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
                uint16_t associationId) = 0;
        virtual void processHeUlTriggeredFrame(Packet *packet,
                const Ptr<const Ieee80211DataHeader>& header, uint16_t associationId) = 0;
    };

  private:
    IActions *actions = nullptr;
    IHeUlMuExchangeCallback *exchangeCallback = nullptr;
    cSimpleModule *owner = nullptr;
    HeUlCoordinator *coordinator = nullptr;
    cMessage *triggerTimer = nullptr;
    simtime_t checkInterval = SIMTIME_ZERO;
    IIeee80211HeUlTriggerPolicy::TriggerType pendingTrigger =
            IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    bool accessRequested = false;
    std::optional<IIeee80211HeUlScheduler::ScheduleContext> committedScheduleContext;

  public:
    ~HeUlTriggerService();
    void configure(IActions *actions, IHeUlMuExchangeCallback *exchangeCallback,
            HeUlCoordinator *coordinator,
            simtime_t checkInterval);
    void start(cSimpleModule *owner);
    bool handleTimer(cMessage *message, cSimpleModule *owner);
    bool hasPendingTrigger() const;
    std::optional<PreparedStart> prepareStart(AccessCategory accessCategory,
            const HeUlPreparationSnapshot& snapshot);
    bool commitStart(const PreparedStart& preparedStart);
    static HeUlScheduleFinalizationResult finalizeSchedule(
            const IIeee80211HeUlScheduler::Schedule& proposedSchedule,
            Hz centerFrequency, Hz channelBandwidth,
            IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
            physicallayer::Ieee80211HeTriggerResponseFinalizationResult *finalizationSnapshot = nullptr);
    static HeUlCandidatePreparation buildCandidates(
            const HeUlPreparationSnapshot& snapshot);
    const char *getPendingTriggerName() const;

    uint32_t allocateTriggerId();
    void planCommitted(const HeUlMuPlan& plan, uint32_t triggerId);
};

} // namespace ieee80211
} // namespace inet

#endif
