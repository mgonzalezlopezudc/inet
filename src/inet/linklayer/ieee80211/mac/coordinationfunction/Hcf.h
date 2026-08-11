//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_HCF_H
#define __INET_HCF_H

#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Hcca.h"
#include "inet/linklayer/ieee80211/mac/common/ModeSetListener.h"
#include "inet/linklayer/ieee80211/mac/contract/IAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IBlockAckAgreementHandlerCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/ICoordinationFunction.h"
#include "inet/linklayer/ieee80211/mac/contract/ICtsPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HtRateControl.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IProcedureCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckAgreementPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientBlockAckProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientQosAckPolicy.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecipientQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/contract/IRtsProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/AmpduTransmissionLedger.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtMfbTransmissionState.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtSoundingPendingState.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceRxTimeoutState.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HtSoundingRetryState.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/QosRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/originator/TxopProcedure.h"
#include "inet/linklayer/ieee80211/mac/protectionmechanism/SingleProtectionMechanism.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"
#include "inet/linklayer/ieee80211/mac/recipient/CtsProcedure.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;
class StationQueueBank;

/**
 * Implements IEEE 802.11 Hybrid Coordination Function.
 */
class INET_API Hcf : public ICoordinationFunction, public IFrameSequenceHandler::ICallback, public IChannelAccess::ICallback, public ITx::ICallback, public IProcedureCallback, public IBlockAckAgreementHandlerCallback, public ModeSetListener
{
  public:
    static simsignal_t edcaCollisionDetectedSignal;
    static simsignal_t blockAckAgreementAddedSignal;
    static simsignal_t blockAckAgreementDeletedSignal;
    static simsignal_t ampduCreatedSignal;
    static simsignal_t ampduNumMpdusSignal;

  private:
    void handleBlockAckInactivityTimeout();

  protected:
    enum class HtAmpduAckContext {
        ORDINARY,
        IMPLICIT_BLOCK_ACK,
    };

    Ieee80211Mac *mac = nullptr;
    IRateControl *dataAndMgmtRateControl = nullptr;
    IIeee80211HtRateControl *htRateControl = nullptr;

    HtSoundingPendingState pendingHtSounding;
    bool enableHtSounding = false;
    int htSoundingNsts = 2;
    Ieee80211HtFeedbackKind htSoundingFeedbackKind = Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING;
    simtime_t htSoundingRetryInterval = SIMTIME_ZERO;
    HtSoundingRetryState htSoundingRetryState;
    HtMfbTransmissionState htMfbTransmissionState;

    cMessage *startRxTimer = nullptr;
    cMessage *inactivityTimer = nullptr;
    FrameSequenceRxTimeoutState frameSequenceRxTimeoutState;

    // Transmission and Reception
    IRx *rx = nullptr;
    ITx *tx = nullptr;

    IQosRateSelection *rateSelection = nullptr;

    // Channel Access Methods
    Edca *edca = nullptr;
    Hcca *hcca = nullptr;

    // MAC Data Service
    IOriginatorMacDataService *originatorDataService = nullptr;
    IRecipientQosMacDataService *recipientDataService = nullptr;

    // MAC Procedures
    IRecipientAckProcedure *recipientAckProcedure = nullptr;
    IOriginatorQoSAckPolicy *originatorAckPolicy = nullptr;
    IRecipientQosAckPolicy *recipientAckPolicy = nullptr;
    IRtsProcedure *rtsProcedure = nullptr;
    IRtsPolicy *rtsPolicy = nullptr;
    ICtsProcedure *ctsProcedure = nullptr;
    ICtsPolicy *ctsPolicy = nullptr;
    IOriginatorBlockAckProcedure *originatorBlockAckProcedure = nullptr;
    IRecipientBlockAckProcedure *recipientBlockAckProcedure = nullptr;

    // Block Ack Agreement Handlers
    IOriginatorBlockAckAgreementHandler *originatorBlockAckAgreementHandler = nullptr;
    IOriginatorBlockAckAgreementPolicy *originatorBlockAckAgreementPolicy = nullptr;
    IRecipientBlockAckAgreementHandler *recipientBlockAckAgreementHandler = nullptr;
    IRecipientBlockAckAgreementPolicy *recipientBlockAckAgreementPolicy = nullptr;

