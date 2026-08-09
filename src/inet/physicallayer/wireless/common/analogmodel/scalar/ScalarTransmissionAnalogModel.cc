//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmissionAnalogModel.h"

namespace inet {

namespace physicallayer {

ScalarTransmissionAnalogModel::ScalarTransmissionAnalogModel(const simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, W power) :
    ScalarSignalAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, power)
{
}

ScalarTransmissionAnalogModel::ScalarTransmissionAnalogModel(const simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration,
        const std::vector<FrequencySegment>& segments, W power) :
    ScalarSignalAnalogModel(preambleDuration, headerDuration, dataDuration,
            segments.front().centerFrequency, segments.front().bandwidth, power)
{
    frequencySegments = segments;
}

} // namespace physicallayer

} // namespace inet
