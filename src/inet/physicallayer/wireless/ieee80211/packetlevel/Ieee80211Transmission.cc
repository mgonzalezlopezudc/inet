//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduLayout.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyModeResolver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

namespace inet {

namespace physicallayer {

namespace {

void requireSegmentLayout(const SpatialTransmissionPlan::Segment& segment,
    int numberOfSpatialStreams, int numberOfSpaceTimeStreams, bool spaceTimeCoded,
    const char *field)
{
    if (segment.getNumberOfSpatialStreams() != numberOfSpatialStreams ||
        segment.getNumberOfSpaceTimeStreams() != numberOfSpaceTimeStreams ||
        segment.hasSpaceTimeCode() != spaceTimeCoded)
        throw cRuntimeError("HT spatial plan %s layout contradicts the canonical PPDU description", field);
}

bool equalSignalFields(const Ieee80211HtSignalField& left,
    const Ieee80211HtSignalField& right)
{
    return left.mcs == right.mcs && left.cbw == right.cbw &&
        left.length == right.length && left.smoothing == right.smoothing &&
        left.notSounding == right.notSounding && left.reserved == right.reserved &&
        left.aggregation == right.aggregation && left.stbc == right.stbc &&
        left.fecCoding == right.fecCoding && left.shortGi == right.shortGi &&
        left.numberOfExtensionSpatialStreams == right.numberOfExtensionSpatialStreams &&
        left.crc == right.crc && left.tail == right.tail;
}

} // namespace

void Ieee80211Transmission::validateSpatialMetadata(const IIeee80211Mode *mode,
    const Ieee80211Channel *channel, simtime_t duration,
    const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
    const std::shared_ptr<const Ieee80211HtPpduDescription>& htPpduDescription)
{
    if (mode == nullptr || channel == nullptr)
        throw cRuntimeError("IEEE 802.11 transmission requires non-null mode and channel metadata");
    if (spatialTransmissionPlan == nullptr)
        throw cRuntimeError("IEEE 802.11 transmission requires an immutable spatial transmission plan");
    spatialTransmissionPlan->validateCompleteCoverage(duration);

    const auto htMode = dynamic_cast<const Ieee80211HtMode *>(mode);
    if (htMode == nullptr) {
        if (htPpduDescription != nullptr)
            throw cRuntimeError("An HT PPDU description can only be attached to an HT transmission");
        for (const auto& segment : spatialTransmissionPlan->getSegments())
            requireSegmentLayout(segment, 1, 1, false, "legacy");
        return;
    }
    if (htPpduDescription == nullptr)
        throw cRuntimeError("An HT transmission requires a canonical HT PPDU description");

    const auto& description = *htPpduDescription;
    const auto dataMode = htMode->getDataMode();
    const auto headerMode = htMode->getHeaderMode();
    const unsigned int modeMcs = dataMode->getModulationAndCodingScheme()->getMcsIndex();
    const bool modeShortGi = dataMode->getGuardIntervalType() ==
        Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT;
    if (description.getMcs() != modeMcs ||
        description.getBandwidth() != dataMode->getBandwidth() ||
        description.getCbw() != (dataMode->getBandwidth() == MHz(40)) ||
        description.getShortGi() != modeShortGi ||
        description.getStbc() != headerMode->getSTBC() ||
        description.getNss() != dataMode->getNumberOfSpatialStreams())
        throw cRuntimeError("Canonical HT-SIG description contradicts the selected HT mode");
    if (description.getBandMode() != htMode->getCenterFrequencyMode() ||
        description.getChannelNumber() != channel->getChannelNumber() ||
        description.getCenterFrequency() != channel->getCenterFrequency())
        throw cRuntimeError("Canonical HT PPDU context contradicts the selected channel");

    const Ieee80211HtPpduLayout layout(description, duration);
    const simtime_t robustMixedPreambleEnd = layout.getRobustMixedPreambleEnd();
    const simtime_t htStfEnd = layout.getHtShortTrainingEnd();
    const auto& segments = spatialTransmissionPlan->getSegments();
    const size_t expectedSegmentCount = 3 + description.getNumberOfDataLongTrainingFields();
    if (segments.size() != expectedSegmentCount)
        throw cRuntimeError("HT spatial plan has %zu segments instead of the canonical %zu",
            segments.size(), expectedSegmentCount);
    if (segments[0].getStartOffset() != SIMTIME_ZERO ||
        segments[0].getEndOffset() != robustMixedPreambleEnd)
        throw cRuntimeError("HT spatial plan robust mixed-format boundary is not [0,28 us)");
    requireSegmentLayout(segments[0], 1, 1, false, "robust preamble and HT-SIG");
    if (segments[1].getStartOffset() != robustMixedPreambleEnd ||
        segments[1].getEndOffset() != htStfEnd)
        throw cRuntimeError("HT spatial plan HT-STF boundary is not [28 us,32 us)");
    requireSegmentLayout(segments[1], description.getNsts(), description.getNsts(),
        false, "HT-STF");
    for (int ltf = 0; ltf < description.getNumberOfDataLongTrainingFields(); ltf++) {
        const simtime_t expectedStart = layout.getHtLongTrainingFieldStart(ltf);
        const simtime_t expectedEnd = layout.getHtLongTrainingFieldEnd(ltf);
        const auto& segment = segments[2 + ltf];
        if (segment.getStartOffset() != expectedStart || segment.getEndOffset() != expectedEnd)
            throw cRuntimeError("HT spatial plan HT-LTF %d has a noncanonical boundary", ltf);
        requireSegmentLayout(segment, description.getNsts(), description.getNsts(),
            false, "HT-LTF");
    }
    const auto& dataSegment = segments.back();
    if (dataSegment.getStartOffset() != layout.getDataStart() ||
        dataSegment.getEndOffset() != layout.getDataEnd())
        throw cRuntimeError("HT spatial plan data field has a noncanonical boundary");
    requireSegmentLayout(dataSegment, description.getNss(), description.getNsts(),
        description.getStbc() != 0, "data");
    if (description.getStbc() != 0) {
        const auto& descriptor = *dataSegment.getSpaceTimeCodeDescriptor();
        if (descriptor.getNumberOfSpatialStreams() != description.getNss() ||
            descriptor.getNumberOfSpaceTimeStreams() != description.getNsts() ||
            dataSegment.getSpaceTimeCodeSlotDuration() != layout.getDataSymbolDuration())
            throw cRuntimeError("HT data space-time descriptor contradicts HT-SIG dimensions or timing");
    }
}

Ieee80211Transmission::Ieee80211Transmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, const ITransmissionPacketModel *packetModel, const ITransmissionBitModel *bitModel, const ITransmissionSymbolModel *symbolModel, const ITransmissionSampleModel *sampleModel, const ITransmissionAnalogModel *analogModel, const IIeee80211Mode *mode, const Ieee80211Channel *channel, std::shared_ptr<const SpatialTransmissionPlan> spatialTransmissionPlan, std::shared_ptr<const Ieee80211HtPpduDescription> htPpduDescription) :
    TransmissionBase(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, packetModel, bitModel, symbolModel, sampleModel, analogModel),
    mode(mode),
    channel(channel),
    spatialTransmissionPlan(spatialTransmissionPlan),
    htPpduDescription(htPpduDescription)
{
    validateSpatialMetadata(mode, channel, endTime - startTime,
        this->spatialTransmissionPlan, this->htPpduDescription);
    if (transmitter != nullptr && this->spatialTransmissionPlan->getNumberOfTransmitAntennas() !=
        transmitter->getAntenna()->getNumAntennas())
        throw cRuntimeError("Spatial transmission plan antenna dimension contradicts the transmitter antenna count");
    if (this->htPpduDescription != nullptr) {
        if (packet == nullptr)
            throw cRuntimeError("HT transmission requires a packet carrying the authoritative HT-SIG");
        const auto header = dynamicPtrCast<const Ieee80211HtPhyHeader>(
            Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet));
        if (header == nullptr)
            throw cRuntimeError("HT transmission packet does not carry an HT PHY header");
        const auto htMode = check_and_cast<const Ieee80211HtMode *>(mode);
        Ieee80211HtPpduContext context;
        context.bandMode = htMode->getCenterFrequencyMode();
        context.preambleFormat = htMode->getPreambleMode()->getPreambleFormat();
        context.channelNumber = channel->getChannelNumber();
        context.centerFrequency = channel->getCenterFrequency();
        context.bandwidth = htMode->getDataMode()->getBandwidth();
        const auto resolution = Ieee80211PhyModeResolver::resolve(*header, context);
        if (!resolution.isSuccess() || !equalSignalFields(
            resolution.description->getSignalField(),
            this->htPpduDescription->getSignalField()))
            throw cRuntimeError("Packet HT-SIG contradicts the attached canonical HT PPDU description");
    }
}

std::ostream& Ieee80211Transmission::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Transmission";
    if (level <= PRINT_LEVEL_DETAIL)
        stream << EV_FIELD(mode, printFieldToString(mode, level + 1, evFlags))
               << EV_FIELD(channel, printFieldToString(channel, level + 1, evFlags));
    return TransmissionBase::printToStream(stream, level);
}

} // namespace physicallayer

} // namespace inet
