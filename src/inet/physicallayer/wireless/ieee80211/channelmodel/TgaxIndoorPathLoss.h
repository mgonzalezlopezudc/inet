//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGAXINDOORPATHLOSS_H
#define __INET_TGAXINDOORPATHLOSS_H

#include "inet/physicallayer/wireless/common/base/packetlevel/PathLossBase.h"

namespace inet {

namespace physicallayer {

/**
 * Median indoor path loss for the TGax Model B and Model D scenarios.
 *
 * This model intentionally excludes wall loss, floor loss, and shadow fading.
 */
class INET_API TgaxIndoorPathLoss : public PathLossBase
{
  protected:
    const char *channelModel = nullptr;
    m breakpointDistance = m(NaN);

  protected:
    virtual void initialize(int stage) override;
    virtual void configureChannelModel(const char *channelModel);
    virtual void validatePropagationParameters(mps propagationSpeed, Hz frequency) const;
    virtual double computeFreeSpacePathLoss(mps propagationSpeed, Hz frequency, m distance) const;

  public:
    TgaxIndoorPathLoss();

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
    virtual double computePathLoss(mps propagationSpeed, Hz frequency, m distance) const override;
    virtual m computeRange(mps propagationSpeed, Hz frequency, double loss) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
