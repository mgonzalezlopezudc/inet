//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IDIMENSIONALTRANSMITTERANALOGMODEL_H
#define __INET_IDIMENSIONALTRANSMITTERANALOGMODEL_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmissionSpectrum.h"

namespace inet {
namespace physicallayer {

/** Optional spectrum-aware capability of a dimensional transmitter model. */
class INET_API IDimensionalTransmitterAnalogModel : public virtual ITransmitterAnalogModel
{
  public:
    using ITransmitterAnalogModel::createAnalogModel;

    virtual ITransmissionAnalogModel *createAnalogModel(
            simtime_t preambleDuration,
            simtime_t headerDuration,
            simtime_t dataDuration,
            Hz centerFrequency,
            Hz nominalBandwidth,
            W totalPower,
            const ITransmissionSpectrum *preambleSpectrum,
            const ITransmissionSpectrum *headerSpectrum,
            const ITransmissionSpectrum *dataSpectrum) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
