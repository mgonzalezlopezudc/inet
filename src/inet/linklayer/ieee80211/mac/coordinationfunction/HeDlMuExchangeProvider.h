//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUEXCHANGEPROVIDER_H
#define __INET_HEDLMUEXCHANGEPROVIDER_H

#include <map>
#include <set>

#include "inet/linklayer/ieee80211/mac/contract/IHeDlMuExchangeCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IHeDlMuSnapshotSource.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuPlan.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingService.h"

namespace inet {
namespace ieee80211 {

/** Owns HE DL MU transaction correlation and rejects late/duplicate outcomes. */
class INET_API HeDlMuExchangeProvider
{
  public:
    class INET_API ReservationRollbackGuard
    {
      private:
        HeDlMuExchangeProvider *provider = nullptr;
        uint64_t transactionToken = 0;

      public:
        ReservationRollbackGuard(HeDlMuExchangeProvider& provider, uint64_t transactionToken);
        ReservationRollbackGuard(const ReservationRollbackGuard&) = delete;
        ReservationRollbackGuard& operator=(const ReservationRollbackGuard&) = delete;
        ~ReservationRollbackGuard();
        void release() { provider = nullptr; }
    };

    struct PreparationResult {
        std::optional<HeDlMuPlan> plan;
        HeMuPlanDiagnostic diagnostic;
    };

    struct StartupParameters {
        int maxAmpduMpduCount = 0;
        int maxHeMuPsduLength = 0;
        simtime_t maxHeMuPpduDuration = SIMTIME_ZERO;
    };

    struct HeDlMuProtectionSnapshot {
        enum class Mechanism {
            SINGLE_PROTECTION,
            MULTIPLE_PROTECTION,
            UNDEFINED_PROTECTION,
        };
        enum class InitialProtection {
            NONE,
            LEGACY_RTS_CTS,
        };
        Mechanism mechanism = Mechanism::UNDEFINED_PROTECTION;
        InitialProtection protection = InitialProtection::NONE;
        bool configured = false;
        bool completed = false;
    };

    enum class StartKind {
        HE_SOUNDING,
        RECOVERY_SINGLE_USER,
        SINGLE_USER_FALLBACK,
        ADDBA_SINGLE_USER,
        HE_DL_MULTIUSER,
    };

    struct PreparedStart {
        StartKind kind = StartKind::SINGLE_USER_FALLBACK;
        AccessCategory accessCategory = AC_BE;
        std::optional<HeSoundingService::StartAction> soundingAction;
        std::optional<HeDlMuPlan> plan;
        HcfQueueToken stageQueueToken;
        HcfPacketIdentity stagePacketIdentity;
        bool startSingleUser = true;
        HeDlMuTxOpFs::AckMethod ackMethod = HeDlMuTxOpFs::AckMethod::MU_BAR_TRIGGER;
        StartupParameters parameters;
        IIeee80211HeDlScheduler *scheduler = nullptr;
    };

    class INET_API IActions : public IHeDlMuExchangeCallback
    {
      public:
        virtual void heDlMuMemberTransmitted(uint64_t transactionToken,
                const HeDlMuMember& member) override = 0;
        virtual void heDlMuUserOutcome(uint64_t transactionToken,
                const MacAddress& peer, HeDlMuUserOutcome outcome) override = 0;
        virtual bool stageHeDlMuPacket(HcfQueueToken queueToken,
                HcfPacketIdentity packetIdentity, AccessCategory accessCategory) = 0;
        virtual bool startHeDlMuSingleUserIfEligible(AccessCategory accessCategory) = 0;
        virtual HeDlMuProtectionSnapshot captureHeDlMuProtection(
                AccessCategory accessCategory) const = 0;
        virtual void configureHeDlMuProtection(AccessCategory accessCategory) = 0;
        virtual void restoreHeDlMuProtection(AccessCategory accessCategory,
                const HeDlMuProtectionSnapshot& snapshot) = 0;
        virtual bool startHeDlMuExchange(AccessCategory accessCategory,
                const HeDlMuPlan& plan, uint64_t transactionToken,
                HeDlMuTxOpFs::AckMethod ackMethod,
                const StartupParameters& parameters) = 0;
    };

  private:
    struct FallbackCandidate {
        HcfQueueToken queueToken;
        HcfPacketIdentity packetIdentity;
    };

    enum class StartPhase {
        IDLE,
        ACTIVE,
    };

