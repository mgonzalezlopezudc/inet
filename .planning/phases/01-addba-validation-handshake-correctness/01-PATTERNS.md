# Phase 01: ADDBA Validation & Handshake Correctness - Pattern Map

**Mapped:** 2026-06-16
**Files analyzed:** 5
**Analogs found:** 5 / 5

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc` | controller | event-driven | `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc` | exact |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h` | controller interface | event-driven | `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h` | exact |
| `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` | controller | event-driven, queue mutation | `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` | exact |
| `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h` | controller interface | event-driven | `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h` | exact |
| `tests/unit/Ieee80211HeMuAddbaValidation_1.test` | test | request-response simulation harness | `tests/unit/Ieee80211HeMuSeqAck_1.test` | role-match |

## Pattern Assignments

### `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc` (controller, event-driven)

**Analog:** `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`

**Imports pattern** (lines 7-20):
```cpp
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
```

**Module and initialization pattern** (lines 25-37):
```cpp
namespace inet {
namespace ieee80211 {

Define_Module(HeHcf);

void HeHcf::initialize(int stage)
{
    Hcf::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(getSubmodule("dlScheduler"));
        delete frameSequenceHandler;
        frameSequenceHandler = new HeFrameSequenceHandler();
    }
}
```

**Head-of-line candidate scan and active BA check** (lines 40-75):
```cpp
std::vector<MacAddress> HeHcf::collectCandidateStations(queueing::IPacketQueue *queue) const
{
    std::vector<MacAddress> candidates;
    std::vector<MacAddress> seenDestinations;
    int n = queue->getNumPackets();
    for (int i = 0; i < n; ++i) {
        Packet *pkt = queue->getPacket(i);
        const auto& header = pkt->peekAtFront<Ieee80211MacHeader>();
        MacAddress dest = header->getReceiverAddress();
        if (dest.isMulticast() || dest.isBroadcast())
            continue;
        bool seen = false;
        for (const auto& c : seenDestinations) {
            if (c == dest) { seen = true; break; }
        }
        if (seen)
            continue;
        seenDestinations.push_back(dest);

        bool isAddbaHandshakePerformed = false;
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            if (dataHeader->getType() == ST_DATA_WITH_QOS) {
                auto baHandler = getOriginatorBlockAckAgreementHandler();
                if (baHandler != nullptr) {
                    auto agreement = baHandler->getAgreement(dest, dataHeader->getTid());
                    if (agreement != nullptr && agreement->getIsAddbaResponseReceived()) {
                        isAddbaHandshakePerformed = true;
                    }
                }
            }
        }
        if (isAddbaHandshakePerformed) {
            candidates.push_back(dest);
        }
    }
    return candidates;
}
```

Planner note: keep the first-seen destination scan, but enforce D-06 strictly. The first packet for a destination decides candidate eligibility; later packets for the same destination must not bypass the head packet's missing agreement.

**MU/SU fallback pattern** (lines 78-111):
```cpp
void HeHcf::startFrameSequence(AccessCategory ac)
{
    // Check whether HE mode and multi-user conditions are met.
    bool isHeMode = (modeSet != nullptr && strcmp(modeSet->getName(), "ax") == 0);
    if (isHeMode) {
        auto edcaf = edca->getEdcaf(ac);
        auto pendingQueue = edcaf->getPendingQueue();
        auto inProgress = edcaf->getInProgressFrames();
        if (inProgress->getLength() > 0) {
            EV_INFO << "HeHcf: Pushing " << inProgress->getLength()
                    << " abandoned in-progress frames back to pendingQueue before starting MU sequence." << endl;
            std::vector<Packet *> framesToRequeue;
            for (int i = 0; i < inProgress->getLength(); ++i) {
                framesToRequeue.push_back(inProgress->getFrames(i));
            }
            for (auto frame : framesToRequeue) {
                inProgress->removeInProgressFrame(frame);
                pendingQueue->pushPacket(frame, nullptr);
            }
        }
        auto candidates = collectCandidateStations(pendingQueue);
        if (candidates.size() >= 2) {
            EV_INFO << "HeHcf: MU-OFDMA opportunity detected for " << candidates.size()
                    << " STAs — starting HeDlMuTxOpFs." << endl;
            frameSequenceHandler->startFrameSequence(
                    new HeDlMuTxOpFs(dlScheduler, candidates, modeSet,
                                     pendingQueue, edcaf->getAckHandler(), this),
                    buildContext(ac), this);
            emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
            return;
        }
    }
    // Fallback: standard single-user frame sequence.
    Hcf::startFrameSequence(ac);
}
```

