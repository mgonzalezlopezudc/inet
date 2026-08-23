//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/backgroundnoise/IsotropicDimensionalBackgroundNoise.h"

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/MultibandListening.h"

namespace inet {

namespace physicallayer {

Define_Module(IsotropicDimensionalBackgroundNoise);

void IsotropicDimensionalBackgroundNoise::initialize(int stage)
{
    cModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        powerSpectralDensity = WpHz(dBmWpMHz2WpHz(par("powerSpectralDensity")));
        power = mW(dBmW2mW(par("power")));
        bandwidth = Hz(par("bandwidth"));
        if (std::isnan(powerSpectralDensity.get()) && std::isnan(power.get()))
            throw cRuntimeError("One of powerSpectralDensity or power parameters must be specified");
        if (!std::isnan(powerSpectralDensity.get()) && !std::isnan(power.get()))
            throw cRuntimeError("Both of powerSpectralDensity and power parameters cannot be specified");
    }
}

std::ostream& IsotropicDimensionalBackgroundNoise::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "IsotropicDimensionalBackgroundNoise";
    if (level <= PRINT_LEVEL_DETAIL) {
        if (!std::isnan(powerSpectralDensity.get()))
            stream << EV_FIELD(powerSpectralDensity);
        else {
            stream << EV_FIELD(power);
            stream << EV_FIELD(bandwidth);
        }
    }
    return stream;
}

const INoise *IsotropicDimensionalBackgroundNoise::computeNoise(const IListening *listening) const
{
    const auto multibandListening = dynamic_cast<const MultibandListening *>(listening);
    const BandListening *bandListening = multibandListening == nullptr ? check_and_cast<const BandListening *>(listening) : nullptr;
    Hz centerFrequency = bandListening != nullptr ? bandListening->getCenterFrequency() : multibandListening->getCenterFrequency();
    Hz listeningBandwidth = bandListening != nullptr ? bandListening->getBandwidth() : multibandListening->getBandwidth();
    WpHz noisePowerSpectralDensity;
    if (!std::isnan(powerSpectralDensity.get()))
        noisePowerSpectralDensity = powerSpectralDensity;
    else {
        Hz noiseBandwidth = std::isnan(bandwidth.get()) ?
                (multibandListening == nullptr ? listeningBandwidth : getFrequencyBandTotalBandwidth(multibandListening->getOccupiedBands())) : bandwidth;
        // A scalar power parameter denotes integrated power over the
        // configured band. Keep its equivalent flat PSD when a per-channel
        // CCA query selects a narrower HT40 slice.
        noisePowerSpectralDensity = power / noiseBandwidth;
    }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& powerFunction = makeShared<ConstantFunction<WpHz, Domain<simsec, Hz>>>(noisePowerSpectralDensity);
    const simtime_t startTime = listening->getStartTime();
    const simtime_t endTime = listening->getEndTime();
    if (multibandListening == nullptr)
        return new DimensionalNoise(startTime, endTime, centerFrequency, listeningBandwidth, makeFirstQuadrantLimitedFunction(powerFunction));
    std::vector<Ptr<const IFunction<double, Domain<simsec, Hz>>>> masks;
    masks.reserve(multibandListening->getOccupiedBands().size());
    for (const auto& band : multibandListening->getOccupiedBands())
        masks.push_back(makeShared<Boxcar2DFunction<double, simsec, Hz>>(simsec(startTime), simsec(endTime), band.getLowerFrequency(), band.getUpperFrequency(), 1));
    Ptr<const IFunction<double, Domain<simsec, Hz>>> occupiedMask;
    if (masks.size() == 1)
        occupiedMask = masks.front();
    else
        occupiedMask = makeShared<SummedFunction<double, Domain<simsec, Hz>>>(masks);
    auto maskedPower = makeFirstQuadrantLimitedFunction(powerFunction)->multiply(occupiedMask);
    return new DimensionalNoise(startTime, endTime, multibandListening->getOccupiedBands(), maskedPower);
}

} // namespace physicallayer

} // namespace inet
