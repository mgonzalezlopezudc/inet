//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_DIMENSIONALMEDIUMANALOGMODEL_H
#define __INET_DIMENSIONALMEDIUMANALOGMODEL_H

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/AnalogModelBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmissionSpectrum.h"

namespace inet {

namespace physicallayer {

using namespace inet::math;

class INET_API DimensionalMediumAnalogModel : public AnalogModelBase
{
  protected:
    bool attenuateWithCenterFrequency = false;

  protected:
    virtual void initialize(int stage) override;

  public:
    /** Returns a delay-adaptive frequency interval count aligned to regular occupied-spectrum centers. */
    static int computeCarrierAlignedFrequencyIntervals(Hz centerFrequency, Hz bandwidth,
        const ITransmissionSpectrum *spectrum, int minimumIntervals);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> computeReceptionPower(const IRadio *radio, const ITransmission *transmission, const IArrival *arrival) const;
    virtual const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> computeCompatibilityNoisePower(
        const IReception *reception, const class ChannelMatrixNoise *noise) const;
    virtual const INoise *computeNoise(const IListening *listening, const IInterference *interference) const override;
    virtual const INoise *computeNoise(const IReception *reception, const INoise *noise) const override;
    virtual const ISnir *computeSNIR(const IReception *reception, const INoise *noise) const override;
    virtual const IReception *computeReception(const IRadio *radio, const ITransmission *transmission, const IArrival *arrival) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
