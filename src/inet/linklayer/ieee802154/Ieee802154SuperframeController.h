// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154SUPERFRAMECONTROLLER_H
#define __INET_IEEE802154SUPERFRAMECONTROLLER_H

#include <map>

#include "inet/common/SimpleModule.h"
#include "inet/common/clock/ClockUserModuleMixin.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154SuperframeClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154SuperframeProvider.h"

namespace inet {

/** Owns superframe clock anchors and the beacon preparation event. */
class INET_API Ieee802154SuperframeController : public ClockUserModuleMixin<SimpleModule>, public IIeee802154SuperframeProvider, public ILifecycle
{
  private:
    ModuleRefByPar<IIeee802154SuperframeClient> client;
    std::map<int, Ieee802154SuperframeReceptionAnchor> receptionAnchors;
    ClockEvent *beaconPreparation = nullptr;
    cMessage *beaconDispatch = nullptr;
    ClockEvent *capEnd = nullptr;
    cMessage *cfpBoundaryEvent = nullptr;
    ClockEvent *syncWake = nullptr;
    ClockEvent *syncDeadline = nullptr;
    ClockEvent *accessSearchDeadline = nullptr;
    std::optional<Ieee802154AccessWindow> accessWindow;
    uint64_t accessEpoch = 0;
    clocktime_t beaconBoundary;
    bool started = true;

  protected:
    virtual int numInitStages() const override;
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;

  public:
    virtual ~Ieee802154SuperframeController();
    virtual clocktime_t getCurrentClockTime() const override { return getClockTime(); }
    virtual bool scheduleBeaconPreparation(clocktime_t boundary, simtime_t readinessLead) override;
    virtual void cancelBeaconPreparation() override;
    virtual bool scheduleCapEnd(clocktime_t boundary) override;
    virtual void cancelCapEnd() override;
    bool scheduleCfpBoundary(simtime_t when) override;
    void cancelCfpBoundary() override;
    virtual void scheduleSyncWake(clocktime_t when) override;
    virtual void scheduleSyncDeadline(clocktime_t when) override;
    virtual void cancelSyncEvents() override;
    virtual Ieee802154AccessWindow publishAccessWindow(const Ieee802154AccessWindow& window) override;
    virtual std::optional<Ieee802154AccessWindow> getAccessWindow() const override;
    virtual void clearAccessWindow() override;
    virtual void scheduleAccessSearchDeadline(clocktime_t when) override;
    virtual void cancelAccessSearchDeadline() override;
    virtual simtime_t getFutureSimulationTime(clocktime_t target) const override { return computeSimTimeFromClockTime(target); }
    virtual clocktime_t getFutureClockTime(simtime_t target) const override { return computeClockTimeFromSimTime(target); }
    simtime_t getForecastUncertainty(simtime_t elapsed, simtime_t maxPropagationDelay) const override;
    virtual void recordReceptionStart(const physicallayer::Ieee802154PhyReceiveStart& start,
            std::optional<Ieee802154AccessWindow> capWindow = {}, std::vector<Ieee802154CfpReceptionCandidate> cfpCandidates = {}) override;
    virtual std::optional<Ieee802154SuperframeReceptionAnchor> takeReceptionAnchor(int transmissionId) override;
    virtual void retireExpiredReceptionAnchors() override;
    virtual void clearReceptionAnchors() override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
};

} // namespace inet
#endif
