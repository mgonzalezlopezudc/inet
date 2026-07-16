//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ICHANNELSNAPSHOT_H
#define __INET_ICHANNELSNAPSHOT_H

#include "inet/common/IPrintableObject.h"
#include "inet/common/Ptr.h"
#include "inet/common/math/IFunction.h"
#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

using namespace inet::math;
using namespace inet::units::values;

/**
 * This interface represents an immutable snapshot of a wideband channel for
 * one transmission and receiver. The power gain may vary over absolute
 * simulation time at the receiver and radio frequency.
 *
 * The power gain is a finite, non-negative, dimensionless multiplier which
 * excludes transmitter and receiver antenna gain, path loss, and obstacle
 * loss. The returned function must own or retain all data it references and
 * remain valid and invariant independently after the snapshot is destroyed.
 *
 * This power-domain interface supports SISO channel realizations. Channel
 * models requiring phase or antenna-pair transfer matrices may add further
 * immutable response data to the snapshot.
 */
class INET_API IChannelSnapshot : public virtual IPrintableObject
#if INET_PTR_IMPLEMENTATION == INET_INTRUSIVE_PTR
    , public IntrusivePtrCounter<IChannelSnapshot>
#endif
{
  public:
    virtual ~IChannelSnapshot() {}

    virtual Ptr<const IFunction<double, Domain<simsec, Hz>>> getPowerGain() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
