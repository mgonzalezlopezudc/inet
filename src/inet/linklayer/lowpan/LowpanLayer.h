// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANLAYER_H
#define __INET_LOWPANLAYER_H

#include <bitset>
#include <map>
#include <set>

#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/OperationalMixin.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanContextTable.h"
#include "inet/linklayer/lowpan/LowpanReassemblyTable.h"
#include "inet/linklayer/lowpan/LowpanDatagramTagAllocator.h"
#include "inet/linklayer/lowpan/LowpanDispatchPrefix.h"
#include "inet/linklayer/lowpan/contract/ILowpanLink.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/queueing/base/PacketProcessorBase.h"
#include "inet/queueing/contract/IActivePacketSource.h"

namespace inet { namespace lowpan {
class LowpanSourceFragmentation;
class LowpanFragmentForwarding;

// IPv6 adaptation with optional mesh-under transit before endpoint reassembly.
class INET_API LowpanLayer : public OperationalMixin<queueing::PacketProcessorBase>, public queueing::IActivePacketSource
{
    friend class LowpanSourceFragmentation;
    friend class LowpanFragmentForwarding;
  protected:
    queueing::PassivePacketSinkRef consumer;
    ILowpanLink *link = nullptr;
    NetworkInterface *networkInterface = nullptr;
    std::unique_ptr<LowpanReassemblyTable> reassembly;
    LowpanDatagramTagAllocator tagAllocator;
    std::unique_ptr<LowpanSourceFragmentation> sourceFragments;
    std::unique_ptr<LowpanFragmentForwarding> fragmentForwarding;
    struct ForwardedTransmission { int ownerModuleId; int64_t token; };
    std::map<int64_t, ForwardedTransmission> forwardedTransmissions;
    LowpanContextTable compressionContexts;
    std::array<bool, 16> configuredContexts{};
    simtime_t reassemblyTimeout;
    bool useIphc = false;
    cMessage *expiryTimer = nullptr;
    bool meshUnder = false;
    uint8_t meshHops = 8;
    simtime_t meshResidence; // Effective per-hop duration after simulation-clock quantization.
    simtime_t broadcastRetention; // Effective first-arrival retention, shared by cache and source guard.
    int maxBroadcastDatagrams = 256;
    std::map<Ieee802154Address, Ieee802154Address> meshRoutes;
    struct BroadcastRecord {
        simtime_t expires;
        bool fragmented;
        uint16_t size;
        uint16_t tag;
        std::bitset<256> offsets;
    };
    std::map<std::pair<Ieee802154Address, uint8_t>, BroadcastRecord> broadcasts;
    std::array<simtime_t, 256> sequenceReservations{};
    uint8_t nextBroadcastSequence = 0;
    std::set<int64_t> pendingNativeTransmissions; // IDs only; bounded by the finite lower queue/current frame.
    static simsignal_t datagramAcceptedSignal;
    static simsignal_t datagramCompletedSignal;
    static simsignal_t fragmentSentSignal;
    static simsignal_t fragmentReceivedSignal;
    static simsignal_t reassemblyCompletedSignal;
    static simsignal_t reassemblyExpiredSignal;
    static simsignal_t reassemblyInvalidatedSignal;
    static simsignal_t meshForwardedSignal;
    static simsignal_t nativeFrameTransmissionSucceededSignal;
    static simsignal_t nativeFrameTransmissionFailedSignal;
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *message) override;
    virtual void handleMessageWhenDown(cMessage *message) override;
    virtual void handleStartOperation(LifecycleOperation *operation) override { expireBroadcasts(); }
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    virtual bool isInitializeStage(int stage) const override { return stage == INITSTAGE_LINK_LAYER; }
    virtual bool isModuleStartStage(int stage) const override { return stage == ModuleStartOperation::STAGE_LINK_LAYER; }
    virtual bool isModuleStopStage(int stage) const override { return stage == ModuleStopOperation::STAGE_LINK_LAYER; }
    void processUpperPacket(Packet *packet);
    void processLowerPacket(Packet *packet);
    void processInnerPacket(Packet *packet, const Ieee802154Address& source, const Ieee802154Address& destination,
            LowpanDispatchPrefix::State state = {0, false});
    void processMeshPacket(Packet *packet, LowpanDispatchPrefix::State state, const Ptr<const Chunk>& leadingPages);
    void processFragment(Packet *packet, bool first, const Ieee802154Address& source, const Ieee802154Address& destination,
            LowpanDispatchPrefix::State state);
    void deliverIpv6(Packet *packet);
    bool decompressHeader(Packet *packet, int originalSize, const Ieee802154Address& source, const Ieee802154Address& destination);
    void expireBroadcasts();
    bool acceptBroadcast(Packet *packet, const Ieee802154Address& originator, uint8_t sequence,
            LowpanDispatchPrefix::State state);
    void forwardMesh(Packet *packet, const Ptr<const Chunk>& header, const Ieee802154Address& destination,
            const Ptr<const Chunk>& leadingPages);
    simtime_t meshDeadline(simtime_t duration) const;
    simtime_t meshReservation(uint8_t hops, simtime_t retention) const;
    void scheduleExpiry();
    void expireReassembly();
    void clearTransientState();
    bool isNativeTransmissionOperational() const;
    bool canPushPreparedPacket(Packet *packet) const;
    PacketDropReason preparedRefusalReason() const;
    bool pushPreparedPacket(Packet *packet); // Consumes packet on both admission and refusal.
    std::optional<uint16_t> allocateDatagramTag(simtime_t now, simtime_t reuseAfter);
    bool pushForwardedPacket(Packet *packet, int ownerModuleId, int64_t token);
    void notifyForwardingUnavailable();
    bool isValidIpv6Packet(const Packet *packet) const;
    void dropLowpanPacket(Packet *packet, LowpanDropReason reason);
    void removeRequestTags(Packet *packet) const;
    void installContext(int id, const Ipv6Address& prefix, int prefixLength,
            bool allowCompression, simtime_t compressionUntil, simtime_t validUntil);

  public:
    LowpanLayer();
    virtual ~LowpanLayer();
    // Prefix replacement invalidates partial RX; refresh of the same prefix
    // changes future lookup eligibility without changing reconstructed bytes.
    void setCompressionContext(int id, const Ipv6Address& prefix, int prefixLength,
            bool allowCompression, simtime_t validUntil);
    bool isConfiguredContext(int id) const;
    const LowpanContextTable::Context *getCompressionContext(int id) const;
    void setLearnedContext(int id, const Ipv6Address& prefix, int prefixLength,
            bool allowCompression, simtime_t compressionUntil, simtime_t validUntil);
    void removeLearnedContext(int id);
    virtual bool supportsPacketPushing(const cGate *gate) const override { return gate == this->gate("lowerLayerOut"); }
    virtual bool supportsPacketPulling(const cGate *gate) const override { return false; }
    virtual queueing::IPassivePacketSink *getConsumer(const cGate *gate) override { return consumer.get(); }
    virtual void handleCanPushPacketChanged(const cGate *gate) override {}
    virtual void handlePushPacketProcessed(Packet *packet, const cGate *gate, bool successful) override;
};

} } // namespace inet::lowpan
#endif
