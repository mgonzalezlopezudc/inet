//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHCFFEATURESET_H
#define __INET_IHCFFEATURESET_H

#include <functional>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"

namespace inet {
namespace ieee80211 {

class IHcfExchangeProvider;
class HcfContext;
class HePeerStateService;
class HeQueueService;
class HeDlMuExchangeProvider;
class HeTriggeredUlExchangeService;
class HeSoundingService;
enum class HcfExchangeClass;

enum class HcfAmendmentRuntimeKind {
    COMMON,
    VHT,
    HE,
};

struct INET_API HcfHeRuntimeServices
{
    HePeerStateService *peerStateService = nullptr;
    HeQueueService *queueService = nullptr;
    HeDlMuExchangeProvider *dlMuExchangeProvider = nullptr;
    HeTriggeredUlExchangeService *triggeredUlExchangeService = nullptr;
    HeSoundingService *soundingService = nullptr;

    bool isComplete() const
    {
        return peerStateService != nullptr && queueService != nullptr &&
                dlMuExchangeProvider != nullptr && triggeredUlExchangeService != nullptr &&
                soundingService != nullptr;
    }
};

/** Immutable configuration projected by the HCF facade into its feature set. */
struct INET_API HcfFeatureConfiguration
{
    bool enableHtSounding = false;
    bool enableVhtSuBeamforming = false;
    bool enableVhtDlMuMimo = false;
    bool enableHeUlMuOfdma = false;
    bool enableHeDlMuMimo = false;
};

/**
 * Non-owning provider reference whose lifetime is owned by the feature set
 * module. Every descriptor is executable and participates in selection.
 */
class INET_API HcfExchangeProviderDescriptor
{
  private:
    HcfExchangeClass exchangeClass;
    IHcfExchangeProvider *provider = nullptr;

  public:
    HcfExchangeProviderDescriptor(HcfExchangeClass exchangeClass,
            IHcfExchangeProvider *provider = nullptr) :
        exchangeClass(exchangeClass), provider(provider) {}

    HcfExchangeClass getExchangeClass() const { return exchangeClass; }
    bool isExecutable() const { return provider != nullptr; }
    /**
     * Returns the executable provider. Invalid compositions are rejected
     * instead of exposing a null pointer.
     */
    IHcfExchangeProvider& getExecutableProvider() const
    {
        if (provider == nullptr)
            throw cRuntimeError("HCF exchange descriptor is inert and has no executable provider");
        return *provider;
    }
};

/** NED-paired composition contract for the HCF exchange provider set. */
class INET_API IHcfFeatureSet
{
  public:
    using ExchangeCommitter = std::function<void(HcfExchangeClass, const HcfContext&)>;

    virtual ~IHcfFeatureSet() {}

    /**
     * Returns descriptors by value. Provider pointers are non-owning and stay
     * valid only while this OMNeT++ feature-set module and its children live.
     */
    virtual std::vector<HcfExchangeProviderDescriptor> getExchangeProviderDescriptors() = 0;
    virtual void configureFeatures(const HcfFeatureConfiguration&) {}
    virtual void configureExchangeCommitter(const ExchangeCommitter& committer) = 0;
    virtual HcfAmendmentRuntimeKind getAmendmentRuntimeKind() const
        { return HcfAmendmentRuntimeKind::COMMON; }
    virtual HcfHeRuntimeServices getHeRuntimeServices() { return {}; }
};

} // namespace ieee80211
} // namespace inet

#endif
