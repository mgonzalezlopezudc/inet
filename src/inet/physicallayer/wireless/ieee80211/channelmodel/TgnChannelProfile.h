//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGNCHANNELPROFILE_H
#define __INET_TGNCHANNELPROFILE_H

#include <cmath>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

enum class TgnModel { A, B, C, D, E, F };
enum class TgnCondition { NLOS, LOS };

struct INET_API TgnTap
{
    int reportTapIndex;
    simtime_t excessDelay;
};

struct INET_API TgnCluster
{
    int reportClusterIndex;
    double meanAoADegrees;
    double receiverAngularSpreadDegrees;
    double meanAoDDegrees;
    double transmitterAngularSpreadDegrees;
};

struct INET_API TgnComponent
{
    int stableComponentIndex;
    int reportClusterIndex;
    int reportTapIndex;
    double relativePowerDb;
    double rawLinearPower;
    double normalizedLinearPower;
};

class INET_API TgnChannelProfile
{
  protected:
    TgnModel model;
    simtime_t rmsDelaySpread;
    double breakpointDistanceMeters;
    double shadowSigmaLosDb;
    double shadowSigmaNlosDb;
    double firstTapKDb;
    std::vector<TgnTap> taps;
    std::vector<TgnCluster> clusters;
    std::vector<TgnComponent> components;

  public:
    TgnChannelProfile(TgnModel model, simtime_t rmsDelaySpread, double breakpointDistanceMeters,
        double shadowSigmaLosDb, double shadowSigmaNlosDb, double firstTapKDb,
        const std::vector<TgnCluster>& clusters, const std::vector<TgnTap>& taps,
        const std::vector<TgnComponent>& components);

  protected:
    void validate() const;

  public:
    static TgnModel parseModel(const char *name);
    static TgnCondition parseCondition(const char *name);
    static const char *getModelName(TgnModel model);
    static TgnChannelProfile create(TgnModel model);

    TgnModel getModel() const { return model; }
    simtime_t getRmsDelaySpread() const { return rmsDelaySpread; }
    double getBreakpointDistanceMeters() const { return breakpointDistanceMeters; }
    double getShadowSigmaLosDb() const { return shadowSigmaLosDb; }
    double getShadowSigmaNlosDb() const { return shadowSigmaNlosDb; }
    double getFirstTapKDb() const { return firstTapKDb; }
    double getFirstTapKLinear() const { return std::pow(10.0, firstTapKDb / 10.0); }
    simtime_t getMaximumExcessDelay() const { return taps.back().excessDelay; }
    const std::vector<TgnTap>& getTaps() const { return taps; }
    const std::vector<TgnCluster>& getClusters() const { return clusters; }
    const std::vector<TgnComponent>& getComponents() const { return components; }
    const TgnTap& getTap(int reportTapIndex) const;
    const TgnCluster& getCluster(int reportClusterIndex) const;
    const TgnComponent& getFirstTapComponent() const;
    bool hasFluorescentEffect(const TgnComponent& component) const;
    bool hasVehicleEffect(const TgnComponent& component) const;
};

} // namespace physicallayer
} // namespace inet

#endif
