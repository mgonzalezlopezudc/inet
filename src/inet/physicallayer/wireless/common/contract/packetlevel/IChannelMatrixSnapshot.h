//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ICHANNELMATRIXSNAPSHOT_H
#define __INET_ICHANNELMATRIXSNAPSHOT_H

#include <memory>

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixResponse.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelSnapshot.h"

namespace inet {
namespace physicallayer {

/**
 * Channel snapshot extension carrying phase-preserving antenna-pair response
 * data in addition to the legacy scalar power gain.
 */
class INET_API IChannelMatrixSnapshot : public virtual IChannelSnapshot
{
  public:
    virtual std::shared_ptr<const IChannelMatrixResponse> getChannelMatrixResponse() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
