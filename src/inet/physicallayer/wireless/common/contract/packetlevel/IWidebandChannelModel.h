//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IWIDEBANDCHANNELMODEL_H
#define __INET_IWIDEBANDCHANNELMODEL_H

#include <memory>

#include "inet/common/IPrintableObject.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IArrival.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmission.h"

namespace inet {
namespace physicallayer {

class INET_API IWidebandChannelModel : public virtual IPrintableObject
{
  public:
    virtual ~IWidebandChannelModel() = default;
    virtual void addRadio(const IRadio *radio) = 0;
    virtual void removeRadio(const IRadio *radio) = 0;
    virtual std::shared_ptr<const IChannelMatrixSnapshot> computeChannel(const IRadio *receiver, const ITransmission *transmission, const IArrival *arrival) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