    IActions *actions = nullptr;
    HeSoundingService *soundingProvider = nullptr;
    StartPhase startPhase = StartPhase::IDLE;
    std::optional<AccessCategory> pendingProtectionAccessCategory;
    std::optional<HeDlMuProtectionSnapshot> pendingProtectionSnapshot;
    uint64_t activeTransactionToken = 0;
    Packet *containerPacket = nullptr;
    std::vector<HeDlMuMember> members;
    std::set<const Packet *> transmittedMembers;
    std::set<MacAddress> completedUsers;
    std::map<MacAddress, std::vector<Packet *>> reservedPackets;
    IIeee80211HeDlScheduler *pendingScheduler = nullptr;
    IIeee80211HeDlScheduler::ScheduleContext pendingScheduleContext;
    std::vector<IIeee80211HeDlScheduler::RuAllocation> pendingAllocations;
    bool forceNextSingleUser[4] = {};
    uint64_t nextTransactionToken = 1;

    static FallbackCandidate selectFallbackCandidate(
            const HeDlMuPreparationSnapshot& snapshot);
    void restorePendingProtection();

  public:
    void configure(IActions *actions, HeSoundingService *soundingProvider);
    static IIeee80211HeDlScheduler::ScheduleContext buildCandidateContext(
            const IIeee80211HeDlScheduler::ScheduleContext& snapshotContext);
    static IIeee80211HeDlScheduler::ScheduleContext buildScheduleContext(
            const HeDlMuPreparationSnapshot& snapshot);
    static HcfQueueToken selectOldestEligibleQueue(
            const HeDlMuPreparationSnapshot& snapshot);
    static PreparationResult preparePlan(
            const IIeee80211HeDlScheduler::ScheduleContext& snapshotContext,
            IIeee80211HeDlScheduler& scheduler);
    std::optional<PreparedStart> prepareStart(AccessCategory accessCategory,
            const HeDlMuPreparationSnapshot& snapshot,
            IIeee80211HeDlScheduler& scheduler,
            const StartupParameters& parameters);
    std::optional<PreparedStart> prepareSingleUserStart(
            AccessCategory accessCategory,
            const HeDlMuPreparationSnapshot& snapshot) const;
    bool commitStart(const PreparedStart& preparedStart);
    bool tryStart(AccessCategory accessCategory,
            const HeDlMuPreparationSnapshot& snapshot,
            IIeee80211HeDlScheduler& scheduler,
            const StartupParameters& parameters);
    bool hasForcedSingleUser(AccessCategory accessCategory) const;
    bool consumeForcedSingleUser(AccessCategory accessCategory);
    bool reservePlan(const HeDlMuPlan& plan, uint64_t transactionToken);
    void rollbackReservation(uint64_t transactionToken);
    void finalizeReservation(uint64_t transactionToken,
            const std::vector<HeDlMuMember>& members);
    bool isActiveContainer(const Packet *packet) const;
    /** Dispatches the committed members for an active transmitted container. */
    bool routeTransmittedContainer(Packet *packet, bool notifyActions = true);
    uint64_t getActiveTransactionToken() const { return activeTransactionToken; }
    const std::vector<HeDlMuMember>& getActiveMembers() const { return members; }

    queueing::IPacketQueue *resolveHeDlMuQueue(HcfQueueToken token) const;
    virtual Packet *getReservedHeDlMuPacket(uint64_t transactionToken,
            const MacAddress& peer) const;
    virtual bool isReservedHeDlMuPacket(uint64_t transactionToken,
            const MacAddress& peer, const Packet *packet) const;
    IOriginatorBlockAckAgreementHandler *getHeDlMuBlockAckHandler() const;
    IOriginatorMacDataService *getHeDlMuOriginatorDataService() const;
    IQosRateSelection *getHeDlMuRateSelection() const;
    MacAddress getHeDlMuTransmitterAddress() const;
    int getHeDlMuFcsMode() const;
    uint8_t getHeDlMuBssColor() const;
    uint16_t getHeDlMuAssociationId(const MacAddress& peer) const;
    std::optional<Ieee80211NegotiatedHeCapabilities>
            getHeDlMuNegotiatedCapabilities(const MacAddress& peer) const;
    void heDlMuPlanCommitted(uint64_t transactionToken,
            Packet *containerPacket, const std::vector<HeDlMuMember>& members);
    bool heDlMuMemberTransmitted(uint64_t transactionToken,
            const HeDlMuMember& member, bool notifyActions = true);
    bool heDlMuUserOutcome(uint64_t transactionToken,
            const MacAddress& peer, HeDlMuUserOutcome outcome, bool notifyActions = true);
};

} // namespace ieee80211
} // namespace inet

#endif
