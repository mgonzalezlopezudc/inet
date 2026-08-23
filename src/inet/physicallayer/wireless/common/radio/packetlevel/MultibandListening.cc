//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/radio/packetlevel/MultibandListening.h"

namespace inet {
namespace physicallayer {

MultibandListening::MultibandListening(const IRadio *radio, simtime_t startTime, simtime_t endTime, Coord startPosition, Coord endPosition,
        const std::vector<FrequencyBand>& occupiedBands) :
    ListeningBase(radio, startTime, endTime, startPosition, endPosition),
    occupiedBands(normalizeFrequencyBands(occupiedBands)),
    envelopeCenterFrequency(getFrequencyBandEnvelopeCenter(this->occupiedBands)),
    envelopeBandwidth(getFrequencyBandEnvelopeBandwidth(this->occupiedBands))
{
}

std::ostream& MultibandListening::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "MultibandListening";
    if (level <= PRINT_LEVEL_DETAIL)
        stream << EV_FIELD(envelopeCenterFrequency)
               << EV_FIELD(envelopeBandwidth);
    return ListeningBase::printToStream(stream, level);
}

bool MultibandListening::contains(const FrequencyBand& band) const
{
    for (const auto& occupiedBand : occupiedBands)
        if (occupiedBand.contains(band))
            return true;
    return false;
}

} // namespace physicallayer
} // namespace inet
