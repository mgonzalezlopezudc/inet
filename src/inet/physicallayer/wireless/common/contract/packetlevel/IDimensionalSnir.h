//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IDIMENSIONALSNIR_H
#define __INET_IDIMENSIONALSNIR_H

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h"

namespace inet {
namespace physicallayer {

/** Optional capability exposing the immutable time/frequency SNIR function. */
class INET_API IDimensionalSnir : public virtual ISnir
{
  public:
    virtual const Ptr<const math::IFunction<double, math::Domain<simsec, Hz>>> getSnir() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