Planner note: preserve fallback through `Hcf::startFrameSequence(ac)`. Do not enqueue ADDBA directly from `HeHcf`; the SU path invokes the existing originator BA lifecycle.

**MU failure requeue pattern** (lines 182-194):
```cpp
else {
    EV_INFO << "Retrying frame in MU-OFDMA: " << failedPacket->getName() << ", re-queuing.\n";
    auto h = failedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
    h->setRetry(true);
    failedPacket->insertAtFront(h);

    // Remove from inProgressFrames
    edcaf->getInProgressFrames()->removeInProgressFrame(failedPacket);

    // Re-enqueue into pendingQueue
    auto pendingQueue = edcaf->getPendingQueue();
    pendingQueue->pushPacket(failedPacket, nullptr);
}
```

### `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h` (controller interface, event-driven)

**Analog:** `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h`

**Header guard and include pattern** (lines 7-14):
```cpp
#ifndef __INET_HEHCF_H
#define __INET_HEHCF_H

#include <vector>

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/queueing/contract/IPacketQueue.h"
```

**Protected helper declaration pattern** (lines 34-52):
```cpp
class INET_API HeHcf : public Hcf
{
  protected:
    IIeee80211HeDlScheduler *dlScheduler = nullptr;

  protected:
    virtual void initialize(int stage) override;

    /**
     * Scans the pending queue front-to-back and returns up to maxMuStations
     * unique destination MAC addresses, in first-seen order.
     */
    virtual std::vector<MacAddress> collectCandidateStations(queueing::IPacketQueue *queue) const;

    /**
     * Override: selects HeDlMuTxOpFs when ≥2 unique destination STAs are
     * queued and HE mode is active; otherwise delegates to Hcf::startFrameSequence().
     */
    virtual void startFrameSequence(AccessCategory ac) override;
```

Planner note: if a shared active-BA helper is added to `HeHcf`, keep it protected/private and const-friendly. If it must also be used by `HeDlMuTxOpFs`, prefer a small file-local helper duplicated in each `.cc` or a narrow utility only if the planner wants one semantic source.

### `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` (controller, event-driven queue mutation)

**Analog:** `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`

**Imports pattern** (lines 7-20):
```cpp
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"
```

**Constructor state pattern** (lines 27-40):
```cpp
HeDlMuTxOpFs::HeDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                             const std::vector<MacAddress>& candidates,
                             Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback)
    : dlScheduler(dlScheduler),
      candidates(candidates),
      modeSet(modeSet),
      pendingQueue(pendingQueue),
      ackHandler(ackHandler),
      callback(callback)
{
}
```

**Scheduler and context access pattern** (lines 64-85):
```cpp
// Obtain per-STA RU assignments from the scheduler.
auto allocations = dlScheduler->schedule(candidates, channelCenterFrequency, channelBandwidth);
if (allocations.empty())
    throw cRuntimeError("HeDlMuTxOpFs: scheduler returned empty RU allocation");

// Assemble the container packet and populate Ieee80211HeMuTag.
auto container = new Packet("HE-MU-PPDU");

// Standard QoS data header — broadcast receiver signals HE MU frame.
auto containerHdr = makeShared<Ieee80211DataHeader>();
containerHdr->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
containerHdr->setType(ST_DATA_WITH_QOS);
containerHdr->setChunkLength(b(288)); // minimal 802.11 QoS data header size

auto muTag = container->addTag<Ieee80211HeMuTag>();

// 1. Calculate the total sequential ACK sequence duration
simtime_t totalDuration = simtime_t::ZERO;
auto hcf = dynamic_cast<Hcf *>(callback);
auto originatorBAHandler = hcf ? hcf->getOriginatorBlockAckAgreementHandler() : nullptr;
auto hcfModule = check_and_cast<cModule *>(callback);
auto rateSelection = check_and_cast<IQosRateSelection *>(hcfModule->getSubmodule("rateSelection"));
```

