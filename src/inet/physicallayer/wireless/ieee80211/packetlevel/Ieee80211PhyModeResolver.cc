//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyModeResolver.h"

#include <limits>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtSignalField.h"

namespace inet {
namespace physicallayer {

namespace {

Ieee80211HtSignalField getSignalField(const Ieee80211HtPhyHeader& header)
{
    Ieee80211HtSignalField field;
    field.mcs = header.getMcs();
    field.cbw = header.getCbw();
    field.length = static_cast<uint16_t>(header.getLengthField().get<B>());
    field.smoothing = header.getSmoothing();
    field.notSounding = header.getNotSounding();
    field.reserved = header.getReserved();
    field.aggregation = header.getAggregation();
    field.stbc = header.getStbc();
    field.fecCoding = header.getFecCoding();
    field.shortGi = header.getShortGi();
    field.numberOfExtensionSpatialStreams = header.getNumberOfExtensionSpatialStreams();
    field.crc = header.getCrc();
    field.tail = header.getTail();
    return field;
}

bool isLegalTable19_12Combination(int numberOfSpatialStreams, uint8_t stbc, int numberOfSpaceTimeStreams)
{
    // IEEE Std 802.11-2024 Table 19-12.
    return (numberOfSpatialStreams == 1 && (stbc == 0 || stbc == 1) && numberOfSpaceTimeStreams == 1 + stbc) ||
           (numberOfSpatialStreams == 2 && (stbc == 0 || stbc == 1 || stbc == 2) && numberOfSpaceTimeStreams == 2 + stbc) ||
           (numberOfSpatialStreams == 3 && (stbc == 0 || stbc == 1) && numberOfSpaceTimeStreams == 3 + stbc) ||
           (numberOfSpatialStreams == 4 && stbc == 0 && numberOfSpaceTimeStreams == 4);
}

int getNumberOfDataLongTrainingFields(int numberOfSpaceTimeStreams)
{
    // IEEE Std 802.11-2024 Table 19-13.
    switch (numberOfSpaceTimeStreams) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 4;
        default: return -1;
    }
}

int getNumberOfExtensionLongTrainingFields(uint8_t numberOfExtensionSpatialStreams)
{
    // IEEE Std 802.11-2024 Table 19-14.
    switch (numberOfExtensionSpatialStreams) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        default: return -1;
    }
}

} // namespace

Ieee80211PhyModeResolver::Result Ieee80211PhyModeResolver::resolve(const Ieee80211HtPhyHeader& header, const Ieee80211HtPpduContext& context)
{
    if (header.getChunkLength() != B(6))
        return {Status::FORMAT_VIOLATION, nullptr};

    const auto length = header.getLengthField().get<B>();
    if (length < 0 || length > std::numeric_limits<uint16_t>::max())
        return {Status::FORMAT_VIOLATION, nullptr};
    const auto field = getSignalField(header);
    // These limits are implied by the wire widths. They are checked here as
    // well so manually constructed malformed chunks receive a stable status
    // instead of an exception from the bit packer.
    if (field.mcs > 0x7f || field.stbc > 0x3 || field.numberOfExtensionSpatialStreams > 0x3 || field.tail > 0x3f)
        return {Status::RESERVED_HT_SIG, nullptr};
    if (!verifyIeee80211HtSignalFieldCrc(field))
        return {Status::FORMAT_VIOLATION, nullptr};
    const Hz signaledBandwidth = field.cbw ? MHz(40) : MHz(20);
    if ((context.bandwidth != MHz(20) && context.bandwidth != MHz(40)) ||
        context.bandwidth != signaledBandwidth)
        return {Status::FORMAT_VIOLATION, nullptr};
    if (!field.reserved || field.tail != 0 || field.stbc == 3)
        return {Status::RESERVED_HT_SIG, nullptr};
    // IEEE Std 802.11-2024 Table 19-35: MCS 32 is the optional 40 MHz
    // duplicate-format lowest-rate MCS and is not legal with CBW=0.
    if (field.mcs == 32 && !field.cbw)
        return {Status::RESERVED_HT_SIG, nullptr};

    const int numberOfSpatialStreams = Ieee80211HtModeBase::getNumberOfSpatialStreamsForMcs(field.mcs);
    if (numberOfSpatialStreams < 0)
        return {Status::RESERVED_HT_SIG, nullptr};
    const int numberOfSpaceTimeStreams = numberOfSpatialStreams + field.stbc;
    if (!isLegalTable19_12Combination(numberOfSpatialStreams, field.stbc, numberOfSpaceTimeStreams))
        return {Status::RESERVED_HT_SIG, nullptr};

    const int numberOfDataLongTrainingFields = getNumberOfDataLongTrainingFields(numberOfSpaceTimeStreams);
    const int numberOfExtensionLongTrainingFields = getNumberOfExtensionLongTrainingFields(field.numberOfExtensionSpatialStreams);
    if (numberOfDataLongTrainingFields < 0 || numberOfExtensionLongTrainingFields < 0 ||
        numberOfSpaceTimeStreams + field.numberOfExtensionSpatialStreams > 4 ||
        numberOfDataLongTrainingFields + numberOfExtensionLongTrainingFields > 5)
        return {Status::RESERVED_HT_SIG, nullptr};

    const int numberOfLongTrainingFields = numberOfDataLongTrainingFields + numberOfExtensionLongTrainingFields;
    auto description = std::shared_ptr<const Ieee80211HtPpduDescription>(new Ieee80211HtPpduDescription(
        context, field, numberOfSpatialStreams, numberOfSpaceTimeStreams,
        numberOfDataLongTrainingFields, numberOfExtensionLongTrainingFields,
        numberOfLongTrainingFields));
    return {Status::SUCCESS, description};
}

} // namespace physicallayer
} // namespace inet
