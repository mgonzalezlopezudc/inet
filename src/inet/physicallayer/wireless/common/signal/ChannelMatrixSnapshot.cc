//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/signal/ChannelMatrixSnapshot.h"

#include <cmath>
#include <utility>

namespace inet {
namespace physicallayer {

ConstantChannelMatrixResponse::ConstantChannelMatrixResponse(ChannelMatrix channelMatrix) :
    channelMatrix(std::move(channelMatrix))
{
}

int ConstantChannelMatrixResponse::getNumReceiveAntennas() const
{
    return channelMatrix.getNumReceiveAntennas();
}

int ConstantChannelMatrixResponse::getNumTransmitAntennas() const
{
    return channelMatrix.getNumTransmitAntennas();
}

ChannelMatrix ConstantChannelMatrixResponse::getChannelMatrix(simsec time, Hz frequency) const
{
    if (!std::isfinite(time.get<s>().dbl()) || !std::isfinite(frequency.get<Hz>()) || frequency <= Hz(0))
        throw cRuntimeError("Channel matrix query time must be finite and frequency must be finite and positive");
    return channelMatrix;
}

ChannelMatrixSnapshot::ChannelMatrixSnapshot(const Ptr<const IFunction<double, Domain<simsec, Hz>>>& powerGain,
        std::shared_ptr<const IChannelMatrixResponse> channelMatrixResponse) :
    powerGain(powerGain),
    channelMatrixResponse(std::move(channelMatrixResponse))
{
    if (powerGain == nullptr)
        throw cRuntimeError("Channel matrix snapshot power gain function must not be null");
    if (this->channelMatrixResponse == nullptr)
        throw cRuntimeError("Channel matrix snapshot response must not be null");
    if (this->channelMatrixResponse->getNumReceiveAntennas() <= 0 ||
            this->channelMatrixResponse->getNumTransmitAntennas() <= 0)
        throw cRuntimeError("Channel matrix snapshot response dimensions must be positive");
}

std::ostream& ChannelMatrixSnapshot::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "ChannelMatrixSnapshot";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(powerGain)
               << EV_FIELD(numReceiveAntennas, channelMatrixResponse->getNumReceiveAntennas())
               << EV_FIELD(numTransmitAntennas, channelMatrixResponse->getNumTransmitAntennas());
    return stream;
}

Ptr<const IFunction<double, Domain<simsec, Hz>>> ChannelMatrixSnapshot::getPowerGain() const
{
    return powerGain;
}

std::shared_ptr<const IChannelMatrixResponse> ChannelMatrixSnapshot::getChannelMatrixResponse() const
{
    return channelMatrixResponse;
}

} // namespace physicallayer
} // namespace inet
