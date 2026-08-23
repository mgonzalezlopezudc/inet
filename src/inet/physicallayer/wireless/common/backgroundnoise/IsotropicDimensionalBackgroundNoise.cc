//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/backgroundnoise/IsotropicDimensionalBackgroundNoise.h"

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/MultibandFunction.h"
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
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> components(multibandListening->getOccupiedBands().size(), makeFirstQuadrantLimitedFunction(powerFunction));
    auto maskedPower = makeShared<MultibandFunction<WpHz>>(multibandListening->getOccupiedBands(), components);
    return new DimensionalNoise(startTime, endTime, multibandListening->getOccupiedBands(), maskedPower);
}

} // namespace physicallayer

} // namespace inet
