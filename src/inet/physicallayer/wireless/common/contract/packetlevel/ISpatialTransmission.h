//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ISPATIALTRANSMISSION_H
#define __INET_ISPATIALTRANSMISSION_H

#include <memory>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class SpatialTransmissionPlan;

/**
 * Optional capability exposing a transmission's immutable spatial plan.
 *
 * A null shared pointer explicitly denotes a legacy scalar transmission with
 * no spatial plan attached.
 */
class INET_API ISpatialTransmission
{
  public:
    virtual ~ISpatialTransmission() = default;
    virtual const std::shared_ptr<const SpatialTransmissionPlan>& getSpatialTransmissionPlan() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