**Existing partial active-BA predicate in duration pass** (lines 109-123):
```cpp
bool hasBlockAckAgreement = false;
auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
    // Set the frame mode on the sub-packet first so response mode calculations don't fail due to missing mode
    auto staMode = rateSelection->computeMode(staPacket, dataOrMgmtHdr, nullptr);
    RateSelection::setFrameMode(staPacket, dataOrMgmtHdr, staMode);

    if (auto dataHdr = dynamicPtrCast<const Ieee80211DataHeader>(dataOrMgmtHdr)) {
        if (dataHdr->getType() == ST_DATA_WITH_QOS && originatorBAHandler != nullptr) {
            auto agreement = originatorBAHandler->getAgreement(alloc.staAddress, dataHdr->getTid());
            if (agreement != nullptr && agreement->getIsAddbaResponseReceived()) {
                hasBlockAckAgreement = true;
            }
        }
    }
}
```

Planner note: make this predicate strict. Phase 1 should not use the current non-BA normal ACK fallback branch for MU admission.

**Queue removal and tag allocation pattern that needs final guard** (lines 157-203):
```cpp
// 2. Build the final MU container packet and assign duration/sequence numbers to sub-packets
for (const auto& alloc : allocations) {
    // Find the first queued packet destined for this STA.
    Packet *staPacket = nullptr;
    int n = pendingQueue->getNumPackets();
    for (int i = 0; i < n; ++i) {
        Packet *pkt = pendingQueue->getPacket(i);
        const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
        if (hdr->getReceiverAddress() == alloc.staAddress) {
            staPacket = pkt;
            break;
        }
    }
    if (staPacket == nullptr) {
        EV_WARN << "HeDlMuTxOpFs: no queued packet for STA " << alloc.staAddress
                << ", skipping RU " << alloc.ru.index << endl;
        continue;
    }

    // Remove from pending queue, assign sequence number, set duration, and notify the ack handler.
    pendingQueue->removePacket(staPacket);
    auto macHdr = staPacket->peekAtFront<Ieee80211MacHeader>();
    if (auto dataOrMgmtHdr = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macHdr)) {
        auto dataOrMgmtHdrWritable = staPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
        auto heHcf = dynamic_cast<HeHcf *>(callback);
        if (heHcf != nullptr && !dataOrMgmtHdrWritable->getRetry()) {
            auto originatorQosDataService = check_and_cast<OriginatorQosMacDataService *>(heHcf->getOriginatorMacDataService());
            originatorQosDataService->assignSequenceNumber(dataOrMgmtHdrWritable);
        }
        // Set the duration field to totalDuration
        dataOrMgmtHdrWritable->setDurationField(totalDuration);
        staPacket->insertAtFront(dataOrMgmtHdrWritable);

        ackHandler->frameGotInProgress(dataOrMgmtHdrWritable);
    }

    // Store a duplicate in the tag (tag owns the copy).
    Packet *dupPkt = staPacket->dup();
    muTag->addAllocation(alloc.ru.index, dupPkt);
    ActiveAllocation activeAlloc;
    activeAlloc.staAddress = alloc.staAddress;
    activeAlloc.ruIndex = alloc.ru.index;
    activeAllocations.push_back(activeAlloc);

    // Add to inProgressFrames!
    context->getInProgressFrames()->addInProgressFrame(staPacket);
}
```

Planner note: insert the final BA/TID validation after `staPacket` is found and before `pendingQueue->removePacket(staPacket)`. If invalid, `EV_WARN` and `continue`; leaving the packet in the pending queue is required.

**Error handling pattern** (lines 205-209):
```cpp
if (muTag->getAllocations().empty())
    throw cRuntimeError("HeDlMuTxOpFs: no packets assembled for MU-OFDMA transmission");

EV_INFO << "HeDlMuTxOpFs: assembled HE MU PPDU with "
        << muTag->getAllocations().size() << " RU allocations. Total sequential duration = " << totalDuration << endl;
```

