//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFFEATURESET_H
#define __INET_HCFFEATURESET_H

#include <memory>

#include "inet/linklayer/ieee80211/mac/contract/IHcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePeerStateService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeQueueService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingService.h"

namespace inet {
namespace ieee80211 {

/** Composition-only common HCF feature set. */
class INET_API CommonHcfFeatureSet : public cSimpleModule, public IHcfFeatureSet
{
  private:
    HcfFeatureConfiguration configuration;

  protected:
    const HcfFeatureConfiguration& getConfiguration() const { return configuration; }
  public:
    virtual void configureFeatures(const HcfFeatureConfiguration& configuration) override
        { this->configuration = configuration; }
};

/** Composition-only VHT HCF feature set. */
class INET_API VhtHcfFeatureSet : public CommonHcfFeatureSet
{
  public:
    virtual HcfAmendmentRuntimeKind getAmendmentRuntimeKind() const override
        { return HcfAmendmentRuntimeKind::VHT; }
};

/** Composition-only HE HCF feature set. */
class INET_API HeHcfFeatureSet : public CommonHcfFeatureSet
{
  private:
    HePeerStateService peerStateService;
    HeQueueService queueService;
    HeDlMuExchangeProvider dlMuExchangeProvider;
    HeTriggeredUlExchangeService triggeredUlExchangeService;
    HeSoundingService soundingService;

  public:
    virtual ~HeHcfFeatureSet();
    virtual HcfAmendmentRuntimeKind getAmendmentRuntimeKind() const override
        { return HcfAmendmentRuntimeKind::HE; }
    virtual HcfHeRuntimeServices getHeRuntimeServices() override
        { return {&peerStateService, &queueService, &dlMuExchangeProvider,
                &triggeredUlExchangeService, &soundingService}; }
    HePeerStateService& getPeerStateService() { return peerStateService; }
    const HePeerStateService& getPeerStateService() const { return peerStateService; }
    HeQueueService& getQueueService() { return queueService; }
    const HeQueueService& getQueueService() const { return queueService; }
    HeDlMuExchangeProvider& getDlMuExchangeProvider() { return dlMuExchangeProvider; }
    HeTriggeredUlExchangeService& getTriggeredUlExchangeService() { return triggeredUlExchangeService; }
    HeSoundingService& getSoundingService() { return soundingService; }
};

} // namespace ieee80211
} // namespace inet

#endif
