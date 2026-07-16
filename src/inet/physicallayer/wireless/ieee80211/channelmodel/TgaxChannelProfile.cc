//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <utility>

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxChannelProfile.h"

namespace inet {

namespace physicallayer {

namespace {

using Model = TgaxChannelProfile::Model;
using Component = TgaxChannelProfile::Component;

const std::vector<Component>& getBaseComponents(Model model)
{
    // IEEE 802.11-03/940r4, Appendix C. Powers are the printed relative
    // per-cluster values and intentionally retain their original precision.
    static const std::vector<Component> modelA = {
        {1, 0, 0.0, 0},
    };
    static const std::vector<Component> modelB = {
        {1, 0, 0.0, 0}, {1, 10, -5.4, 0}, {1, 20, -10.8, 0}, {1, 30, -16.2, 0}, {1, 40, -21.7, 0},
        {2, 20, -3.2, 0}, {2, 30, -6.3, 0}, {2, 40, -9.4, 0}, {2, 50, -12.5, 0}, {2, 60, -15.6, 0}, {2, 70, -18.7, 0}, {2, 80, -21.8, 0},
    };
    static const std::vector<Component> modelC = {
        {1, 0, 0.0, 0}, {1, 10, -2.1, 0}, {1, 20, -4.3, 0}, {1, 30, -6.5, 0}, {1, 40, -8.6, 0},
        {1, 50, -10.8, 0}, {1, 60, -13.0, 0}, {1, 70, -15.2, 0}, {1, 80, -17.3, 0}, {1, 90, -19.5, 0},
        {2, 60, -5.0, 0}, {2, 70, -7.2, 0}, {2, 80, -9.3, 0}, {2, 90, -11.5, 0},
        {2, 110, -13.7, 0}, {2, 140, -15.8, 0}, {2, 170, -18.0, 0}, {2, 200, -20.2, 0},
    };
    static const std::vector<Component> modelD = {
        {1, 0, 0.0, 0}, {1, 10, -0.9, 0}, {1, 20, -1.7, 0}, {1, 30, -2.6, 0}, {1, 40, -3.5, 0},
        {1, 50, -4.3, 0}, {1, 60, -5.2, 0}, {1, 70, -6.1, 0}, {1, 80, -6.9, 0}, {1, 90, -7.8, 0},
        {1, 110, -9.0, 0}, {1, 140, -11.1, 0}, {1, 170, -13.7, 0}, {1, 200, -16.3, 0},
        {1, 240, -19.3, 0}, {1, 290, -23.2, 0},
        {2, 110, -6.6, 0}, {2, 140, -9.5, 0}, {2, 170, -12.1, 0}, {2, 200, -14.7, 0},
        {2, 240, -17.4, 0}, {2, 290, -21.9, 0}, {2, 340, -25.5, 0},
        {3, 240, -18.8, 0}, {3, 290, -23.2, 0}, {3, 340, -25.2, 0}, {3, 390, -26.7, 0},
    };
    static const std::vector<Component> modelE = {
        {1, 0, -2.6, 0}, {1, 10, -3.0, 0}, {1, 20, -3.5, 0}, {1, 30, -3.9, 0}, {1, 50, -4.5, 0},
        {1, 80, -5.6, 0}, {1, 110, -6.9, 0}, {1, 140, -8.2, 0}, {1, 180, -9.8, 0}, {1, 230, -11.7, 0},
        {1, 280, -13.9, 0}, {1, 330, -16.1, 0}, {1, 380, -18.3, 0}, {1, 430, -20.5, 0}, {1, 490, -22.9, 0},
        {2, 50, -1.8, 0}, {2, 80, -3.2, 0}, {2, 110, -4.5, 0}, {2, 140, -5.8, 0}, {2, 180, -7.1, 0},
        {2, 230, -9.9, 0}, {2, 280, -10.3, 0}, {2, 330, -14.3, 0}, {2, 380, -14.7, 0},
        {2, 430, -18.7, 0}, {2, 490, -19.9, 0}, {2, 560, -22.4, 0},
        {3, 180, -7.9, 0}, {3, 230, -9.6, 0}, {3, 280, -14.2, 0}, {3, 330, -13.8, 0},
        {3, 380, -18.6, 0}, {3, 430, -18.1, 0}, {3, 490, -22.8, 0},
        {4, 490, -20.6, 0}, {4, 560, -20.5, 0}, {4, 640, -20.7, 0}, {4, 730, -24.6, 0},
    };
    static const std::vector<Component> modelF = {
        {1, 0, -3.3, 0}, {1, 10, -3.6, 0}, {1, 20, -3.9, 0}, {1, 30, -4.2, 0}, {1, 50, -4.6, 0},
        {1, 80, -5.3, 0}, {1, 110, -6.2, 0}, {1, 140, -7.1, 0}, {1, 180, -8.2, 0}, {1, 230, -9.5, 0},
        {1, 280, -11.0, 0}, {1, 330, -12.5, 0}, {1, 400, -14.3, 0}, {1, 490, -16.7, 0}, {1, 600, -19.9, 0},
        {2, 50, -1.8, 0}, {2, 80, -2.8, 0}, {2, 110, -3.5, 0}, {2, 140, -4.4, 0}, {2, 180, -5.3, 0},
        {2, 230, -7.4, 0}, {2, 280, -7.0, 0}, {2, 330, -10.3, 0}, {2, 400, -10.4, 0},
        {2, 490, -13.8, 0}, {2, 600, -15.7, 0}, {2, 730, -19.9, 0},
        {3, 180, -5.7, 0}, {3, 230, -6.7, 0}, {3, 280, -10.4, 0}, {3, 330, -9.6, 0},
        {3, 400, -14.1, 0}, {3, 490, -12.7, 0}, {3, 600, -18.5, 0},
        {4, 400, -8.8, 0}, {4, 490, -13.3, 0}, {4, 600, -18.7, 0},
        {5, 600, -12.9, 0}, {5, 730, -14.2, 0},
        {6, 880, -16.3, 0}, {6, 1050, -21.2, 0},
    };

    switch (model) {
        case Model::A: return modelA;
        case Model::B: return modelB;
        case Model::C: return modelC;
        case Model::D: return modelD;
        case Model::E: return modelE;
        case Model::F: return modelF;
        default: throw cRuntimeError("Invalid TGax channel model");
    }
}

int computeExpansionFactor(Hz bandwidth)
{
    if (!std::isfinite(bandwidth.get<MHz>()) || bandwidth <= Hz(0) || bandwidth > MHz(160))
        throw cRuntimeError("Unsupported TGax indoor channel bandwidth: %g MHz (expected 0 < bandwidth <= 160 MHz)", bandwidth.get<MHz>());
    else if (bandwidth <= MHz(40))
        return 1;
    else if (bandwidth <= MHz(80))
        return 2;
    else
        return 4;
}

std::vector<Component> expandComponents(const std::vector<Component>& baseComponents, int expansionFactor)
{
    // IEEE 802.11-09/0308r12, Section 2 and Appendix B.3.3: insert
    // k - 1 independently realized taps after each non-final cluster tap,
    // interpolating its relative power linearly in dB.
    if (expansionFactor == 1 || baseComponents.size() == 1)
        return baseComponents;

    std::vector<Component> result;
    auto maximumClusterIndex = std::max_element(baseComponents.begin(), baseComponents.end(),
            [] (const auto& left, const auto& right) { return left.clusterIndex < right.clusterIndex; })->clusterIndex;
    auto insertedTapSpacingNs = 10.0 / expansionFactor;
    for (int clusterIndex = 1; clusterIndex <= maximumClusterIndex; clusterIndex++) {
        std::vector<Component> clusterComponents;
        std::copy_if(baseComponents.begin(), baseComponents.end(), std::back_inserter(clusterComponents),
                [clusterIndex] (const auto& component) { return component.clusterIndex == clusterIndex; });
        std::sort(clusterComponents.begin(), clusterComponents.end(),
                [] (const auto& left, const auto& right) { return left.excessDelayNs < right.excessDelayNs; });
        for (size_t i = 0; i < clusterComponents.size(); i++) {
            const auto& component = clusterComponents[i];
            result.push_back(component);
            if (i + 1 < clusterComponents.size()) {
                const auto& nextComponent = clusterComponents[i + 1];
                for (int j = 1; j < expansionFactor; j++) {
                    auto excessDelayNs = component.excessDelayNs + j * insertedTapSpacingNs;
                    auto interpolationRatio = (excessDelayNs - component.excessDelayNs) /
                            (nextComponent.excessDelayNs - component.excessDelayNs);
                    auto powerDb = component.powerDb + interpolationRatio * (nextComponent.powerDb - component.powerDb);
                    result.push_back({clusterIndex, excessDelayNs, powerDb, 0});
                }
            }
        }
    }
    std::sort(result.begin(), result.end(), [] (const auto& left, const auto& right) {
        if (left.excessDelayNs != right.excessDelayNs)
            return left.excessDelayNs < right.excessDelayNs;
        else
            return left.clusterIndex < right.clusterIndex;
    });
    return result;
}

void normalizeComponents(std::vector<Component>& components)
{
    double totalPower = 0;
    for (const auto& component : components)
        totalPower += pow(10.0, component.powerDb / 10.0);
    for (auto& component : components)
        component.normalizedPower = pow(10.0, component.powerDb / 10.0) / totalPower;
}

} // namespace

TgaxChannelProfile::TgaxChannelProfile(Model model, int expansionFactor, std::vector<Component> components) :
    model(model),
    expansionFactor(expansionFactor),
    components(std::move(components))
{
}

TgaxChannelProfile TgaxChannelProfile::create(Model model, Hz bandwidth)
{
    auto expansionFactor = computeExpansionFactor(bandwidth);
    auto components = expandComponents(getBaseComponents(model), expansionFactor);
    normalizeComponents(components);
    return TgaxChannelProfile(model, expansionFactor, std::move(components));
}

TgaxChannelProfile TgaxChannelProfile::create(const char *model, Hz bandwidth)
{
    if (model == nullptr)
        throw cRuntimeError("TGax channel model must not be null");
    else if (!strcmp(model, "A"))
        return create(Model::A, bandwidth);
    else if (!strcmp(model, "B"))
        return create(Model::B, bandwidth);
    else if (!strcmp(model, "C"))
        return create(Model::C, bandwidth);
    else if (!strcmp(model, "D"))
        return create(Model::D, bandwidth);
    else if (!strcmp(model, "E"))
        return create(Model::E, bandwidth);
    else if (!strcmp(model, "F"))
        return create(Model::F, bandwidth);
    else
        throw cRuntimeError("Unknown TGax channel model: '%s' (expected A, B, C, D, E, or F)", model);
}

double TgaxChannelProfile::computeRmsDelaySpreadNs() const
{
    double meanDelayNs = 0;
    for (const auto& component : components)
        meanDelayNs += component.excessDelayNs * component.normalizedPower;
    double varianceNs2 = 0;
    for (const auto& component : components) {
        auto differenceNs = component.excessDelayNs - meanDelayNs;
        varianceNs2 += differenceNs * differenceNs * component.normalizedPower;
    }
    return sqrt(varianceNs2);
}

} // namespace physicallayer

} // namespace inet
