//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGAXCHANNELPROFILE_H
#define __INET_TGAXCHANNELPROFILE_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {

namespace physicallayer {

using namespace inet::units::values;

/**
 * Pure value object containing a normalized NLOS indoor TGax channel profile.
 * Profiles above 40 MHz contain the bandwidth-dependent independent components
 * added by the TGac expansion procedure; they are not merely a wider sampling
 * of the unexpanded TGn profile.
 */
class INET_API TgaxChannelProfile
{
  public:
    enum class Model {
        A,
        B,
        C,
        D,
        E,
        F
    };

    struct Component {
        int clusterIndex;
        double excessDelayNs;
        double powerDb;
        double normalizedPower;
    };

  protected:
    Model model;
    int expansionFactor;
    std::vector<Component> components;

  protected:
    TgaxChannelProfile(Model model, int expansionFactor, std::vector<Component> components);

  public:
    static TgaxChannelProfile create(Model model, Hz bandwidth);
    static TgaxChannelProfile create(const char *model, Hz bandwidth);

    Model getModel() const { return model; }
    int getExpansionFactor() const { return expansionFactor; }
    const std::vector<Component>& getComponents() const { return components; }

    double computeRmsDelaySpreadNs() const;
};

} // namespace physicallayer

} // namespace inet

#endif
