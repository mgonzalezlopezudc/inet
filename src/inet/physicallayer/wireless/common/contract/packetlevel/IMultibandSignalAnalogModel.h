//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IMULTIBANDSIGNALANALOGMODEL_H
#define __INET_IMULTIBANDSIGNALANALOGMODEL_H

#include <vector>

#include "inet/physicallayer/wireless/common/contract/packetlevel/FrequencyBand.h"

namespace inet {
namespace physicallayer {

/** Technology-neutral occupied-band metadata for an analog signal or noise. */
class INET_API IMultibandSignalAnalogModel
{
  public:
    virtual ~IMultibandSignalAnalogModel() = default;
    virtual const std::vector<FrequencyBand>& getOccupiedBands() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
