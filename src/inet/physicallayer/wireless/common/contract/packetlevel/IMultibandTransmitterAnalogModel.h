//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IMULTIBANDTRANSMITTERANALOGMODEL_H
#define __INET_IMULTIBANDTRANSMITTERANALOGMODEL_H

#include <vector>

#include "inet/physicallayer/wireless/common/contract/packetlevel/FrequencyBand.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmitterAnalogModel.h"

namespace inet {
namespace physicallayer {

/** Optional factory for one transmission occupying multiple disjoint bands. */
class INET_API IMultibandTransmitterAnalogModel : public virtual ITransmitterAnalogModel
{
  public:
    virtual ITransmissionAnalogModel *createAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration,
            const std::vector<FrequencyBand>& occupiedBands, W power) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
