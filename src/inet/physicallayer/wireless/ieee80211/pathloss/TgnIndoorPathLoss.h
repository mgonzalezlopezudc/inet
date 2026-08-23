//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGNINDOORPATHLOSS_H
#define __INET_TGNINDOORPATHLOSS_H

#include "inet/physicallayer/wireless/common/base/packetlevel/PathLossBase.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgnChannelProfile.h"

namespace inet {
namespace physicallayer {

class INET_API TgnIndoorPathLoss : public PathLossBase
{
  protected:
    TgnModel model = TgnModel::B;
    m breakpointDistance = m(NaN);
    m referenceDistance = m(NaN);

  protected:
    virtual void initialize(int stage) override;
    double computeFreeSpacePowerGain(mps propagationSpeed, Hz frequency, m distance) const;

  public:
    virtual double computePathLoss(mps propagationSpeed, Hz frequency, m distance) const override;
    virtual m computeRange(mps propagationSpeed, Hz frequency, double loss) const override;
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
