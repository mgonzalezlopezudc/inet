// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANNATIVEMAC_H
#define __INET_LOWPANNATIVEMAC_H
#include "inet/linklayer/base/MacProtocolBase.h"
#include "inet/linklayer/contract/IMacProtocol.h"
#include "inet/linklayer/lowpan/LowpanNativeLinkDomain.h"
#include "inet/linklayer/lowpan/contract/ILowpanMac.h"
#include "inet/queueing/contract/IActivePacketSink.h"
namespace inet { namespace lowpan {
class INET_API LowpanNativeMac : public MacProtocolBase, public IMacProtocol, public queueing::IActivePacketSink, public ILowpanMac
{
  protected:
    enum State { IDLE, BACKOFF, CCA, DATA_TURNAROUND, DATA_TRANSMIT, ACK_WAIT, ACK_TURNAROUND, ACK_TRANSMIT };
    LowpanNativeLinkDomain *domain = nullptr;
    ModuleRefByPar<physicallayer::IRadio> radio;
    State state = IDLE;
    cMessage *accessTimer = nullptr;
    cMessage *ackTimer = nullptr;
    Packet *pendingAck = nullptr;
    uint8_t nextSequence = 0;
    int backoffs = 0, retries = 0;
    int maxBackoffs = 4, maxRetries = 3, minExponent = 3, maxExponent = 5;
    simtime_t maxLifetime, nextAccessTime;
    physicallayer::IRadio::TransmissionState previousTransmission = physicallayer::IRadio::TRANSMISSION_STATE_UNDEFINED;
    bool ccaBusy = false;
    virtual void initialize(int stage) override;
    virtual void configureNetworkInterface() override;
    virtual void handleSelfMessage(cMessage *message) override;
    virtual void handleUpperPacket(Packet *packet) override;
    virtual void handleLowerPacket(Packet *packet) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signal, intval_t value, cObject *details) override;
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    void clearTransientState();
    void startNextFrame();
    void startBackoff();
    void finishData(bool successful, PacketDropReason reason = OTHER_PACKET_DROP);
    void transmissionFinished();
    bool isRequestValid(const Ieee802154AddressReq& request) const;
    bool isPreparedValid(const Packet *packet, bool encapsulated) const;
    bool canMeetDeadline() const;
    void appendFcs(Packet *packet) const;
    void dropEnvelope(bool expired);
    void dropReceived(Packet *packet, PacketDropReason reason);
  public:
    virtual ~LowpanNativeMac();
    const LowpanNativeLinkDomain *getLinkDomain() const { return domain; }
    virtual Ptr<const LowpanTransmissionReq> prepareTransmission(const Ieee802154AddressReq& request) const override;
    virtual queueing::IPassivePacketSource *getProvider(const cGate *gate) override;
    virtual void handleCanPullPacketChanged(const cGate *gate) override;
    virtual void handlePullPacketProcessed(Packet *packet, const cGate *gate, bool successful) override;
};
} }
#endif
