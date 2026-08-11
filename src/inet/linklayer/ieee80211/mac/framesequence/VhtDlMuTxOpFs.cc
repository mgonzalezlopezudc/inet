// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h"

#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/aggregation/MpduAggregation.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/contract/DurationFinalizedReq.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
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
    if (step == 0) {
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
    }
    auto userCount = plan.getUsers().size();
    if (step > static_cast<int>(2 * userCount))
        return nullptr;
    auto userIndex = (step - 1) / 2;
    if (step % 2 == 1)
        return new TransmitStep(buildBlockAckReq(context, userIndex),
                modeSet->getSifsTime(), true);
    auto transmitted = check_and_cast<ITransmitStep *>(context->getLastStep());
    auto request = transmitted->getFrameToTransmit()->peekAtFront<Ieee80211BlockAckReq>();
    auto timeout = context->getQoSContext()->ackPolicy->getBlockAckTimeout(
            transmitted->getFrameToTransmit(), request);
    return new ReceiveStep(timeout, IReceiveStep::TimeoutHandling::COMPLETE_STEP,
            [this, userIndex](Packet *packet, FrameSequenceContext *context) {
                return isExpectedBlockAck(packet, context, userIndex);
            }, IReceiveStep::UnexpectedResponseHandling::IGNORE_RESPONSE,
            IFrameSequenceStep::Completion::EXPIRED);
}

bool VhtDlMuTxOpFs::completeStep(FrameSequenceContext *context)
{
    if (step > 0 && step % 2 == 0) {
        auto userIndex = (step - 1) / 2;
        auto receive = check_and_cast<IReceiveStep *>(context->getLastStep());
        if (receive->getReceivedFrame() == nullptr)
            for (auto packet : activeUsers.at(userIndex).packets)
                callback->originatorProcessFailedFrame(packet);
    }
    step++;
    return true;
}