Planner note: if strict filtering leaves fewer than two active allocations, prefer aborting MU setup before transmission and returning to SU fallback. If the current frame sequence cannot call fallback directly, throw a clear runtime error only as a last resort; better plan around `HeHcf` prefilter plus final guard.

### `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h` (controller interface, event-driven)

**Analog:** `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h`

**Header guard and includes pattern** (lines 7-19):
```cpp
#ifndef __INET_HEDLMUTXOPFS_H
#define __INET_HEDLMUTXOPFS_H

#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/contract/IAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/queueing/contract/IPacketQueue.h"
```

**State and protected helper pattern** (lines 33-58):
```cpp
class INET_API HeDlMuTxOpFs : public IFrameSequence
{
  protected:
    struct ActiveAllocation {
        MacAddress staAddress;
        int ruIndex;
    };

    int firstStep = -1;
    int step = -1;

    IIeee80211HeDlScheduler *dlScheduler = nullptr;
    std::vector<MacAddress> candidates;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    queueing::IPacketQueue *pendingQueue = nullptr;
    IAckHandler *ackHandler = nullptr;
    IFrameSequenceHandler::ICallback *callback = nullptr;

    std::vector<ActiveAllocation> activeAllocations;

    // Assembled container packet (owned until handed to TransmitStep).
    Packet *containerPacket = nullptr;

  protected:
    /** Build the MU container Packet from the scheduler allocation and pending queue. */
    Packet *buildMuContainerPacket(FrameSequenceContext *context);
```

Planner note: if `buildMuContainerPacket()` needs to signal "not enough valid allocations" without throwing, update the interface carefully and keep ownership clear for `containerPacket`.

### `tests/unit/Ieee80211HeMuAddbaValidation_1.test` (test, request-response simulation harness)

**Analog:** `tests/unit/Ieee80211HeMuSeqAck_1.test`

**`.test` metadata and includes pattern** (lines 1-14):
```cpp
%description:
Test 802.11ax DL OFDMA sequential Block Ack steps, timeouts, and individual retry logic in HeDlMuTxOpFs.

%includes:
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/contract/IQosRateSelection.h"
#include "inet/linklayer/ieee80211/mac/contract/IAckHandler.h"
#include "inet/linklayer/ieee80211/mac/queue/InProgressFrames.h"
#include "inet/queueing/contract/IPacketQueue.h"
```

**Embedded NED and ini pattern** (lines 15-42):
```cpp
%file: TestNetwork.ned
import inet.queueing.queue.PacketQueue;

simple MockRateSelection
{
    @class(MockRateSelection);
}

module MockCallback
{
    parameters:
        @class(MockCallback);
    submodules:
        rateSelection: MockRateSelection;
}

network TestNetwork
{
    submodules:
        test: Test;
        mockHcf: MockCallback;
        pendingQueue: PacketQueue;
}

%inifile: omnetpp.ini
[General]
network = TestNetwork
```

**Mock object pattern** (lines 47-90):
```cpp
class MockRateSelection : public cSimpleModule, public IQosRateSelection
{
  public:
    virtual const physicallayer::IIeee80211Mode *computeResponseCtsFrameMode(Packet *packet, const Ptr<const Ieee80211RtsFrame>& rtsFrame) override { return nullptr; }
    virtual const physicallayer::IIeee80211Mode *computeResponseAckFrameMode(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader) override {
        auto ms = const_cast<physicallayer::Ieee80211ModeSet *>(physicallayer::Ieee80211ModeSet::getModeSet("ax"));
        return ms->getMode(0);
    }
    virtual const physicallayer::IIeee80211Mode *computeResponseBlockAckFrameMode(Packet *packet, const Ptr<const Ieee80211BlockAckReq>& blockAckReq) override {
        auto ms = const_cast<physicallayer::Ieee80211ModeSet *>(physicallayer::Ieee80211ModeSet::getModeSet("ax"));
        return ms->getMode(0);
    }
    virtual const physicallayer::IIeee80211Mode *computeMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, TxopProcedure *txopProcedure) override {
        auto ms = const_cast<physicallayer::Ieee80211ModeSet *>(physicallayer::Ieee80211ModeSet::getModeSet("ax"));
        return ms->getMode(0);
    }
};

Define_Module(MockRateSelection);

class MockAckHandler : public IAckHandler
{
  public:
    virtual bool isEligibleToTransmit(const Ptr<const Ieee80211DataOrMgmtHeader>& header) override { return true; }
    virtual bool isOutstandingFrame(const Ptr<const Ieee80211DataOrMgmtHeader>& header) override { return false; }
    virtual void frameGotInProgress(const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader) override {}
};

class MockScheduler : public IIeee80211HeDlScheduler
{
  public:
    virtual std::vector<RuAllocation> schedule(const std::vector<MacAddress>& candidateStations, Hz centerFrequency, Hz bandwidth) override {
```

