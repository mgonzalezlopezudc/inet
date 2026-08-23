//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MULTIBANDLISTENING_H
#define __INET_MULTIBANDLISTENING_H

#include <vector>

#include "inet/physicallayer/wireless/common/base/packetlevel/ListeningBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/FrequencyBand.h"

namespace inet {
namespace physicallayer {

/**
 * Immutable listening mask for one or more disjoint RF bands.  The center and
 * bandwidth accessors expose only the outer envelope for legacy metadata;
 * medium and receiver code must use getOccupiedBands() for spectral tests.
 */
class INET_API MultibandListening : public ListeningBase
{
  protected:
    const std::vector<FrequencyBand> occupiedBands;
    const Hz envelopeCenterFrequency;
    const Hz envelopeBandwidth;

  public:
    MultibandListening(const IRadio *radio, simtime_t startTime, simtime_t endTime, Coord startPosition, Coord endPosition,
            const std::vector<FrequencyBand>& occupiedBands);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const std::vector<FrequencyBand>& getOccupiedBands() const { return occupiedBands; }
    virtual Hz getCenterFrequency() const { return envelopeCenterFrequency; }
    virtual Hz getBandwidth() const { return envelopeBandwidth; }
    virtual Hz getLowerFrequency() const { return occupiedBands.front().getLowerFrequency(); }
    virtual Hz getUpperFrequency() const { return occupiedBands.back().getUpperFrequency(); }
    virtual bool contains(const FrequencyBand& band) const;
    virtual bool contains(const std::vector<FrequencyBand>& bands) const { return containsFrequencyBandSet(occupiedBands, bands); }
};

} // namespace physicallayer
} // namespace inet

#endif
