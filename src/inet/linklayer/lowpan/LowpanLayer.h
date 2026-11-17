// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANLAYER_H
#define __INET_LOWPANLAYER_H

#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/OperationalMixin.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanReassemblyTable.h"
#include "inet/linklayer/lowpan/LowpanDatagramTagAllocator.h"
#include "inet/linklayer/lowpan/contract/ILowpanLink.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/queueing/base/PacketProcessorBase.h"
#include "inet/queueing/contract/IActivePacketSource.h"

namespace inet { namespace lowpan {

// Per-hop IPv6 adaptation; reassembly always precedes IPv6 delivery.
class INET_API LowpanLayer : public OperationalMixin<queueing::PacketProcessorBase>, public queueing::IActivePacketSource
{
  protected:
    queueing::PassivePacketSinkRef consumer;
    ILowpanLink *link = nullptr;
    NetworkInterface *networkInterface = nullptr;
    std::unique_ptr<LowpanReassemblyTable> reassembly;
    LowpanDatagramTagAllocator tagAllocator;
    simtime_t reassemblyTimeout;
    bool useIphc = false;
    cMessage *expiryTimer = nullptr;
    static simsignal_t datagramAcceptedSignal;
    static simsignal_t datagramCompletedSignal;
    static simsignal_t fragmentSentSignal;
    static simsignal_t fragmentReceivedSignal;
    static simsignal_t reassemblyCompletedSignal;
    static simsignal_t reassemblyExpiredSignal;
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *message) override;
    virtual void handleMessageWhenDown(cMessage *message) override;
    virtual void handleStartOperation(LifecycleOperation *operation) override {}
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    virtual bool isInitializeStage(int stage) const override { return stage == INITSTAGE_LINK_LAYER; }
    virtual bool isModuleStartStage(int stage) const override { return stage == ModuleStartOperation::STAGE_LINK_LAYER; }
    virtual bool isModuleStopStage(int stage) const override { return stage == ModuleStopOperation::STAGE_LINK_LAYER; }
    void processUpperPacket(Packet *packet);
    void processLowerPacket(Packet *packet);
    void processFragment(Packet *packet, bool first);
    void deliverIpv6(Packet *packet);
    bool decompressHeader(Packet *packet, int originalSize);
    void scheduleExpiry();
    void clearTransientState();
    bool isValidIpv6Packet(const Packet *packet) const;
    void dropLowpanPacket(Packet *packet, LowpanDropReason reason);
    void removeRequestTags(Packet *packet) const;

  public:
    virtual ~LowpanLayer();
    virtual bool supportsPacketPushing(const cGate *gate) const override { return gate == this->gate("lowerLayerOut"); }
    virtual bool supportsPacketPulling(const cGate *gate) const override { return false; }
    virtual queueing::IPassivePacketSink *getConsumer(const cGate *gate) override { return consumer.get(); }
    virtual void handleCanPushPacketChanged(const cGate *gate) override {}
    virtual void handlePushPacketProcessed(Packet *packet, const cGate *gate, bool successful) override {}
};

} } // namespace inet::lowpan
#endif