**Packet setup and sequence exercise pattern** (lines 127-168):
```cpp
%activity:
auto mockHcf = check_and_cast<MockCallback *>(getParentModule()->getSubmodule("mockHcf"));
auto pendingQueue = check_and_cast<queueing::IPacketQueue *>(getParentModule()->getSubmodule("pendingQueue"));

// Create two packets for candidate STAs
MacAddress sta1("00:11:22:33:44:55");
MacAddress sta2("66:77:88:99:AA:BB");

Packet *p1 = new Packet("data-sta1");
auto hdr1 = makeShared<Ieee80211DataHeader>();
hdr1->setReceiverAddress(sta1);
hdr1->setType(ST_DATA);
hdr1->setAckPolicy(BLOCK_ACK);
hdr1->setSequenceNumber(SequenceNumberCyclic(0));
p1->insertAtFront(hdr1);
pendingQueue->enqueuePacket(p1);

Packet *p2 = new Packet("data-sta2");
auto hdr2 = makeShared<Ieee80211DataHeader>();
hdr2->setReceiverAddress(sta2);
hdr2->setType(ST_DATA);
hdr2->setAckPolicy(BLOCK_ACK);
hdr2->setSequenceNumber(SequenceNumberCyclic(1));
p2->insertAtFront(hdr2);
pendingQueue->enqueuePacket(p2);

auto modeSet = physicallayer::Ieee80211ModeSet::getModeSet("ax");
auto scheduler = new MockScheduler();
auto ackHandler = new MockAckHandler();

std::vector<MacAddress> candidates = {sta1, sta2};
HeDlMuTxOpFs *fs = new HeDlMuTxOpFs(scheduler, candidates, const_cast<physicallayer::Ieee80211ModeSet*>(modeSet), pendingQueue, ackHandler, mockHcf);

MockInProgressFrames *inProgress = new MockInProgressFrames();
FrameSequenceContext *context = new FrameSequenceContext(MacAddress("AA:BB:CC:DD:EE:FF"), const_cast<physicallayer::Ieee80211ModeSet*>(modeSet), inProgress, nullptr, nullptr, nullptr, nullptr);

fs->startSequence(context, 0);

// --- Step 0: Transmit ---
IFrameSequenceStep *step0 = fs->prepareStep(context);
ASSERT(step0->getType() == IFrameSequenceStep::Type::TRANSMIT);
EV << "Step 0 prepared: Transmit container frame.\n";
```

Planner note: the existing test uses `ST_DATA`; Phase 1 validation tests must use `ST_DATA_WITH_QOS` and set `setTid(...)`, because both candidate filtering and final admission are TID-sensitive.

**Output assertion pattern** (lines 221-230):
```cpp
%contains: stdout
Step 0 prepared: Transmit container frame.
Step 1 prepared: Expect Block Ack with timeout.
Callback: ProcessReceivedFrame BasicBlockAck
Step 2 prepared: Transmit BAR frame.
Step 3 prepared: Expect Block Ack with timeout.
HeDlMuTxOpFs: sequential BlockAck timeout for STA 66-77-88-99-AA-BB, triggering failure recovery.
Callback: ProcessFailedFrame data-sta2
Sequence completed successfully.
Failed calls count: 1
```

