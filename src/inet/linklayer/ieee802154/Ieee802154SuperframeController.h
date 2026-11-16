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

/** Owns superframe clock anchors; no periodic transmission is enabled by this component. */
class INET_API Ieee802154SuperframeController : public ClockUserModuleMixin<SimpleModule>, public IIeee802154SuperframeProvider, public ILifecycle
{
  private:
    ModuleRefByPar<IIeee802154SuperframeClient> client;
    std::map<int, Ieee802154SuperframeReceptionAnchor> receptionAnchors;
    bool started = true;

  protected:
    virtual int numInitStages() const override;
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;

  public:
    virtual void recordReceptionStart(const physicallayer::Ieee802154PhyReceiveStart& start) override;
    virtual std::optional<Ieee802154SuperframeReceptionAnchor> takeReceptionAnchor(int transmissionId) override;
    virtual void retireExpiredReceptionAnchors() override;
    virtual void clearReceptionAnchors() override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
};

} // namespace inet
#endif
