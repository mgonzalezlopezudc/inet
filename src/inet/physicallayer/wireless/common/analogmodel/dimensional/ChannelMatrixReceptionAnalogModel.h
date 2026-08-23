//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRECEPTIONANALOGMODEL_H
#define __INET_CHANNELMATRIXRECEPTIONANALOGMODEL_H

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixReceptionAnalogModel : public DimensionalReceptionAnalogModel
{
  protected:
    std::shared_ptr<const IChannelMatrixSnapshot> snapshot;
    int selectedTransmitAntenna;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> deterministicLargeScalePower;
    Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> ccaPower;

  public:
    ChannelMatrixReceptionAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration,
        Hz centerFrequency, Hz bandwidth, const std::shared_ptr<const IChannelMatrixSnapshot>& snapshot,
        int selectedTransmitAntenna,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& deterministicLargeScalePower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& decodedPower,
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& ccaPower);

    const std::shared_ptr<const IChannelMatrixSnapshot>& getSnapshot() const { return snapshot; }
    int getSelectedTransmitAntenna() const { return selectedTransmitAntenna; }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getDeterministicLargeScalePower() const { return deterministicLargeScalePower; }
    const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& getCcaPower() const { return ccaPower; }
};

} // namespace physicallayer
} // namespace inet

#endif
