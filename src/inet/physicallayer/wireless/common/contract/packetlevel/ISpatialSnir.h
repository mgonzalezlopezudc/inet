//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ISPATIALSNIR_H
#define __INET_ISPATIALSNIR_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h"

namespace inet {
namespace physicallayer {

class MaterializedSpatialReception;

/** Optional SNIR capability exposing immutable per-cell/per-stream results. */
class INET_API ISpatialSnir : public virtual ISnir
{
  public:
    virtual const MaterializedSpatialReception *getMaterializedSpatialReception() const = 0;
    virtual double getMinimum(IRadioSignal::SignalPart part) const = 0;
    virtual double getMaximum(IRadioSignal::SignalPart part) const = 0;
    virtual double getMean(IRadioSignal::SignalPart part) const = 0;
    virtual bool allRequiredOutputPowersMeet(IRadioSignal::SignalPart part, W sensitivity) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
