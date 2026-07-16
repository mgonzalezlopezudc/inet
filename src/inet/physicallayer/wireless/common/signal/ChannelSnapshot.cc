//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/signal/ChannelSnapshot.h"

namespace inet {

namespace physicallayer {

ChannelSnapshot::ChannelSnapshot(const Ptr<const IFunction<double, Domain<simsec, Hz>>>& powerGain) :
    powerGain(powerGain)
{
    if (powerGain == nullptr)
        throw cRuntimeError("Channel snapshot power gain function must not be null");
}

std::ostream& ChannelSnapshot::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "ChannelSnapshot";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(powerGain);
    return stream;
}

Ptr<const IFunction<double, Domain<simsec, Hz>>> ChannelSnapshot::getPowerGain() const
{
    return powerGain;
}

} // namespace physicallayer

} // namespace inet