    // Tx Opportunity
    TxopProcedure *hccaTxop = nullptr;

    // Queues
    InProgressFrames *hccaInProgressFrame = nullptr;

    // Frame sequence handler
    IFrameSequenceHandler *frameSequenceHandler = nullptr;

    // Protection mechanisms
    SingleProtectionMechanism *singleProtectionMechanism = nullptr;

    std::string lastSelectedModePacketName;
    std::string lastSelectedModeName;
    double lastSelectedModeNetBitrate = -1;
    double lastSelectedModeBandwidth = -1;
    int lastSelectedModeNumSpatialStreams = -1;
    AmpduTransmissionLedger ampduTransmissionLedger;
    uint64_t nextServiceDataUnitId = 1;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void forEachChild(cVisitor *v) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    void handleDeferredStartRxTimeout();
    void processResponseAndCancelStartRxTimerIfCompleted(Packet *packet, IReceiveStep *receiveStep);
    static std::vector<Packet *> recoverHtImplicitBlockAckTimeout(
            InProgressFrames *inProgressFrames, QosAckHandler *ackHandler,
            QosRecoveryProcedure *recoveryProcedure,
            IRateControl *rateControl,
            const std::set<std::pair<MacAddress,
                    std::pair<Tid, SequenceControlField>>>& failedFrameIds);
    static HtAmpduAckContext classifyHtAmpduAckContext(
            unsigned int numAggregateMembers,
            const std::vector<Ptr<const Ieee80211MacHeader>>& headers);
    static Packet *buildAmpduPacket(const std::vector<Packet *>& frames, FcsMode fcsMode);
    bool processTransmittedAmpdu(Packet *packet, Edcaf *edcaf, AccessCategory ac);
    TxopProcedure::InitialProtection selectInitialProtection(Packet *frame,
            const physicallayer::IIeee80211Mode *firstMode) const;

