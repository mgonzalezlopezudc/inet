//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmitterAnalogModel.h"

namespace inet {
namespace physicallayer {

Define_Module(ScalarTransmitterAnalogModel);

void ScalarTransmitterAnalogModel::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        defaultCenterFrequency = Hz(par("centerFrequency"));
        defaultBandwidth = Hz(par("bandwidth"));
        defaultPower = W(par("power"));
    }
}

ITransmissionAnalogModel *ScalarTransmitterAnalogModel::createAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, W power) const
{
    auto transmissionCenterFrequency = computeCenterFrequency(centerFrequency);
    auto transmissionBandwidth = computeBandwidth(bandwidth);
    auto transmissionPower = computePower(power);
    return new ScalarTransmissionAnalogModel(preambleDuration, headerDuration, dataDuration, transmissionCenterFrequency, transmissionBandwidth, transmissionPower);
}

ITransmissionAnalogModel *ScalarTransmitterAnalogModel::createAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration,
        const std::vector<FrequencySegment>& segments, W power) const
{
    if (segments.empty())
        throw cRuntimeError("Cannot create a transmission with no frequency segments");
    std::vector<FrequencySegment> computedSegments = segments;
    for (auto& segment : computedSegments) {
        segment.centerFrequency = computeCenterFrequency(segment.centerFrequency);
        segment.bandwidth = computeBandwidth(segment.bandwidth);
    }
    return new ScalarTransmissionAnalogModel(preambleDuration, headerDuration, dataDuration,
            computedSegments, computePower(power));
}

} // namespace physicallayer
} // namespace inet
