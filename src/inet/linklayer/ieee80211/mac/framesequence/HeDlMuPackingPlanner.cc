//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuPackingPlanner.h"

#include <algorithm>
#include <map>

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

B HeDlMuPackingPlanner::calculateAmpduPsduLength(const std::vector<Packet *>& packets)
{
    if (packets.empty())
        return B(0);
    B length(0);
    for (size_t i = 0; i < packets.size(); ++i) {
        B subframeLength = B(4) + B(packets[i]->getByteLength());
        length += subframeLength;
        if (i + 1 != packets.size())
            length += B((4 - subframeLength.get<B>() % 4) % 4);
    }
    return length;
}

HeDlMuPackingPlanner::Plan HeDlMuPackingPlanner::plan(const Parameters& parameters) const
{
    Plan plan;
    if (parameters.pendingQueue == nullptr || !parameters.hasActiveBlockAckAgreement ||
            !parameters.getAvailableBlockAckSlots || !parameters.warnIneligible) {
        plan.failureReason = "packing planner parameters are incomplete";
        return plan;
    }
    std::map<uint16_t, int> associationIdCounts;
    for (const auto& selectedAllocation : parameters.selectedAllocations)
        associationIdCounts[selectedAllocation.associationId]++;

    std::vector<SelectedAllocation> finalAllocations;
    for (auto selectedAllocation : parameters.selectedAllocations) {
        auto dataHeader = selectedAllocation.packet == nullptr ? nullptr :
                dynamicPtrCast<const Ieee80211DataHeader>(
                        selectedAllocation.packet->peekAtFront<Ieee80211MacHeader>());
        if (associationIdCounts[selectedAllocation.associationId] > 1) {
            Tid tid = dataHeader == nullptr ? -1 : dataHeader->getTid();
            auto receiverAddress = dataHeader == nullptr ? selectedAllocation.allocation.staAddress : dataHeader->getReceiverAddress();
            parameters.warnIneligible(selectedAllocation.packet, receiverAddress, tid, selectedAllocation.allocation.ru.index,
                    "association ID collides with another scheduled receiver");
            plan.rejectedFinalValidation++;
            continue;
        }

        if (dataHeader == nullptr || dataHeader->getType() != ST_DATA_WITH_QOS ||
                dataHeader->getReceiverAddress() != selectedAllocation.allocation.staAddress ||
                !parameters.hasActiveBlockAckAgreement(dataHeader->getReceiverAddress(), dataHeader->getTid())) {
            Tid tid = dataHeader == nullptr ? -1 : dataHeader->getTid();
            auto receiverAddress = dataHeader == nullptr ? selectedAllocation.allocation.staAddress : dataHeader->getReceiverAddress();
            parameters.warnIneligible(selectedAllocation.packet, receiverAddress, tid, selectedAllocation.allocation.ru.index,
                    "failed final validation before queue removal");
            plan.rejectedFinalValidation++;
            continue;
        }

        if (parameters.getAvailableBlockAckSlots(dataHeader->getReceiverAddress(), dataHeader->getTid()) <= 0) {
            parameters.warnIneligible(selectedAllocation.packet, dataHeader->getReceiverAddress(), dataHeader->getTid(),
                    selectedAllocation.allocation.ru.index, "Block Ack window has no available entries");
            plan.rejectedFinalValidation++;
            continue;
        }

        auto queueForPacking = selectedAllocation.sourceQueue == nullptr ? parameters.pendingQueue : selectedAllocation.sourceQueue;
        if (queueForPacking == nullptr) {
            plan.failureReason = "selected allocation has no source queue";
            return plan;
        }
        B packedLength = calculateAmpduPsduLength(selectedAllocation.packets);
        std::map<Tid, int> selectedPacketsByTid;
        for (int i = 0; i < queueForPacking->getNumPackets() &&
                (int)selectedAllocation.packets.size() < parameters.maxAmpduMpduCount; ++i) {
            Packet *candidatePacket = queueForPacking->getPacket(i);
            auto candidateHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    candidatePacket->peekAtFront<Ieee80211MacHeader>());
            if (candidateHeader == nullptr || candidateHeader->getType() != ST_DATA_WITH_QOS ||
                    candidateHeader->getReceiverAddress() != selectedAllocation.allocation.staAddress ||
                    (!selectedAllocation.multiTidAggregation && candidateHeader->getTid() != dataHeader->getTid()) ||
                    !parameters.hasActiveBlockAckAgreement(candidateHeader->getReceiverAddress(), candidateHeader->getTid()))
                continue;
            if (selectedPacketsByTid[candidateHeader->getTid()] >=
                    parameters.getAvailableBlockAckSlots(candidateHeader->getReceiverAddress(), candidateHeader->getTid()))
                continue;

            B proposedLength = packedLength;
            if (!selectedAllocation.packets.empty()) {
                B previousSubframeLength = B(4) + B(selectedAllocation.packets.back()->getByteLength());
                proposedLength += B((4 - previousSubframeLength.get<B>() % 4) % 4);
            }
            proposedLength += B(4) + B(candidatePacket->getByteLength());
            if (proposedLength.get<B>() > parameters.maxHeMuPsduLength)
                break;
            if (estimateHeMuUserDuration(proposedLength, selectedAllocation.allocation.ru.toneSize,
                    selectedAllocation.allocation.mcs,
                    selectedAllocation.allocation.numberOfSpatialStreams,
                    selectedAllocation.allocation.dcm,
                    parameters.scheduleContext.guardInterval) > parameters.packingDurationLimit)
                break;
            selectedAllocation.packets.push_back(candidatePacket);
            selectedPacketsByTid[candidateHeader->getTid()]++;
            packedLength = proposedLength;
            selectedAllocation.psduLength = proposedLength;
        }
        if (selectedAllocation.packets.empty()) {
            parameters.warnIneligible(selectedAllocation.packet, dataHeader->getReceiverAddress(), dataHeader->getTid(),
                    selectedAllocation.allocation.ru.index,
                    "HoL MPDU exceeds aligned, TXOP, or HE PPDU packing limit");
            plan.rejectedFinalValidation++;
            continue;
        }
        finalAllocations.push_back(selectedAllocation);
    }

    if (finalAllocations.size() < 2) {
        plan.failureReason = "fewer than two active Block Ack allocations remain after final validation";
        return plan;
    }

    auto calculatePlannedPpdu = [&] {
        std::vector<Ieee80211HeUserPhyParameters> users;
        std::map<int, int> ruStreamStartIndex;
        for (const auto& selectedAllocation : finalAllocations) {
            Ieee80211HeUserPhyParameters user;
            user.ru = selectedAllocation.allocation.ru;
            user.mcs = selectedAllocation.allocation.mcs;
            user.numberOfSpatialStreams = selectedAllocation.allocation.numberOfSpatialStreams;
            user.dcm = selectedAllocation.allocation.dcm;
            user.leakageSum = selectedAllocation.allocation.leakageSum;
            user.coding = parameters.scheduleContext.coding;
            user.psduLength = selectedAllocation.psduLength;
            user.staId = selectedAllocation.associationId;
            user.streamStartIndex = ruStreamStartIndex[user.ru.index];
            ruStreamStartIndex[user.ru.index] += user.numberOfSpatialStreams;
            users.push_back(user);
        }
        return computeHePpduParameters(users, parameters.scheduleContext.channelBandwidth,
                HE_MU_DOWNLINK, parameters.scheduleContext.guardInterval,
                parameters.scheduleContext.ltfType,
                parameters.scheduleContext.packetExtensionDurationUs);
    };

    auto plannedPpdu = calculatePlannedPpdu();
    while ((!plannedPpdu || plannedPpdu.parameters.duration > parameters.ppduDurationLimit) &&
            finalAllocations.size() >= 2) {
        auto longest = std::max_element(finalAllocations.begin(), finalAllocations.end(),
                [] (const SelectedAllocation& a, const SelectedAllocation& b) {
                    return a.psduLength < b.psduLength;
                });
        ASSERT(longest != finalAllocations.end());
        if (longest->packets.size() > 1) {
            B previousSubframeLength = B(4) +
                    B(longest->packets[longest->packets.size() - 2]->getByteLength());
            B previousSubframePadding =
                    B((4 - previousSubframeLength.get<B>() % 4) % 4);
            B removedSubframeLength = B(4) + B(longest->packets.back()->getByteLength());
            longest->packets.pop_back();
            longest->psduLength -= previousSubframePadding + removedSubframeLength;
        }
        else
            finalAllocations.erase(longest);
        if (finalAllocations.size() >= 2)
            plannedPpdu = calculatePlannedPpdu();
        plan.durationTrimIterations++;
    }

    if (finalAllocations.size() < 2 || !plannedPpdu ||
            plannedPpdu.parameters.duration > parameters.ppduDurationLimit) {
        plan.failureReason = "no complete HE MU PPDU fits the PHY/TXOP duration limit";
        return plan;
    }

    std::map<int, int> ruTotalNsts;
    std::map<int, int> ruUserCount;
    for (const auto& selectedAllocation : finalAllocations) {
        ruTotalNsts[selectedAllocation.allocation.ru.index] += selectedAllocation.allocation.numberOfSpatialStreams;
        ruUserCount[selectedAllocation.allocation.ru.index]++;
    }
    std::map<int, int> ruStreamStartIndex;
    for (auto& selectedAllocation : finalAllocations) {
        int ruIdx = selectedAllocation.allocation.ru.index;
        selectedAllocation.streamStartIndex = ruStreamStartIndex[ruIdx];
        ruStreamStartIndex[ruIdx] += selectedAllocation.allocation.numberOfSpatialStreams;
        selectedAllocation.muMimo = ruUserCount[ruIdx] > 1;
        selectedAllocation.totalNsts = ruTotalNsts[ruIdx];
    }

    plan.allocations = finalAllocations;
    plan.ppdu = plannedPpdu;
    plan.valid = true;
    return plan;
}

} // namespace ieee80211
} // namespace inet
