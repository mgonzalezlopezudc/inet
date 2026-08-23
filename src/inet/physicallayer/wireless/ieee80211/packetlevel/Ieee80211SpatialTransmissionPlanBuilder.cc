//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211SpatialTransmissionPlanBuilder.h"

#include <cmath>
#include <set>

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtSpaceTimeCodeBuilder.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduLayout.h"

namespace inet {
namespace physicallayer {

namespace {

std::vector<int> resolveAntennaIndices(int numberOfTransmitAntennas, int numberOfSpaceTimeStreams,
    const std::vector<int>& requestedIndices)
{
    if (numberOfTransmitAntennas <= 0)
        throw cRuntimeError("HT spatial plan requires a positive physical transmit antenna count, got %d",
            numberOfTransmitAntennas);
    if (numberOfSpaceTimeStreams <= 0)
        throw cRuntimeError("HT spatial plan requires a positive NSTS, got %d", numberOfSpaceTimeStreams);

    std::vector<int> indices = requestedIndices;
    if (indices.empty()) {
        indices.reserve(numberOfSpaceTimeStreams);
        for (int stream = 0; stream < numberOfSpaceTimeStreams; stream++)
            indices.push_back(stream);
    }
    if ((int)indices.size() != numberOfSpaceTimeStreams)
        throw cRuntimeError("HT spatial plan antenna index count %zu does not match NSTS %d",
            indices.size(), numberOfSpaceTimeStreams);

    std::set<int> uniqueIndices;
    for (int index : indices) {
        if (index < 0 || index >= numberOfTransmitAntennas)
            throw cRuntimeError("HT spatial plan transmit antenna index %d is outside [0,%d)",
                index, numberOfTransmitAntennas);
        if (!uniqueIndices.insert(index).second)
            throw cRuntimeError("HT spatial plan transmit antenna index %d occurs more than once", index);
    }
    return indices;
}

ComplexMatrix makeDirectMapping(int numberOfTransmitAntennas, const std::vector<int>& antennaIndices)
{
    ComplexMatrix mapping(numberOfTransmitAntennas, antennaIndices.size());
    for (int stream = 0; stream < (int)antennaIndices.size(); stream++)
        mapping.get(antennaIndices[stream], stream) = 1;
    return mapping;
}

std::vector<simtime_t> getCyclicShiftDelays(int numberOfSpaceTimeStreams)
{
    // IEEE Std 802.11-2024 Table 19-10 (standards corpus chunk 08103).
    switch (numberOfSpaceTimeStreams) {
        case 1: return {SimTime(0, SIMTIME_NS)};
        case 2: return {SimTime(0, SIMTIME_NS), SimTime(-400, SIMTIME_NS)};
        case 3: return {SimTime(0, SIMTIME_NS), SimTime(-400, SIMTIME_NS), SimTime(-200, SIMTIME_NS)};
        case 4: return {SimTime(0, SIMTIME_NS), SimTime(-400, SIMTIME_NS), SimTime(-200, SIMTIME_NS), SimTime(-600, SIMTIME_NS)};
        default:
            throw cRuntimeError("HT spatial plan does not support NSTS %d for cyclic shifts", numberOfSpaceTimeStreams);
    }
}

SpatialTransmissionPlan::Segment makeSegment(simtime_t start, simtime_t end,
    int numberOfTransmitAntennas, int numberOfSpatialStreams, int numberOfSpaceTimeStreams,
    const std::vector<int>& antennaIndices, const std::vector<simtime_t>& delays,
    const std::shared_ptr<const SpaceTimeCodeDescriptor>& descriptor = nullptr,
    simtime_t spaceTimeCodeSlotDuration = SIMTIME_ZERO)
{
    if (numberOfSpaceTimeStreams == 1)
        return SpatialTransmissionPlan::Segment(start, end, numberOfSpatialStreams, numberOfSpaceTimeStreams,
            makeDirectMapping(numberOfTransmitAntennas, {antennaIndices.front()}), {1.0});
    const double fraction = 1.0 / numberOfSpatialStreams;
    return SpatialTransmissionPlan::Segment(start, end, numberOfSpatialStreams, numberOfSpaceTimeStreams,
        makeDirectMapping(numberOfTransmitAntennas, antennaIndices),
        std::vector<double>(numberOfSpatialStreams, fraction), delays, descriptor,
        spaceTimeCodeSlotDuration);
}

} // namespace

std::shared_ptr<const SpatialTransmissionPlan> Ieee80211SpatialTransmissionPlanBuilder::build(
    const Ieee80211HtPpduDescription& description,
    simtime_t totalPpduDuration,
    int numberOfTransmitAntennas,
    const std::vector<int>& orderedTransmitAntennaIndices)
{
    if (!std::isfinite(totalPpduDuration.dbl()) || totalPpduDuration <= SIMTIME_ZERO)
        throw cRuntimeError("HT spatial plan PPDU duration must be finite and positive, got %s",
            totalPpduDuration.str().c_str());

    // This is an explicit local support boundary, not a second HT-SIG
    // validator.  Valid canonical forms outside it must remain distinguishable
    // from malformed fields and fail as locally unsupported here.
    if (description.getPreambleFormat() != Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)
        throw cRuntimeError("HT spatial plan locally supports mixed-format HT only");
    if (description.getFecCoding())
        throw cRuntimeError("HT spatial plan locally supports BCC only; LDPC is unsupported");
    if (description.getNumberOfExtensionSpatialStreams() != 0)
        throw cRuntimeError("HT spatial plan locally supports NESS=0 only");
    if (description.getMcs() > 31)
        throw cRuntimeError("HT spatial plan locally supports direct EQM MCS 0..31 only, got MCS %u",
            description.getMcs());
    if (description.getStbc() == 0 && description.getNss() != description.getNsts())
        throw cRuntimeError("HT spatial plan local non-STBC support requires NSS=NSTS, got NSS=%d NSTS=%d",
            description.getNss(), description.getNsts());
    if (description.getStbc() != 0 &&
        (description.getNss() != 1 || description.getNsts() != 2 || description.getStbc() != 1))
        throw cRuntimeError("HT spatial plan locally supports only the NSS=1, NSTS=2 Alamouti layout");

    const Hz expectedBandwidth = description.getCbw() ? MHz(40) : MHz(20);
    if (description.getBandwidth() != expectedBandwidth)
        throw cRuntimeError("HT spatial plan HT-SIG channel width does not match the PPDU context");
    const Ieee80211HtPpduLayout layout(description, totalPpduDuration);

    const auto antennaIndices = resolveAntennaIndices(numberOfTransmitAntennas,
        description.getNsts(), orderedTransmitAntennaIndices);
    const auto delays = getCyclicShiftDelays(description.getNsts());
    const std::shared_ptr<const SpaceTimeCodeDescriptor> descriptor = description.getStbc() == 0 ? nullptr :
        std::make_shared<const SpaceTimeCodeDescriptor>(Ieee80211HtSpaceTimeCodeBuilder::build(
            description.getNss(), description.getNsts(), description.getStbc()));
    const ComplexMatrix legacyMapping = makeDirectMapping(numberOfTransmitAntennas, {antennaIndices.front()});
    const simtime_t robustMixedPreambleEnd = layout.getRobustMixedPreambleEnd();
    const simtime_t htStfEnd = layout.getHtShortTrainingEnd();

    std::vector<SpatialTransmissionPlan::Segment> segments;
    segments.reserve(3 + description.getNumberOfDataLongTrainingFields());
    // L-STF (8 us), L-LTF (8 us), L-SIG (4 us), and HT-SIG (8 us).
    segments.emplace_back(SimTime(0, SIMTIME_US), robustMixedPreambleEnd,
        1, 1, legacyMapping, std::vector<double>{1.0});
    // HT-STF starts after the robust mixed-format portion.  CSD applies from
    // this segment through each HT-LTF and the data field. Training fields
    // directly exercise all NSTS; they are not Alamouti-coded data symbols.
    segments.push_back(makeSegment(robustMixedPreambleEnd, htStfEnd, numberOfTransmitAntennas,
        description.getNsts(), description.getNsts(), antennaIndices, delays));

    for (int ltf = 0; ltf < description.getNumberOfDataLongTrainingFields(); ltf++) {
        segments.push_back(makeSegment(layout.getHtLongTrainingFieldStart(ltf),
            layout.getHtLongTrainingFieldEnd(ltf), numberOfTransmitAntennas,
            description.getNsts(), description.getNsts(), antennaIndices, delays));
    }
    segments.push_back(makeSegment(layout.getDataStart(), layout.getDataEnd(), numberOfTransmitAntennas,
        description.getNss(), description.getNsts(), antennaIndices, delays, descriptor,
        descriptor == nullptr ? SIMTIME_ZERO : layout.getDataSymbolDuration()));

    auto plan = std::make_shared<const SpatialTransmissionPlan>(numberOfTransmitAntennas, segments);
    plan->validateCompleteCoverage(totalPpduDuration);
    return plan;
}

} // namespace physicallayer
} // namespace inet