    virtual void startFrameSequence(AccessCategory ac);
    void resumeContention();
    void handleEdcafInternalCollision(Edcaf *edcaf);
    virtual void handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs);

    void sendUp(const std::vector<Packet *>& completeFrames);
    FrameSequenceContext *buildContext(AccessCategory ac);
    virtual std::vector<Packet *> getHtImplicitBlockAckFrames(Edcaf *edcaf) const;
    virtual int getMaxAmpduLengthExponent(const MacAddress& peer,
            int defaultExponent, physicallayer::Ieee80211PhyFamily phyFamily) const;
    virtual bool hasFrameToTransmit();
    virtual bool hasFrameToTransmit(AccessCategory ac);
    virtual bool isReceptionInProgress();
    virtual bool shouldRestartWideChannelAccess(Edcaf *edcaf);
    virtual bool isLegacyHtMultiTidBlockAckEnabled() const;
    virtual bool processHeaderlessNdpIndication(Packet *packet) { return false; }
    bool processHtHeaderlessNdpIndication(Packet *packet);
    bool processHtNdpAnnouncement(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header);
    bool mayStartHtSounding(const MacAddress& peer,
            const physicallayer::IIeee80211Mode *mode) const;
    void sendStandaloneHtMfb();
    void attachPendingHtMcsControl(Packet *packet,
            const physicallayer::IIeee80211Mode *mode);

    // Recipient
    virtual void recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);
    virtual void recipientProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);
    virtual void recipientProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header);
    virtual void recipientProcessTransmittedControlResponseFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header);

    // Originator
    virtual void originatorProcessTransmittedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& mgmtHeader, AccessCategory ac);
    virtual void originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac);
    virtual void originatorProcessTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, AccessCategory ac);
    virtual void originatorProcessReceivedManagementFrame(const Ptr<const Ieee80211MgmtHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac);
    virtual void originatorProcessReceivedControlFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, Packet *lastTransmittedPacket, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac);
    void processReceivedAck(Edcaf *edcaf,
            const Ptr<const Ieee80211AckFrame>& ackFrame,
            Packet *lastTransmittedPacket,
            const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader);
    void processReceivedBlockAck(Edcaf *edcaf,
            const Ptr<const Ieee80211BlockAck>& blockAck, AccessCategory ac);
    void processFailedBlockAckReq(Edcaf *edcaf,
            const Ptr<const Ieee80211BlockAckReq>& blockAckReq,
            bool requireValidSequenceNumber);
    virtual void originatorProcessReceivedDataFrame(const Ptr<const Ieee80211DataHeader>& header, const Ptr<const Ieee80211MacHeader>& lastTransmittedHeader, AccessCategory ac);
    virtual void originatorProcessBlockAckResult(
            const Ptr<const Ieee80211BlockAck>& blockAck,
            const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>& ackedFrames,
            AccessCategory ac) {}

    virtual void setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, const physicallayer::IIeee80211Mode *mode) const;
    virtual void recordSelectedMode(Packet *packet, const physicallayer::IIeee80211Mode *mode);
    virtual bool isSentByUs(const Ptr<const Ieee80211MacHeader>& header) const;
    virtual bool isForUs(const Ptr<const Ieee80211MacHeader>& header) const;

    static constexpr uint32_t MAX_FINITE_BUFFER_STATUS_BYTES =
            std::numeric_limits<uint32_t>::max() - 1;
    static void addBufferedTrafficServiceBytes(uint32_t& total, uint64_t amount);
    uint64_t allocateServiceDataUnitId();
    void tagMacSapServiceDataUnit(Packet *packet,
            const Ptr<const Ieee80211DataHeader>& header);
    uint32_t calculateBufferedTrafficServiceBytes(Edcaf *edcaf, const MacAddress& peer,
            int tid, const std::vector<Packet *>& additionalPackets) const;
    virtual uint32_t getBufferedTrafficServiceBytes(
            Edcaf *edcaf, const MacAddress& peer, int tid = -1) const;

    // Per-STA queue bank routing (HE OFDMA scheduling)
    virtual queueing::IPacketQueue *getPerStaQueue(const MacAddress& staAddr, AccessCategory ac);

  protected:
    // IFrameSequenceHandler::ICallback
    virtual void originatorProcessRtsProtectionFailed(Packet *packet) override;
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket) override;
    virtual void originatorProcessFailedFrame(Packet *packet) override;
    virtual void frameSequenceFinished() override;
    virtual void transmitFrame(Packet *packet, simtime_t ifs) override;
    virtual void scheduleStartRxTimer(simtime_t timeout) override;

    // IChannelAccess::ICallback
    virtual void channelGranted(IChannelAccess *channelAccess) override;

    // ITx::ICallback
    virtual void transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;

    // IProcedureCallback
    virtual void transmitControlResponseFrame(Packet *responsePacket, const Ptr<const Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<const Ieee80211MacHeader>& receivedHeader) override;
    virtual void processMgmtFrame(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader) override;

    // IProcedureCallback
    virtual void scheduleInactivityTimer(simtime_t timeout) override;

    std::string getFrameSequenceInfo() const;

  public:
    virtual ~Hcf();

    /**
     * Returns true when the local, non-negotiated HT implicit Block Ack
     * feature is enabled. Ieee80211Mac uses this at the deaggregation boundary
     * so HCF can inspect the complete A-MPDU.
     */
    virtual bool isHtImplicitBlockAckEnabled() const;

    IOriginatorMacDataService *getOriginatorMacDataService() const { return originatorDataService; }
    IOriginatorBlockAckAgreementHandler *getOriginatorBlockAckAgreementHandler() const { return originatorBlockAckAgreementHandler; }
    virtual void invalidatePeerDerivedState(const MacAddress& peer);
    virtual queueing::IPacketQueue *resolvePerStaQueue(const MacAddress& staAddr, AccessCategory ac) {
      return getPerStaQueue(staAddr, ac);
    }
    virtual void twtServicePeriodChanged();

    // ICoordinationFunction
    virtual void processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header) override;
    virtual void processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void corruptedFrameReceived() override;
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
