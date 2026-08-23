//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXPHYSICALPOWERMATERIALIZER_H
#define __INET_CHANNELMATRIXPHYSICALPOWERMATERIALIZER_H

#include <memory>

#include "inet/common/math/IFunction.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

/**
 * Eagerly evaluates a channel snapshot and a spatial transmission plan into
 * an immutable aggregate physical power spectral density function.
 *
 * The returned function owns only sampled values.  In particular, evaluating
 * it never calls the snapshot or the spatial plan, which keeps reception and
 * noise objects safe after their source channel computation has gone away.
 */
class INET_API ChannelMatrixPhysicalPowerMaterializer final
{
  public:
    using PhysicalPowerFunction = math::IFunction<WpHz, math::Domain<simsec, Hz>>;

    /**
     * Computes tr(H Q(f) Csts Q(f)^H H^H) for one eagerly selected segment.
     * The frequency argument is baseband frequency, in hertz.
     */
    static double computePhysicalGain(const IChannelMatrixSnapshot& snapshot,
        const SpatialTransmissionPlan::Segment& segment, simtime_t absoluteTime,
        Hz basebandFrequency);

    /**
     * Samples the channel over the complete reception interval and returns a
     * self-contained aggregate physical PSD equal to the supplied deterministic
     * large-scale PSD multiplied by the sampled physical matrix gain.
     */
    static const Ptr<const PhysicalPowerFunction> materialize(
        const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
        const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
        simtime_t receptionStartTime, simtime_t receptionEndTime,
        Hz centerFrequency, Hz bandwidth,
        const Ptr<const PhysicalPowerFunction>& deterministicLargeScalePower);
};

} // namespace physicallayer
} // namespace inet

#endif
