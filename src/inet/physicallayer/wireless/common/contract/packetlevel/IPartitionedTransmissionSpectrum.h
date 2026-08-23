//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IPARTITIONEDTRANSMISSIONSPECTRUM_H
#define __INET_IPARTITIONEDTRANSMISSIONSPECTRUM_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmissionSpectrum.h"

namespace inet {
namespace physicallayer {

/** Optional capability exposing immutable per-signal-part spectrum metadata. */
class INET_API IPartitionedTransmissionSpectrum
{
  public:
    virtual ~IPartitionedTransmissionSpectrum() {}
    virtual const ITransmissionSpectrum *getPreambleSpectrum() const = 0;
    virtual const ITransmissionSpectrum *getHeaderSpectrum() const = 0;
    virtual const ITransmissionSpectrum *getDataSpectrum() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
