//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfRuntime.h"

#include <map>

#include "inet/linklayer/ieee80211/mac/contract/IHcfExchangeProvider.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h"

namespace inet {
namespace ieee80211 {

HcfRuntime::HcfRuntime(IHcfFeatureSet *featureSet,
        const HcfFeatureConfiguration& configuration) : featureSet(featureSet)
{
    if (featureSet == nullptr)
        throw cRuntimeError("HCF featureSet submodule does not implement IHcfFeatureSet");
    featureSet->configureFeatures(configuration);

    std::map<HcfExchangeClass, HcfExchangeProviderDescriptor> descriptorsByClass;
    for (const auto& descriptor : featureSet->getExchangeProviderDescriptors()) {
        if (!isTransactionalExchangeClass(descriptor.getExchangeClass()))
            throw cRuntimeError("HCF feature set cannot register non-transactional exchange class %d",
                    static_cast<int>(descriptor.getExchangeClass()));
        if (descriptor.isExecutable() &&
                descriptor.getExecutableProvider().getExchangeClass() != descriptor.getExchangeClass())
            throw cRuntimeError("HCF provider class does not match its descriptor class");
        auto inserted = descriptorsByClass.emplace(descriptor.getExchangeClass(), descriptor);
        if (!inserted.second)
            throw cRuntimeError("Duplicate HCF exchange descriptor for class %d",
                    static_cast<int>(descriptor.getExchangeClass()));
    }
    for (auto exchangeClass : getHcfExchangeClassOrder()) {
        auto it = descriptorsByClass.find(exchangeClass);
        if (it != descriptorsByClass.end()) {
            exchangeProviderDescriptors.push_back(it->second);
            descriptorsByClass.erase(it);
        }
    }
    if (!descriptorsByClass.empty())
        throw cRuntimeError("HCF feature set returned an unknown exchange class");
    exchangeSelector = std::make_unique<HcfExchangeSelector>(exchangeProviderDescriptors);
}

const HcfExchangeProviderDescriptor *HcfRuntime::findExchangeProviderDescriptor(
        HcfExchangeClass exchangeClass) const
{
    for (const auto& descriptor : exchangeProviderDescriptors)
        if (descriptor.getExchangeClass() == exchangeClass)
            return &descriptor;
    return nullptr;
}

void HcfRuntime::validateExecutableProviders() const
{
    for (const auto& descriptor : exchangeProviderDescriptors)
        if (!descriptor.isExecutable())
            throw cRuntimeError("HCF exchange class %d is composition-only and has no executable provider",
                    static_cast<int>(descriptor.getExchangeClass()));
}

void HcfRuntime::configureExchangeCommitter(
        const IHcfFeatureSet::ExchangeCommitter& committer)
{
    featureSet->configureExchangeCommitter(committer);
}

} // namespace ieee80211
} // namespace inet
