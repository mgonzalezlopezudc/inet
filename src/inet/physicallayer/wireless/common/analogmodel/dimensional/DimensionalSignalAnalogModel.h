//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_DIMENSIONALSIGNALANALOGMODEL_H
#define __INET_DIMENSIONALSIGNALANALOGMODEL_H

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/NarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/FrequencyBand.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IDimensionalSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IMultibandSignalAnalogModel.h"

namespace inet {

namespace physicallayer {

using namespace inet::math;

class INET_API DimensionalSignalAnalogModel : public NarrowbandSignalAnalogModel, public IDimensionalSignalAnalogModel, public IMultibandSignalAnalogModel
{
  protected:
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> power;
    const std::vector<FrequencyBand> occupiedBands;
    const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> componentPowers;

    static std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>> normalizeComponentPowers(
            const std::vector<FrequencyBand>& bands,
            const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>& components,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& combinedPower);

  protected:
    static Hz computeEnvelopeCenterFrequency(const std::vector<FrequencyBand>& occupiedBands) { return getFrequencyBandEnvelopeCenter(occupiedBands); }
    static Hz computeEnvelopeBandwidth(const std::vector<FrequencyBand>& occupiedBands) { return getFrequencyBandEnvelopeBandwidth(occupiedBands); }

  public:
    DimensionalSignalAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power);
    DimensionalSignalAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const std::vector<FrequencyBand>& occupiedBands, const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power);
    DimensionalSignalAnalogModel(const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration,
            const std::vector<FrequencyBand>& occupiedBands,
            const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>& componentPowers,
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getPower() const override { return power; }
    virtual const std::vector<FrequencyBand>& getOccupiedBands() const override { return occupiedBands; }
    virtual const std::vector<Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>>& getComponentPowers() const { return componentPowers; }
    virtual W computeMinPower(simtime_t startTime, simtime_t endTime) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
