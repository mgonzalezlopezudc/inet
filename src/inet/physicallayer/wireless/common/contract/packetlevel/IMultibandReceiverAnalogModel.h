//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IMULTIBANDRECEIVERANALOGMODEL_H
#define __INET_IMULTIBANDRECEIVERANALOGMODEL_H

#include <vector>

#include "inet/physicallayer/wireless/common/contract/packetlevel/FrequencyBand.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReceiverAnalogModel.h"

namespace inet {
namespace physicallayer {

/** Optional receiver factory for a union listening mask over disjoint bands. */
class INET_API IMultibandReceiverAnalogModel : public virtual IReceiverAnalogModel
{
  public:
    virtual IListening *createListening(const IRadio *radio, const simtime_t startTime, const simtime_t endTime,
            const Coord& startPosition, const Coord& endPosition, const std::vector<FrequencyBand>& occupiedBands) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