## Shared Patterns

### Active Originator Block Ack Agreement

**Source:** `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc` lines 59-68; `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` lines 116-121
**Apply to:** `HeHcf::collectCandidateStations()`, `HeDlMuTxOpFs::buildMuContainerPacket()`

```cpp
auto agreement = baHandler->getAgreement(dest, dataHeader->getTid());
if (agreement != nullptr && agreement->getIsAddbaResponseReceived()) {
    isAddbaHandshakePerformed = true;
}
```

```cpp
auto agreement = originatorBAHandler->getAgreement(alloc.staAddress, dataHdr->getTid());
if (agreement != nullptr && agreement->getIsAddbaResponseReceived()) {
    hasBlockAckAgreement = true;
}
```

### Agreement Lifecycle and ADDBA Background Initiation

**Source:** `src/inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.cc` lines 124-132, 135-152, 164-172
**Apply to:** SU fallback behavior; do not duplicate in HE coordination

```cpp
void OriginatorBlockAckAgreementHandler::processTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IProcedureCallback *callback)
{
    auto agreement = getAgreement(dataHeader->getReceiverAddress(), dataHeader->getTid());
    if (blockAckAgreementPolicy->isAddbaReqNeeded(packet, dataHeader) && agreement == nullptr) {
        auto addbaReq = buildAddbaRequest(dataHeader->getReceiverAddress(), dataHeader->getTid(), dataHeader->getSequenceNumber() + 1, blockAckAgreementPolicy);
        createAgreement(addbaReq);
        auto addbaPacket = new Packet("AddbaReq", addbaReq);
        callback->processMgmtFrame(addbaPacket, addbaReq);
    }
}
```

```cpp
void OriginatorBlockAckAgreementHandler::updateAgreement(OriginatorBlockAckAgreement *agreement, const Ptr<const Ieee80211AddbaResponse>& addbaResp)
{
    agreement->setIsAddbaResponseReceived(true);
    agreement->setBufferSize(addbaResp->getBufferSize());
    agreement->setBlockAckTimeoutValue(addbaResp->getBlockAckTimeoutValue());
    agreement->calculateExpirationTime();
}
```

```cpp
void OriginatorBlockAckAgreementHandler::processReceivedDelba(const Ptr<const Ieee80211Delba>& delba, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
{
    if (blockAckAgreementPolicy->isDelbaAccepted(delba))
        terminateAgreement(delba->getTransmitterAddress(), delba->getTid());
}
```

### Management Frame AC_VO Classification

**Source:** `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc` lines 117-145 and 855-860
**Apply to:** ADDBA request fallback path

```cpp
void Hcf::processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    Enter_Method("processUpperFrame(%s)", packet->getName());
    take(packet);
    EV_INFO << "Processing upper frame: " << packet->getName() << endl;
    AccessCategory ac = AccessCategory(-1);
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(header)) // TODO + non-QoS frames
        ac = AccessCategory::AC_VO;
    else if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header))
        ac = edca->classifyFrame(dataHeader);
    else
        throw cRuntimeError("Unknown message type");
    EV_INFO << "The upper frame has been classified as a " << printAccessCategory(ac) << " frame." << endl;
    auto pendingQueue = edca->getEdcaf(ac)->getPendingQueue();
    pendingQueue->enqueuePacket(packet);
    if (!pendingQueue->isEmpty()) {
        auto edcaf = edca->getChannelOwner();
        if (edcaf == nullptr || edcaf->getAccessCategory() != ac) {
            EV_DETAIL << "Requesting channel for access category " << printAccessCategory(ac) << endl;
            edca->requestChannelAccess(ac, this);
        }
    }
}
```

```cpp
void Hcf::processMgmtFrame(Packet *mgmtPacket, const Ptr<const Ieee80211MgmtHeader>& mgmtHeader)
{
    Enter_Method("processMgmtFrame");
    mgmtPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    processUpperFrame(mgmtPacket, mgmtHeader);
}
```

### SU Fallback Transmission and ADDBA Trigger

**Source:** `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc` lines 231-235 and 557-563
**Apply to:** Missing-agreement fallback from HE scheduling

