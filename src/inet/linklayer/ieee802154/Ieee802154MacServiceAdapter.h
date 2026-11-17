// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSERVICEADAPTER_H
#define __INET_IEEE802154MACSERVICEADAPTER_H

#include "inet/common/ModuleRefByPar.h"
#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceProvider.h"

namespace inet {

class Protocol;

class INET_API Ieee802154MacServiceAdapter : public SimpleModule, public IIeee802154MacServiceClient
{
  private:
    ModuleRefByPar<IIeee802154MacServiceProvider> service;
    const Protocol *upperProtocol = nullptr;
    Ieee802154MacServiceRequestId lastRequestId = 0;
    virtual void initialize() override;
    virtual void handleMessage(cMessage *message) override;
    void prepareIndication(Packet *packet);

  public:
    virtual void handleDataConfirm(Ieee802154MacServiceRequestId id, const Ieee802154MacDataConfirm& confirm) override;
    virtual void handleGetConfirm(Ieee802154MacServiceRequestId id, const Ieee802154MacGetConfirm& confirm) override;
    virtual void handleSetConfirm(Ieee802154MacServiceRequestId id, const Ieee802154MacSetConfirm& confirm) override;
    virtual void handleResetConfirm(Ieee802154MacServiceRequestId id, const Ieee802154MacResetConfirm& confirm) override;
    virtual void handleDataIndication(Packet *packet, const Ieee802154MacDataIndication& indication) override;
    virtual void handlePromiscuousIndication(Packet *packet, const Ieee802154MacPromiscuousIndication& indication) override;
    virtual void handleCommStatusIndication(Packet *packet, const Ieee802154MacCommStatusIndication& indication) override;
};

} // namespace inet
#endif
