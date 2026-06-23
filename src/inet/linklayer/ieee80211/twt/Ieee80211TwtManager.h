//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211TWTMANAGER_H
#define __INET_IEEE80211TWTMANAGER_H

#include <map>
#include <vector>

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/twt/ITwtManager.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;
class Ieee80211Mib;

class INET_API Ieee80211TwtManager : public SimpleModule, public ITwtManager
{
  protected:
    bool enabled = false;
    int maxIndividualAgreementsPerPeer = 8;
    Ieee80211Mac *mac = nullptr;
    Ieee80211Mib *mib = nullptr;
    cMessage *servicePeriodTimer = nullptr;
    std::vector<TwtAgreement> agreements;
    std::vector<TwtBroadcastSchedule> broadcastSchedules;
    bool stationAwake = true;
    simtime_t lastRadioStateChange = SIMTIME_ZERO;
    simtime_t awakeTime = SIMTIME_ZERO;
    simtime_t sleepTime = SIMTIME_ZERO;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual void finish() override;
    virtual void rescheduleServicePeriodTimer();
    virtual void updateServicePeriodState();
    virtual bool isAgreementActiveNow(const TwtAgreement& agreement, simtime_t now) const;
    virtual simtime_t getNextEventTime(const TwtAgreement& agreement, simtime_t now) const;
    virtual TwtAgreement *findAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId);
    virtual TwtBroadcastSchedule *findBroadcastScheduleForUpdate(uint8_t broadcastId);
    virtual void expireBroadcastSchedules();

  public:
    virtual ~Ieee80211TwtManager();
    virtual bool isEnabled() const override { return enabled; }
    virtual void installAgreement(const TwtAgreement& agreement) override;
    virtual void removeAgreement(const MacAddress& peer, uint8_t flowId, bool broadcast, uint8_t broadcastId) override;
    virtual bool updateNextWakeTime(const MacAddress& peer, uint8_t flowId, simtime_t nextWakeTime) override;
    virtual bool isStationAwake() const override { return !enabled || stationAwake; }
    virtual bool isPeerEligible(const MacAddress& peer) const override;
    virtual void notifyPeerAwake(const MacAddress& peer) override;
    virtual void installBroadcastSchedule(const TwtBroadcastSchedule& schedule) override;
    virtual void removeBroadcastSchedule(uint8_t broadcastId) override;
    virtual bool findBroadcastSchedule(uint8_t broadcastId, TwtBroadcastSchedule& schedule) const override;
    virtual std::vector<TwtBroadcastSchedule> getBroadcastSchedules() const override;
    virtual bool addBroadcastMember(uint8_t broadcastId, const MacAddress& peer) override;
    virtual void removeBroadcastMember(uint8_t broadcastId, const MacAddress& peer) override;
};

} // namespace ieee80211
} // namespace inet

#endif
