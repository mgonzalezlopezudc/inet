// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154SLOTTEDACCESS_H
#define __INET_IEEE802154SLOTTEDACCESS_H

#include <optional>
#include "inet/common/SimpleModule.h"
#include "inet/common/clock/ClockUserModuleMixin.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessProvider.h"

namespace inet {

/** Owns a single slotted CSMA-CA attempt across actual CAP windows. */
class INET_API Ieee802154SlottedAccess : public ClockUserModuleMixin<SimpleModule>, public IIeee802154AccessProvider, public ILifecycle
{
  private:
    struct Attempt {
        uint64_t id;
        Ieee802154AccessRequest parameters;
        Ieee802154AccessWindow window;
        uint8_t nb = 0;
        uint8_t be = 0;
        uint8_t cw = 2;
        int remaining = 0;
        bool needsDraw = true;
        bool waitingForCca = false;
        bool deferred = false;
    };
    ModuleRefByPar<IIeee802154AccessClient> client;
    std::optional<Attempt> attempt;
    uint64_t lastId = 0;
    bool started = true;
    ClockEvent *timer = nullptr;
    simsignal_t slotsSignal, countSignal, exponentSignal, contentionWindowSignal;

    virtual int numInitStages() const override;
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    clocktime_t boundaryAtOrAfter(clocktime_t earliest, simtime_t physicalEarliest) const;
    bool validWindow(const Ieee802154AccessWindow& window) const;
    bool canFit(clocktime_t firstCca) const;
    void advance();
    void requestCca();
    void complete(const Ieee802154MacServiceResult& result, std::optional<simtime_t> startAt = {});
    void cancelTimer();

  public:
    virtual ~Ieee802154SlottedAccess();
    virtual Ieee802154MacServiceAdmission start(uint64_t id, const Ieee802154AccessRequest& request) override;
    virtual Ieee802154MacServiceAdmission updateWindow(uint64_t id, const Ieee802154AccessWindow& window) override;
    virtual void handleCcaResult(uint64_t id, bool idle) override;
    virtual void deferCca(uint64_t id, simtime_t notBefore) override;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id, Ieee802154MacServiceAbortReason reason) override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *callback) override;
};

} // namespace inet
#endif
