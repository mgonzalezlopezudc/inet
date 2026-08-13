//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfRecipientService.h"

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/common/Ieee80211FcsChecker.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

void HcfRecipientService::deleteWithObservation(Packet *packet, DropReason reason,
        IActions& actions) noexcept
{
    actions.packetDropped(packet, reason);
    actions.deletePacket(packet);
}

void HcfRecipientService::cleanUpOwnedPackets(Packet *aggregate,
        Packet *elicitingAggregate,
        std::vector<std::pair<Packet *, Ptr<const Ieee80211MacHeader>>>& members,
        IActions& actions) noexcept
{
    for (auto& member : members)
        if (member.first != nullptr) {
            actions.deletePacket(member.first);
            member.first = nullptr;
        }
    if (elicitingAggregate != nullptr)
        actions.deletePacket(elicitingAggregate);
    if (aggregate != nullptr)
        actions.deletePacket(aggregate);
}

void HcfRecipientService::processFrame(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header, IActions& actions) const
{
    if (packet == nullptr)
        throw cRuntimeError("HCF recipient service requires a packet and MAC header");

    try {
        if (header == nullptr)
            throw cRuntimeError("HCF recipient service requires a packet and MAC header");
        constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
                Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
        auto packetHeader = dynamicPtrCast<const Ieee80211MacHeader>(
                packet->peekAtFront(b(-1), parsingFlags));
        if (packetHeader.get() != header.get())
            throw cRuntimeError("HCF recipient header does not belong to the submitted packet");

        auto disposition = actions.classifyAddress(packet, header);
        if (disposition == AddressDisposition::FOREIGN) {
            deleteWithObservation(packet, DropReason::NOT_ADDRESSED_TO_US, actions);
            return;
        }

        actions.packetReceived(packet, header);
        if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
            actions.processImmediateResponse(packet, dataOrMgmtHeader);

        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
            // QoS Null is a control/state carrier, not an MSDU for the MAC SAP.
            if (dataHeader->getType() == ST_QOS_NULL)
                actions.deletePacket(packet);
            else
                actions.deliverData(packet, dataHeader, false);
        }
        else if (auto managementHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
            actions.deliverManagement(packet, managementHeader);
        else {
            actions.processControl(packet, header);
            actions.deletePacket(packet);
        }
    }
    catch (...) {
        actions.deletePacket(packet);
        throw;
    }
}

