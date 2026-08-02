//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.h"

#include <map>
#include <numeric>
#include <tuple>

namespace inet {
namespace ieee80211 {

Define_Module(HeUlSchedulerBacklogBased);

namespace {

using AllocationTree = physicallayer::Ieee80211HeRuAllocationTree;

struct PartialSchedule
{
    uint32_t stationMask = 0;
    int randomAccessRus = 0;
    std::vector<IIeee80211HeUlScheduler::RuAllocation> allocations;
    std::vector<int64_t> delivered;
    int64_t totalDelivered = 0;
    int64_t unusedCapacity = 0;
};

using PartialKey = std::pair<uint32_t, int>;

std::vector<std::tuple<int, int, uint16_t>> getSignature(const PartialSchedule& schedule)
{
    std::vector<std::tuple<int, int, uint16_t>> result;
    for (const auto& allocation : schedule.allocations)
        result.emplace_back(allocation.ru.toneOffset, allocation.ru.toneSize,
                allocation.associationId);
    std::sort(result.begin(), result.end());
    return result;
}

bool isBetter(const PartialSchedule& left, const PartialSchedule& right)
{
    if (left.totalDelivered != right.totalDelivered)
        return left.totalDelivered > right.totalDelivered;
    if (left.delivered != right.delivered)
        return std::lexicographical_compare(right.delivered.begin(), right.delivered.end(),
                left.delivered.begin(), left.delivered.end());
    if (left.unusedCapacity != right.unusedCapacity)
        return left.unusedCapacity < right.unusedCapacity;
    return getSignature(left) < getSignature(right);
}

void retain(std::map<PartialKey, PartialSchedule>& states, PartialSchedule candidate)
{
    auto key = std::make_pair(candidate.stationMask, candidate.randomAccessRus);
    auto it = states.find(key);
    if (it == states.end() || isBetter(candidate, it->second))
        states[key] = std::move(candidate);
}

void collectLeaves(const AllocationTree& tree, std::vector<physicallayer::Ieee80211HeRu>& leaves)
{
    if (tree.children.empty())
        leaves.push_back(tree.ru);
    else
        for (const auto& child : tree.children)
            collectLeaves(child, leaves);
}

struct TreeNodeInfo
{
    physicallayer::Ieee80211HeRu ru;
    int parent = -1;
    std::vector<std::pair<int, int>> leaves;
};

int collectTreeNodeInfo(const AllocationTree& tree, int parent,
        std::vector<TreeNodeInfo>& nodes)
{
    int index = nodes.size();
    nodes.push_back({tree.ru, parent, {}});
    if (tree.children.empty())
        nodes[index].leaves.emplace_back(tree.ru.toneSize, tree.ru.toneOffset);
    else
        for (const auto& child : tree.children) {
            int childIndex = collectTreeNodeInfo(child, index, nodes);
            nodes[index].leaves.insert(nodes[index].leaves.end(),
                    nodes[childIndex].leaves.begin(), nodes[childIndex].leaves.end());
        }
    return index;
}

} // namespace

void HeUlSchedulerBacklogBased::initialize(int stage)
{
    HeUlSchedulerBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        maxOptimizedStations = par("maxOptimizedStations");
        if (maxOptimizedStations <= 0 || maxOptimizedStations > 8)
            throw cRuntimeError("maxOptimizedStations must be in the range 1..8");
        WATCH(maxOptimizedStations);
    }
}

IIeee80211HeUlScheduler::Schedule HeUlSchedulerBacklogBased::schedule(
        const ScheduleContext& context)
{
    Schedule result;
    result.commonDuration = computeCommonDuration(context, {});
    static const std::set<int> mandatoryHeNonApRuToneSizes =
            {26, 52, 106, 242};
    std::vector<CandidateInfo> candidates;
    for (const auto& candidate : context.candidates) {
        const bool typed = candidate.hasTypedBacklogEstimates;
        const bool capabilitiesValid = !typed ||
                (candidate.hasNegotiatedHeCapabilities &&
                 candidate.negotiatedHeCapabilities.localRxPeerTx.valid &&
                 candidate.negotiatedHeCapabilities.localRxPeerTx.ofdma &&
                 candidate.negotiatedHeCapabilities.localRxPeerTx.supportedChannelWidths.count(
                         context.channelBandwidth) != 0 &&
                 std::includes(candidate.negotiatedHeCapabilities.localRxPeerTx.
                         supportedRuToneSizes.begin(),
                         candidate.negotiatedHeCapabilities.localRxPeerTx.
                                 supportedRuToneSizes.end(),
                         mandatoryHeNonApRuToneSizes.begin(),
                         mandatoryHeNonApRuToneSizes.end()));
        const bool fresh = !typed || candidate.hasFreshReport;
        const auto selectedBacklogBytes = candidate.getSelectedBacklogBytes();
        const bool eligible = !candidate.ulMuDisabled && fresh &&
                (selectedBacklogBytes > 0 || candidate.isUnknownProbe()) &&
                capabilitiesValid;
        EV_DEBUG << "HE UL capacity candidate: AID=" << candidate.associationId
                 << ", AC=" << candidate.selectedAccessCategory
                 << ", typed=" << typed
                 << ", fresh=" << fresh
                 << ", backlogKind=" << static_cast<int>(
                         candidate.backlogEstimates[candidate.selectedAccessCategory].kind)
                 << ", lowerBound=" << selectedBacklogBytes
                 << ", unknownProbe=" << candidate.isUnknownProbe()
                 << ", ulMuDisabled=" << candidate.ulMuDisabled
                 << ", capabilitiesValid=" << capabilitiesValid
                 << ", anchor=" << candidate.anchor
                 << ", eligible=" << eligible << "\n";
        if (eligible)
            candidates.push_back(candidate);
    }
    std::sort(candidates.begin(), candidates.end(), [] (const auto& left, const auto& right) {
        if (left.anchor != right.anchor)
            return left.anchor;
        if (left.isUnknownProbe() != right.isUnknownProbe())
            return !left.isUnknownProbe();
        if (left.lastService != right.lastService)
            return left.lastService < right.lastService;
        if (left.getSelectedBacklogBytes() != right.getSelectedBacklogBytes())
            return left.getSelectedBacklogBytes() > right.getSelectedBacklogBytes();
        if (left.associationId != right.associationId)
            return left.associationId < right.associationId;
        return left.staAddress < right.staAddress;
    });

    const int maximumRus = physicallayer::getHeMaxRuCount(context.channelBandwidth);
    const int randomAccessRus = computeRandomAccessRuCount(context, maximumRus);
    int prefixSize = std::min({static_cast<int>(candidates.size()), maxMuStations,
            std::max(0, maximumRus - randomAccessRus)});
    const auto tree = physicallayer::getHeRuAllocationTree(
            context.channelCenterFrequency, context.channelBandwidth);
    const int targetRssiDbm = computeTargetRssiDbm(context);

    auto makeAllocation = [&] (int stationIndex, const physicallayer::Ieee80211HeRu& ru, int requestedNss = 0) {
        RuAllocation allocation;
        const auto& candidate = candidates[stationIndex];
        allocation.staAddress = candidate.staAddress;
        allocation.associationId = candidate.associationId;
        allocation.tid = candidate.selectedTid;
        allocation.accessCategory = candidate.selectedAccessCategory;
        allocation.ru = ru;

        int maxSupportedNss = 1;
        if (candidate.hasNegotiatedHeCapabilities &&
                candidate.negotiatedHeCapabilities.localRxPeerTx.valid) {
            maxSupportedNss = getMaxNss(candidate.negotiatedHeCapabilities.localRxPeerTx.mcsNss);
            maxSupportedNss = std::min(maxSupportedNss, 8);
        }
        if (maxSupportedNss < 1)
            maxSupportedNss = 1;

        // UL OFDMA assigns users to separate RUs, so each non-MU user may use
        // all of its negotiated spatial streams within its assigned RU. Keep
        // the explicit request for the full-bandwidth UL MU-MIMO path, but
        // constrain both paths by the coding rules enforced by the PHY.
        maxSupportedNss = std::min(maxSupportedNss,
                candidate.coding == physicallayer::HE_CODING_BCC ? 4 : 8);
        int nss = requestedNss > 0 ? std::min(requestedNss, maxSupportedNss) : maxSupportedNss;
        allocation.numberOfSpatialStreams = nss;
        allocation.mcs = selectMcs(context, candidate, ru, nss);
        allocation.coding = candidate.coding;
        allocation.targetRssiDbm = targetRssiDbm;
        allocation.estimatedDuration = result.commonDuration;

        if (candidate.hasTypedBacklogEstimates) {
            const auto& capabilities =
                    candidate.negotiatedHeCapabilities.localRxPeerTx;
            int nssIndex = std::min(std::max(0, nss - 1), 7);
            if (capabilities.supportedRuToneSizes.count(ru.toneSize) == 0 ||
                    capabilities.mcsNss.maxMcsPerNss[nssIndex] < 0 ||
                    (allocation.coding == physicallayer::HE_CODING_LDPC &&
                     !candidate.negotiatedHeCapabilities.mutual.ldpc))
                return std::make_pair(allocation, INT64_C(-1));
            allocation.mcs = std::min(allocation.mcs,
                    capabilities.mcsNss.maxMcsPerNss[nssIndex]);
            // IEEE 802.11 HE BCC is limited to MCS 0-9 and RUs no larger
            // than 242 tones; larger-RU/MCS edges require negotiated LDPC.
            if (allocation.coding == physicallayer::HE_CODING_BCC &&
                    (allocation.mcs > 9 || ru.toneSize > 242 || allocation.numberOfSpatialStreams > 4))
                return std::make_pair(allocation, INT64_C(-1));
        }

        auto boundary = context.finalizedBoundary;
        if (boundary.ulLength == 0) {
            physicallayer::Ieee80211HeUserPhyParameters user;
            user.ru = ru;
            user.mcs = allocation.mcs;
            user.coding = allocation.coding;
            user.psduLength = B(1);
            physicallayer::Ieee80211HeTriggerResponseFinalizationRequest request;
            request.users = {user};
            request.centerFrequency = context.channelCenterFrequency;
            request.channelBandwidth = context.channelBandwidth;
            request.durationBudget = result.commonDuration;
            auto finalized = physicallayer::finalizeHeTriggerResponse(request);
            if (finalized) {
                boundary.channelBandwidth = context.channelBandwidth;
                boundary.ulLength = finalized.ulLength;
                boundary.guardInterval = finalized.parameters.common.guardInterval;
                boundary.ltfType = finalized.parameters.common.ltfType;
                boundary.preFecPaddingFactor = finalized.parameters.common.preFecPaddingFactor;
                boundary.ldpcExtraSymbolSegment = finalized.parameters.common.ldpcExtraSymbol;
                boundary.peDisambiguity = finalized.peDisambiguity;
                boundary.numberOfHeLtfSymbols = finalized.parameters.common.numberOfHeLtfSymbols;
                boundary.packetExtensionDurationUs =
                        finalized.parameters.common.packetExtensionDurationUs;
            }
        }
        auto capacity = physicallayer::getHeTbPsduCapacity(boundary, ru,
                allocation.mcs, allocation.numberOfSpatialStreams, allocation.coding);
        const int64_t psduCapacity = capacity ?
                capacity.maximumPsduLength.get<B>() : 0;
        // Candidate backlog is the queued MAC Packet::getByteLength(), not
        // application payload bytes. The final STA-side construction uses
        // 4 bytes for the A-MPDU delimiter and, for a queued header without
        // BSR/HT Control, another 4 bytes for the inserted HT Control field.
        // Do not subtract the already-counted MAC header and FCS again.
        const int64_t serviceCapacity = std::max<int64_t>(0, psduCapacity -
                getHeTbQueuedPacketOverheadBytes(false));
        allocation.plannedBytes = std::min<int64_t>(
                candidate.getSelectedBacklogBytes(), serviceCapacity);
        return std::make_pair(allocation, serviceCapacity);
    };

    if (context.useUlMuMimoPolicy) {
        std::vector<int> muMimoCandidates;
        for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
            if (candidates[i].negotiatedHeCapabilities.localRxPeerTx.
                    fullBandwidthUlMuMimo)
                muMimoCandidates.push_back(i);
        if (muMimoCandidates.size() >= 2) {
            const auto fullBandwidthRu = physicallayer::getHeEqualRuLayout(
                    context.channelCenterFrequency, context.channelBandwidth, 1).front();
            const int selectedUsers = std::min<int>(muMimoCandidates.size(), maxMuStations);
            int currentStreamStartIndex = 0;
            const int maxTotalStreams = 8;
            for (int user = 0; user < selectedUsers; ++user) {
                const auto& candidate = candidates[muMimoCandidates[user]];
                int maxStaNss = 1;
                if (candidate.hasNegotiatedHeCapabilities &&
                        candidate.negotiatedHeCapabilities.localRxPeerTx.valid) {
                    maxStaNss = getMaxNss(candidate.negotiatedHeCapabilities.localRxPeerTx.mcsNss);
                    maxStaNss = std::min(maxStaNss, 4);
                }
                if (maxStaNss < 1)
                    maxStaNss = 1;
                int allocatedNss = std::min(maxStaNss, maxTotalStreams - currentStreamStartIndex);
                if (allocatedNss < 1)
                    break;
                auto edge = makeAllocation(muMimoCandidates[user], fullBandwidthRu, allocatedNss);
                if (edge.second < 0)
                    continue;
                edge.first.muMimo = (selectedUsers > 1);
                edge.first.streamStartIndex = currentStreamStartIndex;
                currentStreamStartIndex += allocatedNss;
                if (edge.first.plannedBytes > 0) {
                    result.totalPlannedBytes += edge.first.plannedBytes;
                    result.allocations.push_back(edge.first);
                }
            }
            if (result.allocations.size() == 1) {
                result.allocations[0].muMimo = false;
                result.allocations[0].streamStartIndex = 0;
            }
            result.exactOptimization = false;
            result.decisionReason = "UL MU-MIMO policy bypass";
            EV_INFO << "HE UL schedule bypassed OFDMA optimization for "
                    << result.allocations.size() << " full-bandwidth MU-MIMO users"
                    << ", plannedBytes=" << result.totalPlannedBytes << "\n";
            recordSchedule(context, result, result.decisionReason.c_str());
            return result;
        }
    }

    auto exactSearch = [&] (int selectedPrefix) -> std::optional<PartialSchedule> {
        std::function<std::map<PartialKey, PartialSchedule>(const AllocationTree&)> solve;
        solve = [&] (const AllocationTree& node) {
            std::map<PartialKey, PartialSchedule> states;
            PartialSchedule empty;
            empty.delivered.resize(selectedPrefix);
            retain(states, empty);
            for (int station = 0; station < selectedPrefix; ++station) {
                auto edge = makeAllocation(station, node.ru);
                if (edge.second < 0 ||
                        (edge.first.plannedBytes <= 0 &&
                         !candidates[station].isUnknownProbe()))
                    continue;
                PartialSchedule assigned;
                assigned.stationMask = UINT32_C(1) << station;
                assigned.allocations.push_back(edge.first);
                assigned.delivered.resize(selectedPrefix);
                assigned.delivered[station] = edge.first.plannedBytes;
                assigned.totalDelivered = edge.first.plannedBytes;
                assigned.unusedCapacity = edge.second - edge.first.plannedBytes;
                retain(states, std::move(assigned));
            }
            if (node.children.empty()) {
                PartialSchedule ra;
                ra.randomAccessRus = 1;
                ra.delivered.resize(selectedPrefix);
                RuAllocation allocation;
                allocation.randomAccess = true;
                allocation.ru = node.ru;
                allocation.mcs = defaultMcs;
                allocation.coding = physicallayer::HE_CODING_BCC;
                allocation.targetRssiDbm = targetRssiDbm;
                allocation.estimatedDuration = result.commonDuration;
                ra.allocations.push_back(allocation);
                retain(states, std::move(ra));
            }
            else {
                std::map<PartialKey, PartialSchedule> combined;
                PartialSchedule seed;
                seed.delivered.resize(selectedPrefix);
                retain(combined, seed);
                for (const auto& child : node.children) {
                    auto childStates = solve(child);
                    std::map<PartialKey, PartialSchedule> next;
                    for (const auto& leftEntry : combined)
                        for (const auto& rightEntry : childStates) {
                            const auto& left = leftEntry.second;
                            const auto& right = rightEntry.second;
                            if ((left.stationMask & right.stationMask) != 0 ||
                                    left.randomAccessRus + right.randomAccessRus > randomAccessRus)
                                continue;
                            PartialSchedule merged;
                            merged.stationMask = left.stationMask | right.stationMask;
                            merged.randomAccessRus = left.randomAccessRus + right.randomAccessRus;
                            merged.allocations = left.allocations;
                            merged.allocations.insert(merged.allocations.end(),
                                    right.allocations.begin(), right.allocations.end());
                            merged.delivered.resize(selectedPrefix);
                            for (int i = 0; i < selectedPrefix; ++i)
                                merged.delivered[i] = left.delivered[i] + right.delivered[i];
                            merged.totalDelivered = left.totalDelivered + right.totalDelivered;
                            merged.unusedCapacity = left.unusedCapacity + right.unusedCapacity;
                            retain(next, std::move(merged));
                        }
                    combined = std::move(next);
                }
                for (auto& entry : combined)
                    retain(states, std::move(entry.second));
            }
            return states;
        };
        auto states = solve(tree);
        const uint32_t fullMask = selectedPrefix == 0 ? 0 :
                (UINT32_C(1) << selectedPrefix) - 1;
        auto it = states.find({fullMask, randomAccessRus});
        return it == states.end() ? std::nullopt :
                std::optional<PartialSchedule>(std::move(it->second));
    };

    std::optional<PartialSchedule> selected;
    bool exact = prefixSize <= maxOptimizedStations;
    if (exact) {
        while (prefixSize >= 0 && !(selected = exactSearch(prefixSize)))
            --prefixSize;
    }
    else {
        // A conforming HE non-AP STA supports the mandatory 26/52/106/242
        // baseline, so the deterministic fallback begins with canonical
        // 26-tone leaves for the complete fairness prefix and UORA. If a
        // prefix is nevertheless layout-infeasible, remove only its newest
        // tail candidate and retry. Then apply only positive-gain parent
        // promotions; this path makes no global optimality claim.
        std::vector<physicallayer::Ieee80211HeRu> leaves;
        collectLeaves(tree, leaves);
        auto constructFallback = [&] (int selectedPrefix) -> std::optional<PartialSchedule> {
            if (selectedPrefix + randomAccessRus >
                    static_cast<int>(leaves.size()))
                return std::nullopt;
            PartialSchedule fallback;
            fallback.delivered.resize(selectedPrefix);
            for (int i = 0; i < selectedPrefix; ++i) {
                auto edge = makeAllocation(i, leaves[i]);
                if (edge.second < 0 ||
                        (edge.first.plannedBytes <= 0 &&
                         !candidates[i].isUnknownProbe()))
                    return std::nullopt;
                fallback.allocations.push_back(edge.first);
                fallback.delivered[i] = edge.first.plannedBytes;
                fallback.totalDelivered += edge.first.plannedBytes;
                fallback.unusedCapacity += edge.second - edge.first.plannedBytes;
            }
            for (int i = 0; i < randomAccessRus; ++i) {
                RuAllocation allocation;
                allocation.randomAccess = true;
                allocation.ru = leaves[selectedPrefix + i];
                allocation.mcs = defaultMcs;
                allocation.coding = physicallayer::HE_CODING_BCC;
                allocation.targetRssiDbm = targetRssiDbm;
                allocation.estimatedDuration = result.commonDuration;
                fallback.allocations.push_back(allocation);
            }
            return fallback;
        };

        while (prefixSize >= 0 && !(selected = constructFallback(prefixSize)))
            --prefixSize;
        if (selected) {
            auto& fallback = *selected;
            std::vector<TreeNodeInfo> nodes;
            collectTreeNodeInfo(tree, -1, nodes);
            auto findNode = [&] (const physicallayer::Ieee80211HeRu& ru) {
                for (size_t i = 0; i < nodes.size(); ++i)
                    if (nodes[i].ru.toneSize == ru.toneSize &&
                            nodes[i].ru.toneOffset == ru.toneOffset)
                        return static_cast<int>(i);
                return -1;
            };
            for (;;) {
                int bestAllocation = -1;
                RuAllocation bestPromoted;
                int64_t bestGain = 0;
                for (int i = 0; i < prefixSize; ++i) {
                    int nodeIndex = findNode(fallback.allocations[i].ru);
                    if (nodeIndex < 0 || nodes[nodeIndex].parent < 0)
                        continue;
                    const auto& parent = nodes[nodes[nodeIndex].parent];
                    bool siblingOccupied = false;
                    for (size_t j = 0; j < fallback.allocations.size(); ++j) {
                        if (static_cast<int>(j) == i)
                            continue;
                        int otherNode = findNode(fallback.allocations[j].ru);
                        if (otherNode < 0)
                            continue;
                        for (const auto& leaf : nodes[otherNode].leaves)
                            if (std::find(parent.leaves.begin(), parent.leaves.end(), leaf) !=
                                    parent.leaves.end()) {
                                siblingOccupied = true;
                                break;
                            }
                        if (siblingOccupied)
                            break;
                    }
                    if (siblingOccupied)
                        continue;
                    auto promotedEdge = makeAllocation(i, parent.ru);
                    if (promotedEdge.second < 0)
                        continue;
                    auto promoted = promotedEdge.first;
                    int64_t gain = promoted.plannedBytes -
                            fallback.allocations[i].plannedBytes;
                    if (gain > bestGain ||
                            (gain == bestGain && gain > 0 &&
                             (bestAllocation < 0 || i < bestAllocation ||
                              (i == bestAllocation &&
                               std::tie(parent.ru.toneOffset, parent.ru.toneSize) <
                               std::tie(bestPromoted.ru.toneOffset,
                                       bestPromoted.ru.toneSize))))) {
                        bestGain = gain;
                        bestAllocation = i;
                        bestPromoted = promoted;
                    }
                }
                if (bestAllocation < 0 || bestGain <= 0)
                    break;
                fallback.totalDelivered += bestGain;
                fallback.delivered[bestAllocation] = bestPromoted.plannedBytes;
                fallback.allocations[bestAllocation] = bestPromoted;
            }
        }
    }

    if (selected) {
        result.allocations = std::move(selected->allocations);
        result.totalPlannedBytes = selected->totalDelivered;
    }
    result.exactOptimization = exact;
    result.decisionReason = exact ? "capacity-aware exact allocation-tree DP" :
            "capacity-aware deterministic minimum-RU fallback";
    EV_INFO << "HE UL capacity-aware schedule: prefix=" << std::max(0, prefixSize)
            << ", allocated=" << result.allocations.size()
            << ", randomAccessRequested=" << randomAccessRus
            << ", plannedBytes=" << result.totalPlannedBytes
            << ", method=" << (exact ? "exact" : "fallback") << "\n";
    for (const auto& allocation : result.allocations)
        EV_INFO << "HE UL capacity allocation: "
                << (allocation.randomAccess ? "RA" :
                        std::string("AID=") + std::to_string(allocation.associationId))
                << ", RU=" << allocation.ru.toneSize << "@"
                << allocation.ru.toneOffset
                << ", MCS=" << allocation.mcs
                << ", coding=" << (allocation.coding ==
                        physicallayer::HE_CODING_LDPC ? "LDPC" : "BCC")
                << ", plannedBytes=" << allocation.plannedBytes << "\n";
    recordSchedule(context, result, result.decisionReason.c_str());
    return result;
}

} // namespace ieee80211
} // namespace inet
