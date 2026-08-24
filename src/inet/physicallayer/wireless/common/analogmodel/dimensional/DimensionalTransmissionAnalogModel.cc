//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalTransmissionAnalogModel.h"

namespace inet {
namespace physicallayer {

DimensionalTransmissionAnalogModel::DimensionalTransmissionAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    DimensionalTransmissionAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, power, nullptr, nullptr, nullptr)
{
}

DimensionalTransmissionAnalogModel::DimensionalTransmissionAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power, const ITransmissionSpectrum *preambleSpectrum, const ITransmissionSpectrum *headerSpectrum, const ITransmissionSpectrum *dataSpectrum) :
    DimensionalSignalAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, power),
    preambleSpectrum(preambleSpectrum == nullptr ? nullptr : std::make_unique<const TransmissionSpectrum>(preambleSpectrum->getBands())),
    headerSpectrum(headerSpectrum == nullptr ? nullptr : std::make_unique<const TransmissionSpectrum>(headerSpectrum->getBands())),
    dataSpectrum(dataSpectrum == nullptr ? nullptr : std::make_unique<const TransmissionSpectrum>(dataSpectrum->getBands()))
{
}

} // namespace physicallayer

} // namespace inet