Packet *VhtDlMuTxOpFs::buildMuContainerPacket(FrameSequenceContext *context)
{
    struct PreparedMember {
        Packet *original = nullptr;
        bool inProgress = false;
        std::unique_ptr<Packet> packet;
    };

    const auto& users = plan.getUsers();
    ASSERT(users.size() >= 2 && users.size() <= 4);
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
    auto exchangeNav = 2 * users.size() * modeSet->getSifsTime() +
            users.size() * (barDuration + blockAckDuration);

    std::unique_ptr<ISequenceNumberAssignment> sequenceState =
            dataService->cloneSequenceNumberState();
    std::vector<std::vector<PreparedMember>> preparedUsers(users.size());
    std::vector<std::unique_ptr<Packet>> userPsdus;
    std::vector<Ieee80211VhtMuUser> phyUsers;
    bool ldpcExtraOfdmSymbol = false;
    unsigned int totalNsts = 0;
    userPsdus.reserve(users.size());
    phyUsers.reserve(users.size());
    for (size_t i = 0; i < users.size(); ++i) {
        const auto& user = users[i];
        auto isInProgress = [context](Packet *packet) {
            for (int j = 0; j < context->getInProgressFrames()->getLength(); ++j)
                if (context->getInProgressFrames()->getFrames(j) == packet)
                    return true;
            return false;
        };
        auto isQueued = [&user](Packet *packet) {
            for (int j = 0; j < user.sourceQueue->getNumPackets(); ++j)
                if (user.sourceQueue->getPacket(j) == packet)
                    return true;
            return false;
        };
        if (!isInProgress(user.packet) && !isQueued(user.packet))
            throw VhtDlMuStalePlan("VHT DL MU selected packet changed ownership before preparation");

        auto mode = modeSet->findVhtMode(user.mcs, user.numberOfSpatialStreams,
                plan.getContext().channelWidth, user.ldpc);
        if (mode == nullptr)
            throw cRuntimeError("No legal VHT mode for DL MU user");
        auto vhtMode = check_and_cast<const Ieee80211VhtMode *>(mode);
        auto negotiated = mac->getMib()->getNegotiatedVhtCapabilities(user.peer);
        if (!negotiated || !negotiated->localTxPeerRx.valid)
            throw VhtDlMuStalePlan("VHT DL MU peer capabilities changed before preparation");
        int maxAmpduLengthExponent = negotiated->localTxPeerRx.receiverMaxAmpduLengthExponent;
        auto policy = check_and_cast<cModule *>(dataService)->getSubmodule("mpduAggregationPolicy");
        if (policy != nullptr && policy->hasPar("maxAmpduLengthExponent"))
            maxAmpduLengthExponent = std::min(maxAmpduLengthExponent,
                    (int)policy->par("maxAmpduLengthExponent").intValue());
        if (maxAmpduLengthExponent < 0 || maxAmpduLengthExponent > 7)
            throw cRuntimeError("Invalid VHT A-MPDU length exponent: %d",
                    maxAmpduLengthExponent);
        auto maxAmpduLength = (1LL << (13 + maxAmpduLengthExponent)) - 1;

        std::vector<Packet *> candidates = {user.packet};
        for (int j = 0; j < context->getInProgressFrames()->getLength(); ++j) {
            auto packet = context->getInProgressFrames()->getFrames(j);
            if (packet != user.packet && ackHandler->isEligibleToTransmit(
                    packet->peekAtFront<Ieee80211DataOrMgmtHeader>()))
                candidates.push_back(packet);
        }
        for (int j = 0; j < user.sourceQueue->getNumPackets(); ++j) {
            auto packet = user.sourceQueue->getPacket(j);
            if (std::find(candidates.begin(), candidates.end(), packet) == candidates.end())
                candidates.push_back(packet);
        }

        B aggregateLength(0);
        B previousSubframeLength(0);
        for (auto original : candidates) {
            if (preparedUsers[i].size() == 64)
                break;
            bool inProgress = isInProgress(original);
            auto originalHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    original->peekAtFront<Ieee80211MacHeader>());
            if (originalHeader == nullptr || originalHeader->getType() != ST_DATA_WITH_QOS ||
                    originalHeader->getReceiverAddress() != user.peer ||
                    originalHeader->getTid() != user.tid ||
                    originalHeader->getFragmentNumber() != 0 ||
                    originalHeader->getMoreFragments()) {
                if (original == user.packet)
                    throw VhtDlMuStalePlan("VHT DL MU packet no longer satisfies the immutable plan");
                continue;
            }
            if (inProgress && !ackHandler->isEligibleToTransmit(originalHeader)) {
                if (original == user.packet)
                    throw VhtDlMuStalePlan("Selected VHT DL MU retry frame is no longer eligible");
                continue;
            }
            auto copy = std::unique_ptr<Packet>(original->dup());
            if (inProgress) {
                if (auto qosAckHandler = dynamic_cast<QosAckHandler *>(ackHandler))
                    qosAckHandler->setRetryBitIfNeeded(copy.get());
            }
            if (copy->getTotalLength() > B(4095)) {
                if (original == user.packet)
                    throw cRuntimeError("Selected VHT A-MPDU member exceeds 4095 bytes");
                continue;
            }
            auto subframeLength = B(4) + copy->getTotalLength();
            auto candidateLength = aggregateLength + subframeLength;
            if (!preparedUsers[i].empty())
                candidateLength += B((4 - previousSubframeLength.get<B>() % 4) % 4);

            // IEEE Std 802.11-2024, 9.7.1: preserve the typed A-MPDU
            // delimiter/padding/member representation and limits. Clause 21.1.1
            // constrains VHT MU users/NSTS; the selected mode remains the PPDU authority.
            auto paddedCandidateLength = candidateLength + B(
                    (4 - candidateLength.get<B>() % 4) % 4);
            if (paddedCandidateLength.get<B>() > maxAmpduLength ||
                    mode->getDuration(paddedCandidateLength) > mode->getPpduMaxDuration()) {
                if (original == user.packet)
                    throw cRuntimeError("Selected VHT MU PSDU exceeds the PHY mode limits");
                break;
            }

            auto header = copy->removeAtFront<Ieee80211DataHeader>();
            if (!inProgress && !header->getRetry())
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
            aggregateLength = candidateLength;
            previousSubframeLength = subframeLength;
            preparedUsers[i].push_back({original, inProgress, std::move(copy)});
        }

        std::vector<Packet *> aggregateCopies;
        for (const auto& member : preparedUsers[i])
            aggregateCopies.push_back(member.packet->dup());
        MpduAggregation aggregation;
        auto psdu = std::unique_ptr<Packet>(aggregation.aggregateFrames(&aggregateCopies));
        auto psduPadding = (4 - psdu->getDataLength().get<B>() % 4) % 4;
        if (psduPadding != 0)
            psdu->insertAtBack(makeShared<ByteCountChunk>(B(psduPadding)));
        auto psduBytes = psdu->getDataLength();
        Ieee80211VhtMuUser phyUser;
        phyUser.associationId = user.associationId;
        phyUser.userPosition = user.userPosition;
        phyUser.numberOfSpatialStreams = user.numberOfSpatialStreams;
        phyUser.mcs = user.mcs;
        phyUser.ldpcCoding = user.ldpc;
        phyUser.psduLength = psduBytes;
        // Each user contributes only its VHT Data-field duration. The common
        // preamble is represented once by the canonical TXVECTOR.
        phyUser.duration = vhtMode->getDataMode()->getDuration(psduBytes);
        ldpcExtraOfdmSymbol |= user.ldpc &&
                vhtMode->getDataMode()->getLdpcExtraOfdmSymbol(b(psduBytes));
        totalNsts += user.numberOfSpatialStreams;
        phyUser.beamformingGainDb = user.beamformingGainDb;
        phyUser.leakagePenaltyDb = user.leakagePenaltyDb;
        phyUsers.push_back(phyUser);
        userPsdus.push_back(std::move(psdu));
    }

    auto txVector = Ieee80211VhtTxVector::createMu(plan.getContext().channelWidth,
            plan.getContext().groupId, phyUsers,
            Ieee80211VhtPreambleMode::getMuPreambleDuration(totalNsts),
            plan.getContext().shortGi, ldpcExtraOfdmSymbol);
    auto container = std::make_unique<Packet>("VHT-MU-PPDU");
    for (const auto& psdu : userPsdus) {
        container->insertAtBack(psdu->peekData());
        container->getRegionTags().copyTags(psdu->getRegionTags(), psdu->getFrontOffset(),
                container->getBackOffset() - psdu->getDataLength(), psdu->getDataLength());
    }
    container->addTag<Ieee80211VhtTxVectorReq>()->setTxVector(txVector);

    // Revalidate and exercise all fallible hooks before the single ownership
    // boundary; an exception leaves queue, sequence, and BA state untouched.
    int packetIndex = 0;
    for (size_t i = 0; i < users.size(); ++i) {
        for (const auto& member : preparedUsers[i]) {
            bool found = member.inProgress;
            for (int j = 0; j < users[i].sourceQueue->getNumPackets(); ++j)
                found |= users[i].sourceQueue->getPacket(j) == member.original;
            if (!found)
                throw VhtDlMuStalePlan("VHT DL MU packet changed ownership before commit");
            beforePacketCommit(packetIndex++);
        }
    }

    auto originalSequenceState = dataService->cloneSequenceNumberState();
    std::vector<std::vector<bool>> ackApplied(users.size());
    for (size_t i = 0; i < users.size(); ++i)
        ackApplied[i].resize(preparedUsers[i].size(), false);
    try {
        packetIndex = 0;
        for (size_t i = 0; i < users.size(); ++i) {
            for (size_t j = 0; j < preparedUsers[i].size(); ++j, ++packetIndex) {
                auto& member = preparedUsers[i][j];
                if (!member.inProgress) {
                    ackApplied[i][j] = true;
                    ackHandler->frameGotInProgress(member.packet->peekAtFront<Ieee80211DataOrMgmtHeader>());
                    afterCommitMutation(CommitMutation::ACK_STATE, packetIndex);
                }
            }
        }
        dataService->commitSequenceNumberState(*sequenceState);
        afterCommitMutation(CommitMutation::SEQUENCE_STATE, -1);
    }
    catch (...) {
        for (size_t i = 0; i < users.size(); ++i) {
            for (size_t j = 0; j < preparedUsers[i].size(); ++j)
                if (ackApplied[i][j])
                    ackHandler->retireFrame(preparedUsers[i][j].packet->peekAtFront<Ieee80211DataOrMgmtHeader>());
        }
        dataService->commitSequenceNumberState(*originalSequenceState);
        activeUsers.clear();
        throw;
    }
    // From here to publication there are no policy calls or fault hooks: all
    // stale/fallible work completed before the first queue/in-progress signal.
    for (size_t i = 0; i < users.size(); ++i) {
        for (auto& member : preparedUsers[i]) {
            if (!member.inProgress)
                users[i].sourceQueue->removePacket(member.original);
            member.original->removeAll();
            member.original->insertAtBack(member.packet->peekAll());
            member.original->setFrontOffset(member.packet->getFrontOffset());
            member.original->setBackOffset(member.packet->getBackOffset());
            member.original->clearTags();
            member.original->copyTags(*member.packet);
            member.original->getRegionTags() = member.packet->getRegionTags();
            if (!member.inProgress)
                context->getInProgressFrames()->addInProgressFrame(member.original);
        }
    }
    activeUsers.clear();
    for (size_t i = 0; i < users.size(); ++i) {
        std::vector<Packet *> packets;
        for (const auto& member : preparedUsers[i])
            packets.push_back(member.original);
        activeUsers.push_back({users[i], packets});
    }
    return container.release();
}

Packet *VhtDlMuTxOpFs::buildBlockAckReq(FrameSequenceContext *context,
        int userIndex) const
{
    const auto& user = activeUsers.at(userIndex);
    auto header = user.packets.front()->peekAtFront<Ieee80211DataHeader>();
    auto request = context->getQoSContext()->blockAckProcedure->
            buildBasicBlockAckReqFrame(user.candidate.peer, header->getTid(),
                    header->getSequenceNumber());
    auto controlMode = modeSet->getSlowestMandatoryMode(MHz(20));
    auto blockAckDuration = controlMode->getDuration(LENGTH_BASIC_BLOCKACK);
    auto remainingUsers = activeUsers.size() - userIndex;
    auto remaining = (2 * remainingUsers - 1) * modeSet->getSifsTime() +
            (remainingUsers - 1) * controlMode->getDuration(B(24)) +
            remainingUsers * blockAckDuration;
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

std::string VhtDlMuTxOpFs::getHistory() const
{
    std::string history = "VHT-DL-MU (PPDU";
    for (size_t i = 0; i < plan.getUsers().size(); ++i)
        history += "-BAR" + std::to_string(i) + "-BA" + std::to_string(i);
    return history + ")";
}

} // namespace ieee80211
} // namespace inet
