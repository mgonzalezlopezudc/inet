//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUTXOPFS_H
#define __INET_HEDLMUTXOPFS_H

#include <memory>
#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/contract/IAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IHeDlMuExchangeCallback.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuPlan.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Frame sequence for IEEE 802.11ax Downlink MU-OFDMA TXOP.
 *
 * The AP transmits the HE MU container frame, then either solicits HE TB
 * BlockAck responses with an MU-BAR Trigger or polls the selected STAs with
 * sequential BlockAckReq/BlockAck exchanges.
 */
class INET_API HeDlMuTxOpFs : public IFrameSequence
{
  public:
    class INET_API ITransactionObserver
    {
      public:
        virtual ~ITransactionObserver() {}
        virtual void beforePacketCommit(int packetIndex) = 0;
    };

    enum class AckMethod {
        MU_BAR_TRIGGER,
        EXPLICIT_SEQUENTIAL_BAR
    };

    struct ActiveAllocation {
        MacAddress staAddress;
        uint16_t associationId = 0;
        Tid tid = 0;
        int ruIndex;
        physicallayer::Ieee80211HeRu ru;
        int mcs = 0;
        int numberOfSpatialStreams = 1;
        int streamStartIndex = 0;
        int totalNsts = 1;
        bool muMimo = false;
        const physicallayer::IIeee80211Mode *barControlMode = nullptr;
        const physicallayer::IIeee80211Mode *blockAckResponseMode = nullptr;
        Packet *packet = nullptr;
        std::vector<Packet *> packets;
    };

  protected:
    int firstStep = -1;
    int step = -1;

    const HeDlMuPlan dlPlan;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    queueing::IPacketQueue *pendingQueue = nullptr;
    IAckHandler *ackHandler = nullptr;
    IFrameSequenceHandler::ICallback *callback = nullptr;
    IHeDlMuExchangeCallback *heDlMuCallback = nullptr;
    std::unique_ptr<IHeDlMuExchangeCallback> ownedCompatibilityCallback;
    ITransactionObserver *transactionObserver = nullptr;
    uint64_t transactionToken = 0;
    int maxAmpduMpduCount = 16;
    int maxHeMuPsduLength = 6500631;
    simtime_t maxHeMuPpduDuration = SimTime(5.484, SIMTIME_MS);
    AckMethod ackMethod = AckMethod::EXPLICIT_SEQUENTIAL_BAR;
    std::unique_ptr<IFrameSequence> sequence;
    uint32_t ackTriggerId = 0;
    const physicallayer::IIeee80211Mode *muBarControlMode = nullptr;

    std::vector<ActiveAllocation> activeAllocations;

    // Assembled container packet (owned until handed to TransmitStep).
    Packet *containerPacket = nullptr;

  protected:
    /** Build the MU container Packet from the scheduler allocation and pending queue. */
    Packet *buildMuContainerPacket(FrameSequenceContext *context);
    virtual MacAddress getTransmitterAddress() const;
    friend class HeDlMuPerStaBlockAckFs;
    friend class HeDlMuBarBlockAckFs;

  public:
    static void validateConfiguration(int maxAmpduMpduCount,
            int maxHeMuPsduLength, simtime_t maxHeMuPpduDuration);
    HeDlMuTxOpFs(const HeDlMuPlan& dlPlan,
                 physicallayer::Ieee80211ModeSet *modeSet,
                 queueing::IPacketQueue *pendingQueue,
                 IAckHandler *ackHandler,
                 IFrameSequenceHandler::ICallback *callback,
                 IHeDlMuExchangeCallback *heDlMuCallback,
                 uint64_t transactionToken,
                 int maxAmpduMpduCount = 16,
                 int maxHeMuPsduLength = 6500631,
                 simtime_t maxHeMuPpduDuration = SimTime(5.484, SIMTIME_MS),
                 AckMethod ackMethod = AckMethod::EXPLICIT_SEQUENTIAL_BAR);
    HeDlMuTxOpFs(const HeDlMuPlan& dlPlan,
                 physicallayer::Ieee80211ModeSet *modeSet,
                 queueing::IPacketQueue *pendingQueue,
                 IAckHandler *ackHandler,
                 IFrameSequenceHandler::ICallback *callback,
                 IFrameSequenceHandler::ICallback *legacyCallback,
                 IQosRateSelection *legacyRateSelection,
                 int maxAmpduMpduCount = 16,
                 int maxHeMuPsduLength = 6500631,
                 simtime_t maxHeMuPpduDuration = SimTime(5.484, SIMTIME_MS),
                 AckMethod ackMethod = AckMethod::EXPLICIT_SEQUENTIAL_BAR);
    virtual ~HeDlMuTxOpFs();

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;

    virtual bool isContainerPacket(Packet *packet) const { return packet == containerPacket; }
    virtual const std::vector<ActiveAllocation>& getActiveAllocations() const { return activeAllocations; }
    const HeDlMuPlan& getPlan() const { return dlPlan; }
    uint32_t getAckTriggerId() const { return ackTriggerId; }
    void setTransactionObserver(ITransactionObserver *observer) { transactionObserver = observer; }

    virtual std::string getHistory() const override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEDLMUTXOPFS_H
