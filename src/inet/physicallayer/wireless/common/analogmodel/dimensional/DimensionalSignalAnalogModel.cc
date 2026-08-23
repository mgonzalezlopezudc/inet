//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSignalAnalogModel.h"

#include "inet/common/math/Functions.h"

namespace inet {
namespace physicallayer {

DimensionalSignalAnalogModel::DimensionalSignalAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    DimensionalSignalAnalogModel(preambleDuration, headerDuration, dataDuration, std::vector<FrequencyBand>{FrequencyBand(centerFrequency, bandwidth)}, power)
{
}

DimensionalSignalAnalogModel::DimensionalSignalAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const std::vector<FrequencyBand>& occupiedBands, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    DimensionalSignalAnalogModel(preambleDuration, headerDuration, dataDuration, occupiedBands,
            occupiedBands.size() == 1 ? std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>{power} : std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>(), power)
{
}

std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> DimensionalSignalAnalogModel::normalizeComponentPowers(
        const std::vector<FrequencyBand>& bands,
        const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>& components,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& combinedPower)
{
    auto normalizedBands = normalizeFrequencyBands(bands);
    if (components.empty()) {
        if (normalizedBands.size() == 1)
            return {combinedPower};
        throw cRuntimeError("A dimensional multiband signal requires one component per occupied band");
    }
    if (components.size() != bands.size())
        throw cRuntimeError("A dimensional multiband signal requires one component per occupied band");
    std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> result;
    result.reserve(components.size());
    for (const auto& normalizedBand : normalizedBands) {
        auto it = std::find_if(bands.begin(), bands.end(), [&] (const FrequencyBand& band) {
            return band.centerFrequency == normalizedBand.centerFrequency && band.bandwidth == normalizedBand.bandwidth;
        });
        if (it == bands.end())
            throw cRuntimeError("Dimensional multiband signal band/component pairing is inconsistent");
        auto component = components[it - bands.begin()];
        if (component == nullptr)
            throw cRuntimeError("A dimensional multiband signal cannot contain a null component");
        result.push_back(component);
    }
    return result;
}

DimensionalSignalAnalogModel::DimensionalSignalAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration,
        const std::vector<FrequencyBand>& occupiedBands,
        const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>& componentPowers,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power) :
    NarrowbandSignalAnalogModel(preambleDuration, headerDuration, dataDuration, computeEnvelopeCenterFrequency(occupiedBands), computeEnvelopeBandwidth(occupiedBands)),
    power(power),
    occupiedBands(normalizeFrequencyBands(occupiedBands)),
    componentPowers(normalizeComponentPowers(occupiedBands, componentPowers, power))
{
}

std::ostream& DimensionalSignalAnalogModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "DimensionalSignalAnalogModel";
    if (level <= PRINT_LEVEL_DEBUG)
        stream << EV_FIELD(powerMax, power->getMax())
               << EV_FIELD(powerMin, power->getMin());
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(power);
    return NarrowbandSignalAnalogModel::printToStream(stream, level);
}

W DimensionalSignalAnalogModel::computeMinPower(simtime_t startTime, simtime_t endTime) const
{
    Point<simsec> startPoint{ simsec(startTime) };
    Point<simsec> endPoint{ simsec(endTime) };
    W minPower = integrate<WpHz, Domain<simsec, Hz>, 0b10, W, Domain<simsec>>(power)->getMin(Interval<simsec>(startPoint, endPoint, 0b1, 0b1, 0b0));
    EV_DEBUG << "Computing minimum reception power" << EV_FIELD(startPoint) << EV_FIELD(endPoint) << EV_FIELD(minPower) << endl;
    return minPower;
}

} // namespace physicallayer

} // namespace inet
