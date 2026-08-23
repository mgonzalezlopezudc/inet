//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalTransmissionAnalogModel.h"

namespace inet {
namespace physicallayer {

DimensionalTransmissionAnalogModel::DimensionalTransmissionAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    DimensionalSignalAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, power)
{
}

DimensionalTransmissionAnalogModel::DimensionalTransmissionAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const std::vector<FrequencyBand>& occupiedBands, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    DimensionalSignalAnalogModel(preambleDuration, headerDuration, dataDuration, occupiedBands, power)
{
}

DimensionalTransmissionAnalogModel::DimensionalTransmissionAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration,
        const std::vector<FrequencyBand>& occupiedBands,
        const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>& componentPowers,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    DimensionalSignalAnalogModel(preambleDuration, headerDuration, dataDuration, occupiedBands, componentPowers, power)
{
}

} // namespace physicallayer

} // namespace inet
