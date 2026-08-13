//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.h"

#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211FecCodingReq.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Packet *HcfAggregationService::buildAmpduPacket(
        const std::vector<Packet *>& frames, FcsMode fcsMode)
{
    Ptr<const Ieee80211FecCodingReq> fecCodingReq;
    for (auto frame : frames) {
        auto frameFecCodingReq = frame->findTag<Ieee80211FecCodingReq>();
        if (frameFecCodingReq != nullptr) {
            if (fecCodingReq != nullptr &&
                    fecCodingReq->getLdpcAllowed() != frameFecCodingReq->getLdpcAllowed())
                throw cRuntimeError("Cannot build an A-MPDU from MPDUs with different FEC coding requests");
            fecCodingReq = frameFecCodingReq;
        }
    }
    auto aggregatedPacket = new Packet();
    std::string aggregatedName;
    for (size_t i = 0; i < frames.size(); i++) {
        auto frame = frames[i];
        auto trailer = frame->removeAtBack<Ieee80211MacTrailer>(B(4));
        trailer->setFcsMode(fcsMode);
        if (fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(frame, fcsMode));
        frame->insertAtBack(trailer);
        auto mpdu = frame->peekAll();
        auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
        delimiter->setEof(false);
        delimiter->setReserved(0);
        delimiter->setLength(mpdu->getChunkLength().get<B>());
        aggregatedPacket->insertAtBack(delimiter);
        aggregatedPacket->insertAtBack(mpdu);
        aggregatedPacket->getRegionTags().copyTags(frame->getRegionTags(), B(0),
                aggregatedPacket->getBackOffset() - frame->getDataLength(), frame->getDataLength());
        int paddingLength = (4 - (delimiter->getChunkLength() + mpdu->getChunkLength()).get<B>() % 4) % 4;
        if (i + 1 != frames.size() && paddingLength != 0)
            aggregatedPacket->insertAtBack(makeShared<ByteCountChunk>(B(paddingLength)));
        if (i != 0)
            aggregatedName.append("+");
        aggregatedName.append(frame->getName());
    }
    aggregatedPacket->setName(aggregatedName.c_str());
    if (fecCodingReq != nullptr)
        aggregatedPacket->addTag<Ieee80211FecCodingReq>()->setLdpcAllowed(fecCodingReq->getLdpcAllowed());
    return aggregatedPacket;
}

B HcfAggregationService::calculateAmpduLength(
        const std::vector<Packet *>& frames)
{
    B aggregateLength(0);
    for (size_t i = 0; i < frames.size(); i++) {
        // Match buildAmpduPacket(): the complete stored MPDU (including its
        // existing MAC trailer) follows the four-byte delimiter.
        auto subframeLength = B(4) + frames[i]->getTotalLength();
        aggregateLength += subframeLength;
        if (i + 1 != frames.size())
            aggregateLength += B((4 - subframeLength.get<B>() % 4) % 4);
    }
    return aggregateLength;
}

