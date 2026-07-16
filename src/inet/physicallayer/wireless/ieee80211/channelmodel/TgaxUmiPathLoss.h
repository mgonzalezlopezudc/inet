//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGAXUMIPATHLOSS_H
#define __INET_TGAXUMIPATHLOSS_H

#include "inet/physicallayer/wireless/common/base/packetlevel/PathLossBase.h"

namespace inet {
namespace physicallayer {

/**
 * Median ITU-R UMi path loss selected by the TGax channel-model document.
 *
 * This link-level model uses configured effective endpoint roles/heights. It
 * intentionally excludes LOS selection, shadow fading, and building
 * penetration loss.
 */
class INET_API TgaxUmiPathLoss : public PathLossBase
{
  public:
    enum class PropagationCondition { LOS, NLOS };

  protected:
    PropagationCondition propagationCondition = PropagationCondition::NLOS;
    m baseStationHeight = m(10);
    m userTerminalHeight = m(1.5);
    bool enforceApplicabilityRange = true;

  protected:
    virtual void initialize(int stage) override;
    virtual void configure(PropagationCondition propagationCondition,
            m baseStationHeight, m userTerminalHeight, bool enforceApplicabilityRange);
    virtual void configure(const char *propagationCondition,
            m baseStationHeight, m userTerminalHeight, bool enforceApplicabilityRange);
    virtual void validatePropagationParameters(mps propagationSpeed, Hz frequency) const;
    virtual void validateDistance(m distance) const;
    virtual m computeBreakpointDistance(mps propagationSpeed, Hz frequency) const;
    virtual double computePathLossDb(mps propagationSpeed, Hz frequency, m distance) const;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level,
            int evFlags = 0) const override;
    virtual double computePathLoss(mps propagationSpeed, Hz frequency, m distance) const override;
    virtual m computeRange(mps propagationSpeed, Hz frequency, double loss) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
