//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfTransmissionPreparationService.h"

#include <set>

#include "inet/common/packet/Packet.h"

namespace inet {
namespace ieee80211 {

void HcfTransmissionPreparationService::validateRequest(const Request& request)
{
    if (request.packet == nullptr)
        throw cRuntimeError("Cannot prepare a null HCF transmission packet");
    if (request.header == nullptr)
        throw cRuntimeError("Cannot prepare an HCF transmission without a MAC header");
    if (request.ifs < SIMTIME_ZERO)
        throw cRuntimeError("Cannot prepare an HCF transmission with a negative IFS");
    if (!request.container && request.header !=
            request.packet->peekAtFront<Ieee80211MacHeader>())
        throw cRuntimeError("The HCF transmission header is not the packet's front MAC header");
}

void HcfTransmissionPreparationService::validateAggregatePlan(const Request& request,
        const AggregatePlan& aggregatePlan)
{
    if (!aggregatePlan.materialize && !aggregatePlan.members.empty())
        throw cRuntimeError("A non-materialized HCF transmission cannot have aggregate members");
    if (aggregatePlan.materialize && aggregatePlan.members.empty())
        throw cRuntimeError("A materialized HCF transmission must have aggregate members");
    if (request.container && aggregatePlan.materialize)
        throw cRuntimeError("An HCF container transmission cannot be aggregated again");
    if (aggregatePlan.implicitBlockAck && !aggregatePlan.materialize)
        throw cRuntimeError("HT implicit BlockAck requires a materialized aggregate");
    std::set<const Packet *> uniqueMembers;
    unsigned int sourceCount = 0;
    for (auto member : aggregatePlan.members) {
        if (member == nullptr)
            throw cRuntimeError("An HCF aggregate plan contains a null member");
        if (!uniqueMembers.insert(member).second)
            throw cRuntimeError("An HCF aggregate plan contains a duplicate member");
        if (member == request.packet)
            sourceCount++;
    }
    if (aggregatePlan.materialize && sourceCount != 1)
        throw cRuntimeError("An HCF aggregate plan must contain its source packet exactly once");
}

void HcfTransmissionPreparationService::validateProtectionPlan(const Request& request,
        const ProtectionPlan& protectionPlan)
{
    if (protectionPlan.updateDuration && protectionPlan.duration < SIMTIME_ZERO)
        throw cRuntimeError("HCF protection produced a negative Duration field");
    if ((request.durationFinalized ||
            (request.protectionMechanism == ProtectionMechanism::SINGLE &&
             request.durationExemptForSingleProtection)) &&
            protectionPlan.updateDuration)
        throw cRuntimeError("HCF protection attempted to overwrite a finalized or exempt Duration field");
}

HcfTransmissionPreparationService::PreparedTransmission
HcfTransmissionPreparationService::prepare(const Request& request,
        IActions& actions) const
{
    validateRequest(request);

    // IEEE Std 802.11-2024, 10.3.2.11 and 10.25: the existing ACK and
    // BlockAck authorities select QoS Ack Policy before aggregate formation.
    const bool implicitBlockAck = !request.container &&
            actions.isHtImplicitBlockAckEligible(request);
    actions.applySourceRetryState(request.packet);
    Request stagedRequest = request;
    if (!request.container)
        stagedRequest.header = request.packet->peekAtFront<Ieee80211MacHeader>();
    const auto ackPolicy = actions.selectAckPolicy(stagedRequest, implicitBlockAck);
    const auto mode = actions.selectMode(stagedRequest);
    if (mode == nullptr)
        throw cRuntimeError("HCF rate selection returned no PHY mode");
    actions.validateMode(stagedRequest, mode);

    auto aggregatePlan = actions.planAggregation(stagedRequest, mode, ackPolicy,
            implicitBlockAck);
    if (aggregatePlan.implicitBlockAck != implicitBlockAck)
        throw cRuntimeError("HCF aggregation plan changed the implicit BlockAck decision");
    validateAggregatePlan(stagedRequest, aggregatePlan);
    actions.validateAggregation(stagedRequest, mode, aggregatePlan);

    // IEEE Std 802.11-2024, 10.3.1, 10.3.2.6, 10.13, 10.23.2.8 and
    // the PHY-family PPDU-limit tables: reject an invalid protection request
    // after baseline retry/candidate planning mutations but before temporary
    // aggregate allocation. The authoritative duration calculation follows
    // setMode() because it consumes the selected-mode tag.
    actions.validateProtection(stagedRequest, mode, aggregatePlan);

    auto preparedHeader = actions.applyAckPolicy(request.packet, stagedRequest.header,
            ackPolicy, aggregatePlan);
    if (preparedHeader == nullptr)
        throw cRuntimeError("HCF Ack Policy preparation returned no MAC header");
    preparedHeader = actions.applyModePreparation(request.packet, preparedHeader,
            mode);
    if (preparedHeader == nullptr)
        throw cRuntimeError("HCF mode preparation returned no MAC header");
    actions.applyAggregateMemberState(request.packet, aggregatePlan);

    Packet *transmittedPacket = request.packet;
    bool temporaryPacketOwned = false;
    bool sourceModePropagated = false;
    if (aggregatePlan.materialize) {
        transmittedPacket = actions.materializeAggregate(request.packet,
                aggregatePlan);
        if (transmittedPacket == nullptr || transmittedPacket == request.packet)
            throw cRuntimeError("HCF aggregate materialization did not return a temporary packet");
        temporaryPacketOwned = true;
    }

    try {
        if (!request.container) {
            actions.setMode(transmittedPacket, preparedHeader, mode);
            if (aggregatePlan.implicitBlockAck) {
                actions.setSourceMode(request.packet, preparedHeader, mode);
                sourceModePropagated = true;
            }
            auto protectionPlan = actions.computeProtection(stagedRequest, mode,
                    aggregatePlan);
            validateProtectionPlan(request, protectionPlan);
            actions.recordSelectedMode(transmittedPacket, mode);
            actions.observeSelectedRate(transmittedPacket, mode);
            if (protectionPlan.updateDuration)
                actions.applyDuration(request.packet, preparedHeader,
                        protectionPlan.duration);
        }
    }
    catch (...) {
        if (temporaryPacketOwned)
            actions.deleteTemporaryPacket(transmittedPacket);
        throw;
    }

    PreparedTransmission result;
    result.sourcePacket = request.packet;
    result.transmittedPacket = transmittedPacket;
    result.header = preparedHeader;
    result.mode = mode;
    result.ifs = request.ifs;
    result.temporaryPacketOwned = temporaryPacketOwned;
    result.sourceModePropagated = sourceModePropagated;
    return result;
}

void HcfTransmissionPreparationService::commit(PreparedTransmission& transmission,
        IActions& actions) const
{
    if (transmission.terminalState != TerminalState::READY)
        throw cRuntimeError("HCF prepared transmission already reached a terminal state");
    if (transmission.sourcePacket == nullptr || transmission.transmittedPacket == nullptr ||
            transmission.header == nullptr || transmission.mode == nullptr)
        throw cRuntimeError("Incomplete HCF prepared transmission");
    try {
        actions.transmitBorrowed(transmission.transmittedPacket,
                transmission.header, transmission.ifs);
    }
    catch (...) {
        if (transmission.temporaryPacketOwned) {
            actions.deleteTemporaryPacket(transmission.transmittedPacket);
            transmission.temporaryPacketOwned = false;
        }
        transmission.terminalState = TerminalState::HANDOFF_FAILED;
        throw;
    }
    if (transmission.temporaryPacketOwned) {
        actions.deleteTemporaryPacket(transmission.transmittedPacket);
        transmission.temporaryPacketOwned = false;
    }
    transmission.terminalState = TerminalState::COMMITTED;
}

void HcfTransmissionPreparationService::discard(PreparedTransmission& transmission,
        IActions& actions) const
{
    if (transmission.terminalState != TerminalState::READY)
        throw cRuntimeError("HCF prepared transmission already reached a terminal state");
    if (transmission.temporaryPacketOwned) {
        actions.deleteTemporaryPacket(transmission.transmittedPacket);
        transmission.temporaryPacketOwned = false;
    }
    transmission.terminalState = TerminalState::DISCARDED;
}

} // namespace ieee80211
} // namespace inet
