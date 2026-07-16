//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IWIDEBANDCHANNELMODEL_H
#define __INET_IWIDEBANDCHANNELMODEL_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IArrival.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelSnapshot.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmission.h"

namespace inet {
namespace physicallayer {

/**
 * This interface models link-specific wideband channel effects. Implementations
 * may keep state across transmissions, but the returned snapshot must own or
 * retain all data needed to keep it immutable independently of subsequent
 * channel state changes, link eviction, and radio removal.
 */
class INET_API IWidebandChannelModel : public virtual IPrintableObject
{
  public:
    virtual ~IWidebandChannelModel() {}

    virtual Ptr<const IChannelSnapshot> computeChannel(const IRadio *receiver, const ITransmission *transmission, const IArrival *arrival) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
