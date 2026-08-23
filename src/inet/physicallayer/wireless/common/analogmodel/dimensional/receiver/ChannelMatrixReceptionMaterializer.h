//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRECEPTIONMATERIALIZER_H
#define __INET_CHANNELMATRIXRECEPTIONMATERIALIZER_H

#include <memory>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class ChannelMatrixNoise;
class IChannelMatrixReceiver;
class IReception;
class MaterializedSpatialReception;

/** Builds all covariance/detector cells eagerly before SNIR publication. */
class INET_API ChannelMatrixReceptionMaterializer final
{
  public:
    static std::shared_ptr<const MaterializedSpatialReception> materialize(
        const IReception& reception, const ChannelMatrixNoise& noise,
        const IChannelMatrixReceiver& receiver);
};

} // namespace physicallayer
} // namespace inet

#endif
