//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgnChannelProfile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>

namespace inet {
namespace physicallayer {

namespace {

using PowerEntry = std::pair<double, double>;

struct RawCluster {
    TgnCluster cluster;
    std::vector<PowerEntry> powers;
};

simtime_t ns(double value)
{
    return SimTime(value, SIMTIME_NS);
}

RawCluster raw(int index, double aoa, double rxAs, double aod, double txAs, std::initializer_list<PowerEntry> powers)
{
    return {{index, aoa, rxAs, aod, txAs}, powers};
}

TgnChannelProfile buildProfile(TgnModel model, double rmsNs, double breakpoint, double nlosSigma, double kDb,
    std::initializer_list<RawCluster> rawClusters)
{
    std::vector<double> delays;
    for (const auto& rawCluster : rawClusters)
        for (const auto& power : rawCluster.powers)
            delays.push_back(power.first);
    std::sort(delays.begin(), delays.end());
    delays.erase(std::unique(delays.begin(), delays.end()), delays.end());

    std::vector<TgnTap> taps;
    std::map<double, int> tapIndices;
    for (size_t i = 0; i < delays.size(); i++) {
        taps.push_back({(int)i + 1, ns(delays[i])});
        tapIndices[delays[i]] = i + 1;
    }

    std::vector<TgnCluster> clusters;
    std::vector<TgnComponent> components;
    double totalRawPower = 0;
    for (const auto& rawCluster : rawClusters) {
        clusters.push_back(rawCluster.cluster);
        for (const auto& power : rawCluster.powers) {
            double rawLinearPower = std::pow(10.0, power.second / 10.0);
            components.push_back({(int)components.size(), rawCluster.cluster.reportClusterIndex,
                tapIndices.at(power.first), power.second, rawLinearPower, 0});
            totalRawPower += rawLinearPower;
        }
    }
    // INET policy: normalize the complete NLOS component-power sum once; do
    // not independently normalize taps, clusters, or overlapping delays.
    for (auto& component : components)
        component.normalizedLinearPower = component.rawLinearPower / totalRawPower;

    return TgnChannelProfile(model, ns(rmsNs), breakpoint, 3, nlosSigma, kDb, clusters, taps, components);
}

} // namespace

TgnChannelProfile::TgnChannelProfile(TgnModel model, simtime_t rmsDelaySpread, double breakpointDistanceMeters,
    double shadowSigmaLosDb, double shadowSigmaNlosDb, double firstTapKDb,
    const std::vector<TgnCluster>& clusters, const std::vector<TgnTap>& taps,
    const std::vector<TgnComponent>& components) :
    model(model), rmsDelaySpread(rmsDelaySpread), breakpointDistanceMeters(breakpointDistanceMeters),
    shadowSigmaLosDb(shadowSigmaLosDb), shadowSigmaNlosDb(shadowSigmaNlosDb), firstTapKDb(firstTapKDb),
    taps(taps), clusters(clusters), components(components)
{
    validate();
}

TgnModel TgnChannelProfile::parseModel(const char *name)
{
    if (!strcmp(name, "A")) return TgnModel::A;
    if (!strcmp(name, "B")) return TgnModel::B;
    if (!strcmp(name, "C")) return TgnModel::C;
    if (!strcmp(name, "D")) return TgnModel::D;
    if (!strcmp(name, "E")) return TgnModel::E;
    if (!strcmp(name, "F")) return TgnModel::F;
    throw cRuntimeError("Unknown TGn channel profile '%s'", name);
}

TgnCondition TgnChannelProfile::parseCondition(const char *name)
{
    if (!strcmp(name, "nlos")) return TgnCondition::NLOS;
    if (!strcmp(name, "los")) return TgnCondition::LOS;
    throw cRuntimeError("Unknown TGn channel condition '%s'", name);
}

const char *TgnChannelProfile::getModelName(TgnModel model)
{
    switch (model) {
        case TgnModel::A: return "A";
        case TgnModel::B: return "B";
        case TgnModel::C: return "C";
        case TgnModel::D: return "D";
        case TgnModel::E: return "E";
        case TgnModel::F: return "F";
        default: throw cRuntimeError("Unknown TGn channel profile enum");
    }
}

TgnChannelProfile TgnChannelProfile::create(TgnModel model)
{
    switch (model) {
        case TgnModel::A:
            return buildProfile(model, 0, 5, 4, 0, {
                raw(1,45,40,45,40, {{0,0}})});
        case TgnModel::B:
            return buildProfile(model, 15, 5, 4, 0, {
                raw(1,4.3,14.4,225.1,14.4, {{0,0},{10,-5.4},{20,-10.8},{30,-16.2},{40,-21.7}}),
                raw(2,118.4,25.2,106.5,25.4, {{20,-3.2},{30,-6.3},{40,-9.4},{50,-12.5},{60,-15.6},{70,-18.7},{80,-21.8}})});
        case TgnModel::C:
            return buildProfile(model, 30, 5, 5, 0, {
                raw(1,290.3,24.6,13.5,24.7, {{0,0},{10,-2.1},{20,-4.3},{30,-6.5},{40,-8.6},{50,-10.8},{60,-13.0},{70,-15.2},{80,-17.3},{90,-19.5}}),
                raw(2,332.3,22.4,56.4,22.5, {{60,-5.0},{70,-7.2},{80,-9.3},{90,-11.5},{110,-13.7},{140,-15.8},{170,-18.0},{200,-20.2}})});
        case TgnModel::D:
            return buildProfile(model, 50, 10, 5, 3, {
                raw(1,158.9,27.7,332.1,27.4, {{0,0},{10,-0.9},{20,-1.7},{30,-2.6},{40,-3.5},{50,-4.3},{60,-5.2},{70,-6.1},{80,-6.9},{90,-7.8},{110,-9.0},{140,-11.1},{170,-13.7},{200,-16.3},{240,-19.3},{290,-23.2}}),
                raw(2,320.2,31.4,49.3,32.1, {{110,-6.6},{140,-9.5},{170,-12.1},{200,-14.7},{240,-17.4},{290,-21.9},{340,-25.5}}),
                raw(3,276.1,37.4,275.9,36.8, {{240,-18.8},{290,-23.2},{340,-25.2},{390,-26.7}})});
        case TgnModel::E:
            return buildProfile(model, 100, 20, 6, 6, {
                raw(1,163.7,35.8,105.6,36.1, {{0,-2.6},{10,-3.0},{20,-3.5},{30,-3.9},{50,-4.5},{80,-5.6},{110,-6.9},{140,-8.2},{180,-9.8},{230,-11.7},{280,-13.9},{330,-16.1},{380,-18.3},{430,-20.5},{490,-22.9}}),
                raw(2,251.8,41.6,293.1,42.5, {{50,-1.8},{80,-3.2},{110,-4.5},{140,-5.8},{180,-7.1},{230,-9.9},{280,-10.3},{330,-14.3},{380,-14.7},{430,-18.7},{490,-19.9},{560,-22.4}}),
                raw(3,80.0,37.4,61.9,38.0, {{180,-7.9},{230,-9.6},{280,-14.2},{330,-13.8},{380,-18.6},{430,-18.1},{490,-22.8}}),
                raw(4,182.0,40.3,275.7,38.7, {{490,-20.6},{560,-20.5},{640,-20.7},{730,-24.6}})});
        case TgnModel::F:
            return buildProfile(model, 150, 30, 6, 6, {
                raw(1,315.1,48.0,56.2,41.6, {{0,-3.3},{10,-3.6},{20,-3.9},{30,-4.2},{50,-4.6},{80,-5.3},{110,-6.2},{140,-7.1},{180,-8.2},{230,-9.5},{280,-11.0},{330,-12.5},{400,-14.3},{490,-16.7},{600,-19.9}}),
                raw(2,180.4,55.0,183.7,55.2, {{50,-1.8},{80,-2.8},{110,-3.5},{140,-4.4},{180,-5.3},{230,-7.4},{280,-7.0},{330,-10.3},{400,-10.4},{490,-13.8},{600,-15.7},{730,-19.9}}),
                raw(3,74.7,42.0,153.0,47.4, {{180,-5.7},{230,-6.7},{280,-10.4},{330,-9.6},{400,-14.1},{490,-12.7},{600,-18.5}}),
                raw(4,251.5,28.6,112.5,27.2, {{400,-8.8},{490,-13.3},{600,-18.7}}),
                raw(5,68.5,30.7,291.0,33.0, {{600,-12.9},{730,-14.2}}),
                raw(6,246.2,38.2,62.3,38.0, {{880,-16.3},{1050,-21.2}})});
        default:
            throw cRuntimeError("Unknown TGn channel profile enum");
    }
}

void TgnChannelProfile::validate() const
{
    static const int expectedComponents[] = {1, 12, 18, 27, 38, 41};
    // Derived from the exact rounded Appendix C delay/power entries above.
    // These intentionally differ slightly from the report's nominal model labels.
    static const double expectedDerivedRmsNs[] = {
        0,
        15.646634945155343,
        33.4393253316208,
        50.16260546026875,
        98.98424350857287,
        148.80372307797091
    };
    if (taps.empty() || clusters.empty() || components.empty())
        throw cRuntimeError("TGn profile %s is empty", getModelName(model));
    for (size_t i = 0; i < taps.size(); i++) {
        if (taps[i].reportTapIndex != (int)i + 1 || taps[i].excessDelay < SIMTIME_ZERO || (i && taps[i].excessDelay <= taps[i - 1].excessDelay))
            throw cRuntimeError("TGn profile %s has invalid tap ordering", getModelName(model));
    }
    for (size_t i = 0; i < clusters.size(); i++)
        if (clusters[i].reportClusterIndex != (int)i + 1 || clusters[i].receiverAngularSpreadDegrees <= 0 || clusters[i].transmitterAngularSpreadDegrees <= 0)
            throw cRuntimeError("TGn profile %s has invalid cluster data", getModelName(model));
    double normalizedPower = 0;
    for (size_t i = 0; i < components.size(); i++) {
        const auto& component = components[i];
        if (component.stableComponentIndex != (int)i || component.reportTapIndex < 1 || component.reportTapIndex > (int)taps.size() ||
            component.reportClusterIndex < 1 || component.reportClusterIndex > (int)clusters.size() || !std::isfinite(component.relativePowerDb) ||
            !std::isfinite(component.normalizedLinearPower) || component.normalizedLinearPower <= 0)
            throw cRuntimeError("TGn profile %s has invalid component data", getModelName(model));
        normalizedPower += component.normalizedLinearPower;
    }
    if ((int)components.size() != expectedComponents[(int)model] || std::abs(normalizedPower - 1) > 1e-12)
        throw cRuntimeError("TGn profile %s failed component count or normalization validation", getModelName(model));

    double meanDelay = 0;
    for (const auto& component : components)
        meanDelay += component.normalizedLinearPower * getTap(component.reportTapIndex).excessDelay.dbl();
    double variance = 0;
    for (const auto& component : components) {
        double delta = getTap(component.reportTapIndex).excessDelay.dbl() - meanDelay;
        variance += component.normalizedLinearPower * delta * delta;
    }
    double derivedRmsNs = std::sqrt(variance) * 1e9;
    if (std::abs(derivedRmsNs - expectedDerivedRmsNs[(int)model]) > 1e-9)
        throw cRuntimeError("TGn profile %s exact-table RMS delay is %g ns instead of %g ns",
            getModelName(model), derivedRmsNs, expectedDerivedRmsNs[(int)model]);
}

const TgnTap& TgnChannelProfile::getTap(int reportTapIndex) const
{
    if (reportTapIndex < 1 || reportTapIndex > (int)taps.size())
        throw cRuntimeError("Invalid TGn report tap index %d", reportTapIndex);
    return taps[reportTapIndex - 1];
}

const TgnCluster& TgnChannelProfile::getCluster(int reportClusterIndex) const
{
    if (reportClusterIndex < 1 || reportClusterIndex > (int)clusters.size())
        throw cRuntimeError("Invalid TGn report cluster index %d", reportClusterIndex);
    return clusters[reportClusterIndex - 1];
}

const TgnComponent& TgnChannelProfile::getFirstTapComponent() const
{
    for (const auto& component : components)
        if (component.reportTapIndex == 1)
            return component;
    throw cRuntimeError("TGn profile %s has no first-tap component", getModelName(model));
}

bool TgnChannelProfile::hasFluorescentEffect(const TgnComponent& component) const
{
    // INET policy: the report numbers are local within the named cluster.
    return (model == TgnModel::D && component.reportClusterIndex == 2 &&
            (component.reportTapIndex == 12 || component.reportTapIndex == 14 || component.reportTapIndex == 16)) ||
           (model == TgnModel::E && component.reportClusterIndex == 1 &&
            (component.reportTapIndex == 3 || component.reportTapIndex == 5 || component.reportTapIndex == 7));
}

bool TgnChannelProfile::hasVehicleEffect(const TgnComponent& component) const
{
    return model == TgnModel::F && component.reportClusterIndex == 1 && component.reportTapIndex == 3;
}

} // namespace physicallayer
} // namespace inet
