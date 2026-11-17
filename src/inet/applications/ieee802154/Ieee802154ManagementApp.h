// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MANAGEMENTAPP_H
#define __INET_IEEE802154MANAGEMENTAPP_H

#include <map>
#include "inet/common/SimpleModule.h"
#include "inet/common/ModuleRefByPar.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceProvider.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154ManagementServiceProvider.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154ManagementServiceClient.h"

namespace inet {

/** Small higher-layer policy example; protocol timing and association state belong to the MAC. */
class INET_API Ieee802154ManagementApp : public SimpleModule,
        public IIeee802154MacServiceClient, public IIeee802154ManagementServiceClient
{
  private:
    ModuleRefByPar<IIeee802154MacServiceProvider> service;
    ModuleRefByPar<IIeee802154ManagementServiceProvider> management;
    cMessage *actionTimer = nullptr;
    uint64_t nextId = 0;
    bool coordinator = false;
    bool beaconOperation = false;
    bool awaitingAssociationBeacon = false;
    int completedCycles = 0, receivedMsdus = 0, confirmedMsdus = 0;
    uint16_t nextShortAddress = 0x40;
    std::map<Ieee802154Address,uint16_t> allocations;
    std::optional<Ieee802154PanDescriptor> selectedPan;
    enum class Action { START, RESET, POLL } action;
    uint64_t allocateId();
    void accepted(Ieee802154MacServiceAdmission admission) const;
    void succeeded(const Ieee802154MacServiceResult& result) const;
    void set(const char *name, const Ieee802154MacPibValue& value);
    void sendData(const Ieee802154Address& destination, uint16_t panId, bool indirect);
    void associateSelectedPan();
    physicallayer::Ieee802154PhyChannel channel() const;
    virtual void initialize() override;
    virtual void handleMessage(cMessage *message) override;
    virtual void finish() override;

  public:
    virtual ~Ieee802154ManagementApp();
    virtual void handleResetConfirm(uint64_t, const Ieee802154MacResetConfirm& confirm) override;
    void handleSyncConfirm(uint64_t, const Ieee802154MacSyncConfirm& confirm) override;
    void handleStartSuperframeConfirm(uint64_t, const Ieee802154MacStartSuperframeConfirm& confirm) override;
    virtual void handleStartConfirm(uint64_t, const Ieee802154MacStartConfirm& confirm) override;
    virtual void handleScanConfirm(uint64_t, const Ieee802154MacScanConfirm& confirm) override;
    virtual void handleAssociateIndication(const Ieee802154MacAssociateIndication& indication) override;
    virtual void handleAssociateResponseStatus(uint64_t, const Ieee802154MacAssociateResponseStatus& status) override;
    virtual void handleAssociateConfirm(uint64_t, const Ieee802154MacAssociateConfirm& confirm) override;
    virtual void handleDataConfirm(uint64_t, const Ieee802154MacDataConfirm& confirm) override;
    virtual void handleDataIndication(Packet *packet, const Ieee802154MacDataIndication& indication) override;
    virtual void handlePollConfirm(uint64_t, const Ieee802154MacPollConfirm& confirm) override;
    virtual void handleDisassociateConfirm(uint64_t, const Ieee802154MacDisassociateConfirm& confirm) override;
    virtual void handleDisassociateIndication(const Ieee802154MacDisassociateIndication& indication) override;
    virtual void handleSetConfirm(uint64_t, const Ieee802154MacSetConfirm& confirm) override;
    virtual void handleGetConfirm(uint64_t, const Ieee802154MacGetConfirm&) override;
    virtual void handlePurgeConfirm(uint64_t, const Ieee802154MacPurgeConfirm&) override;
    virtual void handleBeaconNotifyIndication(const Ieee802154PanDescriptor&) override;
    virtual void handleBeaconConfirm(uint64_t, const Ieee802154MacBeaconConfirm&) override;
    virtual void handleBeaconRequestIndication(const Ieee802154MacBeaconRequestIndication&) override;
    virtual void handleOrphanIndication(const Ieee802154MacOrphanIndication&) override;
    virtual void handleOrphanResponseStatus(uint64_t, const Ieee802154MacOrphanResponseStatus&) override;
    virtual void handleSyncLossIndication(const Ieee802154MacSyncLossIndication&) override;
    virtual void handleRxEnableConfirm(uint64_t, const Ieee802154MacRxEnableConfirm&) override;
    virtual void handleRxEnableIndication(const Ieee802154MacRxEnableIndication&) override;
    virtual void handlePromiscuousIndication(Packet *packet, const Ieee802154MacPromiscuousIndication&) override;
    virtual void handleCommStatusIndication(Packet *packet, const Ieee802154MacCommStatusIndication&) override;
};

} // namespace inet
#endif
