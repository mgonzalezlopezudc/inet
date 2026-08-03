// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h"

#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/contract/DurationFinalizedReq.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

VhtDlMuTxOpFs::VhtDlMuTxOpFs(const VhtDlMuPlan& plan,
        Ieee80211ModeSet *modeSet, IAckHandler *ackHandler,
        IFrameSequenceHandler::ICallback *callback) :
    plan(plan), modeSet(modeSet), ackHandler(ackHandler), callback(callback)
{
    ASSERT(modeSet != nullptr && ackHandler != nullptr && callback != nullptr);
}

void VhtDlMuTxOpFs::startSequence(FrameSequenceContext *context, int firstStep)
{
    step = 0;
}

IFrameSequenceStep *VhtDlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    switch (step) {
        case 0:
            try {
                containerPacket = buildMuContainerPacket(context);
            }
            catch (const VhtDlMuStalePlan& error) {
                EV_WARN << "VHT DL MU transaction aborted before commit: "
                        << error.what() << endl;
                containerPacket = nullptr;
                activeUsers.clear();
            }
            return containerPacket == nullptr ? nullptr :
                    new TransmitStep(containerPacket, context->getIfs(), true);
        case 1:
            return new TransmitStep(buildBlockAckReq(context, 0),
                    modeSet->getSifsTime(), true);
        case 2: {
            auto transmitted = check_and_cast<ITransmitStep *>(context->getLastStep());
            auto request = transmitted->getFrameToTransmit()->peekAtFront<Ieee80211BlockAckReq>();
            auto timeout = context->getQoSContext()->ackPolicy->getBlockAckTimeout(
                    transmitted->getFrameToTransmit(), request);
            return new ReceiveStep(timeout, IReceiveStep::TimeoutHandling::COMPLETE_STEP,
                    [this](Packet *packet, FrameSequenceContext *context) {
                        return isExpectedBlockAck(packet, context, 0);
                    }, IReceiveStep::UnexpectedResponseHandling::IGNORE_RESPONSE,
                    IFrameSequenceStep::Completion::EXPIRED);
        }
        case 3:
            return new TransmitStep(buildBlockAckReq(context, 1),
                    modeSet->getSifsTime(), true);
        case 4: {
            auto transmitted = check_and_cast<ITransmitStep *>(context->getLastStep());
            auto request = transmitted->getFrameToTransmit()->peekAtFront<Ieee80211BlockAckReq>();
            auto timeout = context->getQoSContext()->ackPolicy->getBlockAckTimeout(
                    transmitted->getFrameToTransmit(), request);
            return new ReceiveStep(timeout, IReceiveStep::TimeoutHandling::COMPLETE_STEP,
                    [this](Packet *packet, FrameSequenceContext *context) {
                        return isExpectedBlockAck(packet, context, 1);
                    }, IReceiveStep::UnexpectedResponseHandling::IGNORE_RESPONSE,
                    IFrameSequenceStep::Completion::EXPIRED);
        }
        case 5:
            return nullptr;
        default:
            throw cRuntimeError("Invalid VHT DL MU sequence step");
    }
}

bool VhtDlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    if (step == 2 || step == 4) {
        int userIndex = step == 2 ? 0 : 1;
        auto receive = check_and_cast<IReceiveStep *>(context->getLastStep());
        if (receive->getReceivedFrame() == nullptr)
            callback->originatorProcessFailedFrame(activeUsers.at(userIndex).packet);
    }
    step++;
    return true;
}

