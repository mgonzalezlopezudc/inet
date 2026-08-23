//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/backgroundnoise/DimensionalBackgroundNoise.h"

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/MultibandListening.h"

namespace inet {

namespace physicallayer {

Define_Module(DimensionalBackgroundNoise);

DimensionalBackgroundNoise::DimensionalBackgroundNoise() :
    power(W(NaN))
{
}

void DimensionalBackgroundNoise::initialize(int stage)
{
    DimensionalTransmitterBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        power = mW(dBmW2mW(par("power")));
    }
}

std::ostream& DimensionalBackgroundNoise::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "DimensionalBackgroundNoise";
    return DimensionalTransmitterBase::printToStream(stream, level);
}

const INoise *DimensionalBackgroundNoise::computeNoise(const IListening *listening) const
{
    const auto multibandListening = dynamic_cast<const MultibandListening *>(listening);
    const BandListening *bandListening = multibandListening == nullptr ? check_and_cast<const BandListening *>(listening) : nullptr;
    const simtime_t startTime = listening->getStartTime();
    const simtime_t endTime = listening->getEndTime();
    if (multibandListening == nullptr) {
        Hz centerFrequency = bandListening->getCenterFrequency();
        Hz bandwidth = bandListening->getBandwidth();
        const auto& powerFunction = createPowerFunction(startTime, endTime, centerFrequency, bandwidth, power);
        return new DimensionalNoise(startTime, endTime, centerFrequency, bandwidth, powerFunction);
    }
    const auto& occupiedBands = multibandListening->getOccupiedBands();
    if (!frequencyGains.empty() && strcmp(frequencyGainsNormalization, "integral") != 0)
        throw cRuntimeError("Dimensional multiband background noise requires integral-normalized frequency gains");
    Hz totalBandwidth = getFrequencyBandTotalBandwidth(occupiedBands);
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> components;
    components.reserve(occupiedBands.size());
    for (const auto& band : occupiedBands) {
        W segmentPower = power * (band.bandwidth / totalBandwidth).get();
        components.push_back(createPowerFunction(startTime, endTime, band.centerFrequency, band.bandwidth, segmentPower));
    }
    auto powerFunction = makeShared<SummedFunction<WpHz, Domain<simsec, Hz>>>(components);
    return new DimensionalNoise(startTime, endTime, occupiedBands, powerFunction);
}

} // namespace physicallayer

} // namespace inet