```cpp
void Hcf::startFrameSequence(AccessCategory ac)
{
    frameSequenceHandler->startFrameSequence(new HcfFs(), buildContext(ac), this);
    emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
}
```

```cpp
void Hcf::originatorProcessTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    edcaf->getAckHandler()->processTransmittedDataOrMgmtFrame(dataHeader);
    if (originatorBlockAckAgreementHandler)
        originatorBlockAckAgreementHandler->processTransmittedDataFrame(packet, dataHeader, originatorBlockAckAgreementPolicy, this);
```

### QoS/TID Extraction

**Source:** `src/inet/linklayer/ieee80211/mac/Ieee80211Mac.cc` lines 258-263 and 297-301; `src/inet/linklayer/ieee80211/mac/originator/OriginatorQosAckPolicy.cc` lines 97-113
**Apply to:** TID-sensitive BA validation and tests

```cpp
if (auto userPriorityReq = packet->findTag<UserPriorityReq>()) {
    // make it a QoS frame, and set TID
    header->setType(ST_DATA_WITH_QOS);
    header->addChunkLength(QOSCONTROL_PART_LENGTH);
    header->setTid(userPriorityReq->getUserPriority());
}
```

```cpp
if (header->getType() == ST_DATA_WITH_QOS) {
    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
    int tid = dataHeader->getTid();
    if (tid < 8)
        packet->addTagIfAbsent<UserPriorityInd>()->setUserPriority(tid);
}
```

```cpp
if (agreement == nullptr)
    return AckPolicy::NORMAL_ACK;
if (agreement->getIsAddbaResponseReceived() && isBlockAckPolicyEligibleFrame(packet, header)) {
    if (checkAgreementPolicy(header, agreement))
        return AckPolicy::BLOCK_ACK;
    else
        return AckPolicy::NORMAL_ACK;
}
```

### Pending Queue Access

**Source:** `src/inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.cc` lines 81-100 and `HeDlMuTxOpFs.cc` lines 161-177
**Apply to:** preserving FIFO and leaving skipped packets pending

```cpp
if (pendingQueue->isEmpty())
    return nullptr;
else {
    Packet *packet = nullptr;
    if (aMsduAggregationPolicy)
        packet = aMsduAggregateIfNeeded(pendingQueue);
    if (!packet) {
        packet = pendingQueue->dequeuePacket();
        take(packet);
    }
    // PS Defer Queueing
    if (sequenceNumberAssignment) {
        auto header = packet->removeAtFront<Ieee80211DataOrMgmtHeader>();
        assignSequenceNumber(header);
        packet->insertAtFront(header);
    }
```

```cpp
int n = pendingQueue->getNumPackets();
for (int i = 0; i < n; ++i) {
    Packet *pkt = pendingQueue->getPacket(i);
    const auto& hdr = pkt->peekAtFront<Ieee80211MacHeader>();
    if (hdr->getReceiverAddress() == alloc.staAddress) {
        staPacket = pkt;
        break;
    }
}
```

### Logging and Runtime Errors

**Source:** `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc` lines 198-221; `HeDlMuTxOpFs.cc` lines 170-173 and 205-209
**Apply to:** missing BA diagnostics and impossible MU assembly state

```cpp
if (tx->isBusy()) {
    EV_WARN << "Channel access granted to the " << printAccessCategory(edcaf->getAccessCategory())
            << " queue while tx is busy (e.g. pending sequential Ack). Releasing channel.\n";
    edcaf->releaseChannel(this);
    return;
}
```

```cpp
if (staPacket == nullptr) {
    EV_WARN << "HeDlMuTxOpFs: no queued packet for STA " << alloc.staAddress
            << ", skipping RU " << alloc.ru.index << endl;
    continue;
}
```

## No Analog Found

No files lack analogs. This phase should modify existing INET 802.11 MAC components and add a focused `.test` following existing unit test structure.

## Metadata

**Analog search scope:** `src/inet/linklayer/ieee80211/mac`, `tests/unit`, `GEMINI.md`
**Files scanned:** 100+ via `rg --files` and targeted `rg`
**Pattern extraction date:** 2026-06-16
