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
#include "inet/physicallayer/wireless/common/signal/ChannelMatrixSignal.h"

namespace inet {

namespace physicallayer {

using namespace inet::math;

class INET_API DimensionalMediumAnalogModel : public AnalogModelBase
{
  protected:
    bool attenuateWithCenterFrequency = false;
    bool enableChannelMatrixMrc = false;
    bool enableChannelMatrixLmmse = false;
    int channelMatrixTransmitAntenna = 0;
    simtime_t channelMatrixTimeResolution = -1;
    Hz channelMatrixFrequencyResolution = Hz(NaN);

  protected:
    virtual void initialize(int stage) override;

  public:
    virtual bool supportsWidebandChannelModel() const override { return true; }
    bool isChannelMatrixLmmseEnabled() const { return enableChannelMatrixLmmse; }

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> computeReceptionPower(const IRadio *radio,
            const ITransmission *transmission, const IArrival *arrival, bool *channelMatrixCombined = nullptr,
            Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> *interferencePower = nullptr,
            std::shared_ptr<const ChannelMatrixSignal> *channelMatrixSignal = nullptr) const;
    virtual const INoise *computeNoise(const IListening *listening, const IInterference *interference) const override;
    virtual const INoise *computeNoise(const IReception *reception, const INoise *noise) const override;
    virtual const ISnir *computeSNIR(const IReception *reception, const INoise *noise) const override;
    virtual const IReception *computeReception(const IRadio *radio, const ITransmission *transmission, const IArrival *arrival) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