HcfAggregationService::TransmissionPlan HcfAggregationService::planTransmission(
        const TransmissionPlanningRequest& request,
        const ITransmissionPlanningActions& actions) const
{
    TransmissionPlan result;
    result.implicitBlockAck = request.implicitBlockAck;
    if (!request.aggregationAllowed)
        return result;
    if (request.sourcePacket == nullptr || request.mode == nullptr)
        throw cRuntimeError("Incomplete HCF aggregation planning request");

    long long maxAggregateLength = 0;
    if (!request.implicitBlockAck) {
        auto sourceHeader = request.sourcePacket->peekAtFront<Ieee80211DataHeader>();
        maxAggregateLength = actions.getMaxAggregateLength(sourceHeader,
                request.phyFamily);
    }
    auto candidates = actions.getCandidates(request.sourcePacket,
            request.implicitBlockAck, maxAggregateLength);
    for (auto candidate : candidates) {
        if (candidate == nullptr)
            throw cRuntimeError("HCF aggregation candidate is null");
        // IEEE Std 802.11-2024, 10.3.2.11, 10.13 and 10.25: retain
        // baseline nontransactional ordering so retry-sensitive Block Ack
        // policy observes the materialized retry state.
        actions.applyRetryState(candidate);
        auto header = candidate->peekAtFront<Ieee80211DataHeader>();
        auto ackPolicy = actions.selectAckPolicy(candidate, header);
        if (!request.implicitBlockAck && ackPolicy != BLOCK_ACK)
            break;
        actions.applySelectedPolicy(candidate, ackPolicy);
        result.members.push_back(candidate);
    }

    if (!request.implicitBlockAck &&
            (request.phyFamily == Ieee80211PhyFamily::HT ||
             request.phyFamily == Ieee80211PhyFamily::HE)) {
        auto durationLimit = request.mode->getPpduMaxDuration();
        auto originalCount = result.members.size();
        while (result.members.size() > 1 &&
                request.mode->getDuration(calculateAmpduLength(result.members)) >
                durationLimit)
            result.members.pop_back();
        if (!result.members.empty() &&
                request.mode->getDuration(calculateAmpduLength(result.members)) >
                durationLimit)
            throw cRuntimeError("Selected PPDU exceeds the maximum duration for the PHY mode");
        if (result.members.size() != originalCount)
            actions.aggregationTrimmed(originalCount, result.members.size(),
                    durationLimit);
    }
    result.materialize = request.implicitBlockAck ? !result.members.empty() :
            result.members.size() > 1;
    if (!result.materialize)
        result.members.clear();
    return result;
}

std::vector<Packet *> HcfAggregationService::selectHtImplicitBlockAckFrames(
        const HtImplicitSelectionRequest& request,
        const IHtImplicitSelectionActions& actions) const
{
    if (!request.enabled || request.sourcePacket == nullptr)
        return {};
    if (request.mode == nullptr)
        throw cRuntimeError("HT implicit BlockAck selection has no PHY mode");
    if (request.phyFamily != Ieee80211PhyFamily::HT &&
            request.phyFamily != Ieee80211PhyFamily::VHT &&
            request.phyFamily != Ieee80211PhyFamily::HE &&
            request.phyFamily != Ieee80211PhyFamily::EHT)
        return {};

    auto sourceHeader = request.sourcePacket->peekAtFront<Ieee80211DataHeader>();
    auto maxAggregateLength = actions.getMaxAggregateLength(sourceHeader,
            request.phyFamily);
    std::vector<Packet *> result;
    for (auto candidate : actions.getCandidates(request.sourcePacket)) {
        if (candidate == nullptr)
            throw cRuntimeError("HT implicit BlockAck candidate is null");
        auto header = candidate->peekAtFront<Ieee80211DataHeader>();
        // IEEE Std 802.11-2024, 10.12.2: an HT A-MPDU MPDU is at most
        // 4095 octets and cannot be a fragment.
        if (candidate->getTotalLength() > B(4095) ||
                header->getFragmentNumber() != 0 || header->getMoreFragments() ||
                actions.selectAckPolicy(candidate, header) != BLOCK_ACK)
            break;
        result.push_back(candidate);
    }
    while (result.size() > 1 &&
            (calculateAmpduLength(result).get<B>() > maxAggregateLength ||
             request.mode->getDuration(calculateAmpduLength(result)) >
             request.mode->getPpduMaxDuration()))
        result.pop_back();
    return result;
}

Packet *HcfAggregationService::materializeTransmission(Packet *ledgerKey,
        const std::vector<Packet *>& subframes, FcsMode fcsMode,
        bool implicitBlockAck)
{
    auto aggregate = buildAmpduPacket(subframes, fcsMode);
    transmissionLedger.record(ledgerKey, subframes, implicitBlockAck);
    return aggregate;
}

bool HcfAggregationService::hasImplicitBlockAck(Packet *packet) const
{
    return transmissionLedger.hasImplicitBlockAck(packet);
}

std::optional<AmpduTransmissionLedger::Entry>
HcfAggregationService::takeTransmission(Packet *packet)
{
    return transmissionLedger.take(packet);
}

bool HcfAggregationService::discardTransmission(Packet *packet)
{
    return transmissionLedger.discard(packet);
}

} // namespace ieee80211
} // namespace inet