Packet *VhtDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    const auto& users = plan.getUsers();
    ASSERT(users.size() == 2);
    auto hcf = dynamic_cast<Hcf *>(callback);
    auto dataService = hcf == nullptr ? nullptr :
            dynamic_cast<OriginatorQosMacDataService *>(hcf->getOriginatorMacDataService());
    if (dataService == nullptr)
        throw cRuntimeError("VHT DL MU requires OriginatorQosMacDataService");
    auto hcfModule = check_and_cast<cModule *>(callback);
    auto mac = check_and_cast<Ieee80211Mac *>(hcfModule->getParentModule());
    auto controlMode = modeSet->getSlowestMandatoryMode(MHz(20));
    auto barDuration = controlMode->getDuration(B(24));
    auto blockAckDuration = controlMode->getDuration(LENGTH_BASIC_BLOCKACK);
    auto exchangeNav = 4 * modeSet->getSifsTime() + 2 * barDuration + 2 * blockAckDuration;

    std::unique_ptr<ISequenceNumberAssignment> sequenceState =
            dataService->cloneSequenceNumberState();
    std::vector<std::unique_ptr<Packet>> prepared;
    std::vector<Ieee80211VhtMuUser> phyUsers;
    prepared.reserve(2);
    phyUsers.reserve(2);
    for (size_t i = 0; i < users.size(); ++i) {
        const auto& user = users[i];
        bool found = false;
        for (int j = 0; j < user.sourceQueue->getNumPackets(); ++j)
            found |= user.sourceQueue->getPacket(j) == user.packet;
        if (!found)
            throw VhtDlMuStalePlan("VHT DL MU selected packet changed queue membership before preparation");
        auto copy = std::unique_ptr<Packet>(user.packet->dup());
        auto header = copy->removeAtFront<Ieee80211DataHeader>();
        if (header->getType() != ST_DATA_WITH_QOS || header->getReceiverAddress() != user.peer ||
                header->getTid() != user.tid || header->getFragmentNumber() != 0 ||
                header->getMoreFragments())
            throw VhtDlMuStalePlan("VHT DL MU packet no longer satisfies the immutable plan");
        if (!header->getRetry())
            sequenceState->assignSequenceNumber(header);
        header->setAckPolicy(BLOCK_ACK);
        header->setDurationField(exchangeNav);
        copy->insertAtFront(header);
        if (copy->getDataLength() >= B(4) &&
                dynamicPtrCast<const Ieee80211MacTrailer>(copy->peekAtBack(B(4))) != nullptr) {
            auto trailer = copy->removeAtBack<Ieee80211MacTrailer>(B(4));
            auto fcsMode = mac->getFcsMode();
            trailer->setFcsMode(fcsMode);
            if (fcsMode == FCS_COMPUTED)
                trailer->setFcs(computeEthernetFcs(copy.get(), fcsMode));
            copy->insertAtBack(trailer);
        }
        auto psduBytes = B(4) + B(copy->getByteLength());
        auto padding = (4 - psduBytes.get<B>() % 4) % 4;
        psduBytes += B(padding);
        auto mode = modeSet->findVhtMode(user.mcs, 1, MHz(20), user.ldpc);
        if (mode == nullptr)
            throw cRuntimeError("No legal VHT mode for DL MU user");
        Ieee80211VhtMuUser phyUser;
        phyUser.associationId = user.associationId;
        phyUser.userPosition = user.userPosition;
        phyUser.mcs = user.mcs;
        phyUser.ldpcCoding = user.ldpc;
        phyUser.psduLength = psduBytes;
        // Each user contributes only its VHT Data-field duration. The common
        // two-LTF preamble is represented once by the canonical TXVECTOR.
        phyUser.duration = mode->getDataMode()->getDuration(psduBytes);
        phyUser.beamformingGainDb = user.beamformingGainDb;
        phyUser.leakagePenaltyDb = user.leakagePenaltyDb;
        phyUsers.push_back(phyUser);
        prepared.push_back(std::move(copy));
    }

    auto txVector = Ieee80211VhtTxVector::createMu(MHz(20), 1, phyUsers,
            Ieee80211VhtPreambleMode::getMuPreambleDuration(2));
    auto container = std::make_unique<Packet>("VHT-MU-PPDU");
    for (size_t i = 0; i < prepared.size(); ++i) {
        auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
        delimiter->setLength(prepared[i]->getByteLength());
        container->insertAtBack(delimiter);
        container->insertAtBack(prepared[i]->peekData());
        auto padding = (4 - (B(4) + B(prepared[i]->getByteLength())).get<B>() % 4) % 4;
        if (padding != 0)
            container->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
    }
    container->addTag<Ieee80211VhtTxVectorReq>()->setTxVector(txVector);

    // Revalidate and exercise all fallible hooks before the single ownership
    // boundary; an exception leaves queue, sequence, and BA state untouched.
    for (size_t i = 0; i < users.size(); ++i) {
        bool found = false;
        for (int j = 0; j < users[i].sourceQueue->getNumPackets(); ++j)
            found |= users[i].sourceQueue->getPacket(j) == users[i].packet;
        if (!found)
            throw VhtDlMuStalePlan("VHT DL MU selected packet changed queue membership before commit");
        beforePacketCommit(i);
    }

    auto originalSequenceState = dataService->cloneSequenceNumberState();
    std::vector<bool> ackApplied(users.size(), false);
    try {
        for (size_t i = 0; i < users.size(); ++i) {
            auto header = prepared[i]->peekAtFront<Ieee80211DataOrMgmtHeader>();
            ackApplied[i] = true;
            ackHandler->frameGotInProgress(header);
            afterCommitMutation(CommitMutation::ACK_STATE, i);
        }
        dataService->commitSequenceNumberState(*sequenceState);
        afterCommitMutation(CommitMutation::SEQUENCE_STATE, -1);
    }
    catch (...) {
        for (size_t i = 0; i < users.size(); ++i) {
            if (ackApplied[i])
                ackHandler->retireFrame(prepared[i]->peekAtFront<Ieee80211DataOrMgmtHeader>());
        }
        dataService->commitSequenceNumberState(*originalSequenceState);
        activeUsers.clear();
        throw;
    }
    // From here to publication there are no policy calls or fault hooks: all
    // stale/fallible work completed before the first queue/in-progress signal.
    for (size_t i = 0; i < users.size(); ++i) {
        auto original = users[i].packet;
        users[i].sourceQueue->removePacket(original);
        original->removeAll();
        original->insertAtBack(prepared[i]->peekAll());
        original->setFrontOffset(prepared[i]->getFrontOffset());
        original->setBackOffset(prepared[i]->getBackOffset());
        original->clearTags();
        original->copyTags(*prepared[i]);
        original->getRegionTags() = prepared[i]->getRegionTags();
        context->getInProgressFrames()->addInProgressFrame(original);
    }
    activeUsers.clear();
    for (const auto& user : users)
        activeUsers.push_back({user, user.packet});
    return container.release();
}