HcfRecipientService::Result HcfRecipientService::processAmpdu(Packet *aggregate,
        AggregateResponsePolicy responsePolicy, IActions& actions) const
{
    if (aggregate == nullptr)
        throw cRuntimeError("HCF recipient service requires an A-MPDU packet");

    constexpr int parsingFlags = Chunk::PF_ALLOW_INCORRECT |
            Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED;
    Packet *elicitingAggregate = nullptr;
    std::vector<std::pair<Packet *, Ptr<const Ieee80211MacHeader>>> members;
    Result result;

    try {
        if (responsePolicy == AggregateResponsePolicy::HT_IMPLICIT_BLOCK_ACK)
            elicitingAggregate = aggregate->dup();
        auto receiveIndication = aggregate->findTag<Ieee80211MpduReceiveInd>();
        unsigned int resultIndex = 0;
        while (aggregate->getDataLength() > b(0) &&
                dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                        aggregate->peekAtFront(b(-1), parsingFlags)) != nullptr) {
            auto delimiter = aggregate->popAtFront<Ieee80211MpduSubframeHeader>(
                    b(-1), parsingFlags);
            auto mpduLength = B(delimiter->getLength());
            if (mpduLength == B(0))
                continue;

            auto status = delimiter->isIncorrect() ?
                    MPDU_DELIMITER_ERROR : MPDU_NOT_EVALUATED;
            if (!delimiter->isIncorrect() && receiveIndication != nullptr &&
                    resultIndex < receiveIndication->getResultsArraySize())
                status = receiveIndication->getResults(resultIndex).status;

            if (mpduLength > aggregate->getDataLength()) {
                actions.packetDropped(aggregate, DropReason::INCORRECTLY_RECEIVED);
                status = MPDU_PAYLOAD_ERROR;
                result.droppedMemberCount++;
            }
            else {
                auto member = new Packet(aggregate->getName());
                member->copyTags(*aggregate);
                member->removeTagIfPresent<Ieee80211MpduReceiveInd>();
                member->insertAtBack(aggregate->popAtFront(mpduLength, parsingFlags));
                auto memberHeader = dynamicPtrCast<const Ieee80211MacHeader>(
                        member->peekAtFront(b(-1), parsingFlags));
                if (!delimiter->isIncorrect() && receiveIndication == nullptr)
                    status = Ieee80211FcsChecker::isFcsOk(member,
                            AggregateReceptionContext::ORDINARY_FRAME) ?
                            MPDU_SUCCESS : MPDU_FCS_ERROR;
                if (status == MPDU_SUCCESS && memberHeader != nullptr)
                    members.emplace_back(member, memberHeader);
                else {
                    deleteWithObservation(member, DropReason::INCORRECTLY_RECEIVED,
                            actions);
                    result.droppedMemberCount++;
                }
            }
            result.decodedMemberCount++;
            resultIndex++;
            int padding = (4 - (B(4) + mpduLength).get<B>() % 4) % 4;
            if (padding > 0 && aggregate->getDataLength() >= B(padding))
                aggregate->popAtFront(B(padding), parsingFlags);
        }

        bool implicitBlockAckAggregate = false;
        if (responsePolicy == AggregateResponsePolicy::HT_IMPLICIT_BLOCK_ACK &&
                result.decodedMemberCount != 0) {
            for (const auto& member : members) {
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(member.second);
                if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS &&
                        dataHeader->getAckPolicy() == NORMAL_ACK) {
                    implicitBlockAckAggregate = true;
                    break;
                }
            }
        }

        if (implicitBlockAckAggregate) {
            std::vector<Ptr<const Ieee80211DataHeader>> admittedHeaders;
            for (const auto& member : members) {
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(member.second);
                if (dataHeader != nullptr &&
                        actions.classifyAddress(member.first, member.second) !=
                                AddressDisposition::FOREIGN)
                    admittedHeaders.push_back(dataHeader);
            }
            bool allMembersAreData = true;
            for (const auto& member : members)
                allMembersAreData &= dynamicPtrCast<const Ieee80211DataHeader>(
                        member.second) != nullptr;
            if (!admittedHeaders.empty() && allMembersAreData)
                result.aggregateResponseSent = actions.processHtImplicitBlockAckResponse(
                        elicitingAggregate, admittedHeaders);

            for (auto& member : members) {
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(member.second);
                auto disposition = dataHeader == nullptr ? AddressDisposition::FOREIGN :
                        actions.classifyAddress(member.first, member.second);
                if (dataHeader != nullptr && disposition != AddressDisposition::FOREIGN) {
                    actions.packetReceived(member.first, member.second);
                    actions.deliverData(member.first, dataHeader, true);
                    member.first = nullptr;
                    result.acceptedMemberCount++;
                }
                else {
                    auto reason = dataHeader != nullptr ? DropReason::NOT_ADDRESSED_TO_US :
                            DropReason::INVALID_IMPLICIT_BLOCK_ACK_MEMBER;
                    deleteWithObservation(member.first, reason, actions);
                    member.first = nullptr;
                    result.droppedMemberCount++;
                }
            }
        }
        else {
            for (auto& member : members) {
                auto disposition = actions.classifyAddress(member.first, member.second);
                if (disposition == AddressDisposition::FOREIGN) {
                    deleteWithObservation(member.first, DropReason::NOT_ADDRESSED_TO_US,
                            actions);
                    member.first = nullptr;
                    result.droppedMemberCount++;
                }
                else {
                    auto ownedMember = member.first;
                    member.first = nullptr;
                    actions.processOrdinaryAggregateMember(ownedMember, member.second);
                    result.acceptedMemberCount++;
                }
            }
        }
    }
    catch (...) {
        cleanUpOwnedPackets(aggregate, elicitingAggregate, members, actions);
        throw;
    }

    if (elicitingAggregate != nullptr)
        actions.deletePacket(elicitingAggregate);
    actions.deletePacket(aggregate);
    return result;
}

} // namespace ieee80211
} // namespace inet
