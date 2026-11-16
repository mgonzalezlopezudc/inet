// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154UNSLOTTEDACCESS_H
#define __INET_IEEE802154UNSLOTTEDACCESS_H
#include <optional>
#include "inet/common/ModuleRefByPar.h"
#include "inet/common/SimpleModule.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessProvider.h"

namespace inet {
class INET_API Ieee802154UnslottedAccess : public SimpleModule, public IIeee802154AccessProvider, public ILifecycle
{
  private:
    struct Attempt {
        uint64_t id;
        Ieee802154AccessRequest parameters;
        uint8_t nb = 0;
        uint8_t be;
        bool waitingForCca = false;
    };
    ModuleRefByPar<IIeee802154AccessClient> client;
    std::optional<Attempt> attempt;
    uint64_t lastId = 0;
    bool started = true;
    cMessage *backoffTimer = nullptr;
    simsignal_t slotsSignal, countSignal, exponentSignal;
    virtual int numInitStages() const override;
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    void backoff();
    void requestCca();
    void complete(const Ieee802154MacServiceResult& result);

  public:
    virtual ~Ieee802154UnslottedAccess();
    virtual Ieee802154MacServiceAdmission start(uint64_t id, const Ieee802154AccessRequest& request) override;
    virtual Ieee802154MacServiceAdmission updateWindow(uint64_t id, const Ieee802154AccessWindow& window) override;
    virtual void handleCcaResult(uint64_t id, bool idle) override;
    virtual void deferCca(uint64_t id, simtime_t notBefore) override;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id, Ieee802154MacServiceAbortReason reason) override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *callback) override;
};
}
#endif