Packet *VhtDlMuTxOpFs::buildBlockAckReq(FrameSequenceContext *context,
        int userIndex) const
{
    const auto& user = activeUsers.at(userIndex);
    auto header = user.packet->peekAtFront<Ieee80211DataHeader>();
    auto request = context->getQoSContext()->blockAckProcedure->
            buildBasicBlockAckReqFrame(user.candidate.peer, header->getTid(),
                    header->getSequenceNumber());
    auto controlMode = modeSet->getSlowestMandatoryMode(MHz(20));
    auto blockAckDuration = controlMode->getDuration(LENGTH_BASIC_BLOCKACK);
    auto remaining = modeSet->getSifsTime() + blockAckDuration;
    if (userIndex == 0)
        remaining += modeSet->getSifsTime() + controlMode->getDuration(B(24)) +
                modeSet->getSifsTime() + blockAckDuration;
    request->setDurationField(remaining);
    auto packet = new Packet("VHT-MU-BasicBlockAckReq", request);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    packet->addTag<DurationFinalizedReq>();
    return packet;
}

bool VhtDlMuTxOpFs::isExpectedBlockAck(Packet *packet,
        FrameSequenceContext *context, int userIndex) const
{
    if (packet == nullptr)
        return false;
    auto blockAck = dynamicPtrCast<const Ieee80211BlockAck>(
            packet->peekAtFront<Ieee80211MacHeader>());
    int tid = -1;
    if (auto basic = dynamicPtrCast<const Ieee80211BasicBlockAck>(blockAck))
        tid = basic->getTidInfo();
    else if (auto compressed = dynamicPtrCast<const Ieee80211CompressedBlockAck>(blockAck))
        tid = compressed->getTidInfo();
    return blockAck != nullptr && context->isForUs(blockAck) &&
            blockAck->getTransmitterAddress() == activeUsers.at(userIndex).candidate.peer &&
            tid == activeUsers.at(userIndex).candidate.tid;
}

} // namespace ieee80211
} // namespace inet
