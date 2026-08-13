//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFRUNTIME_H
#define __INET_HCFRUNTIME_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/IHcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeSelector.h"

namespace inet {
namespace ieee80211 {

/**
 * Read-only HCF composition root. It owns no OMNeT++ module or provider;
 * Hcf destroys it before the feature-set submodule teardown begins.
 */
class INET_API HcfRuntime
{
  private:
    IHcfFeatureSet *featureSet;
    std::vector<HcfExchangeProviderDescriptor> exchangeProviderDescriptors;
    std::unique_ptr<HcfExchangeSelector> exchangeSelector;

  public:
    explicit HcfRuntime(IHcfFeatureSet *featureSet,
            const HcfFeatureConfiguration& configuration = {});

    const std::vector<HcfExchangeProviderDescriptor>& getExchangeProviderDescriptors() const { return exchangeProviderDescriptors; }
    const HcfExchangeProviderDescriptor& getSingleUserDescriptor() const;
    const HcfExchangeProviderDescriptor *findExchangeProviderDescriptor(HcfExchangeClass exchangeClass) const;
    HcfExchangeSelector& getExchangeSelector() { return *exchangeSelector; }
    const HcfExchangeSelector& getExchangeSelector() const { return *exchangeSelector; }

    /** Rejects composition-only descriptors before exchange selection is enabled. */
    void validateExecutableProviders() const;
    void configureExchangeCommitter(
            const IHcfFeatureSet::ExchangeCommitter& committer);
};

} // namespace ieee80211
} // namespace inet

#endif
