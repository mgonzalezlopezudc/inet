//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHCFFEATURESET_H
#define __INET_IHCFFEATURESET_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"

namespace inet {
namespace ieee80211 {

class HePeerStateService;
class HeQueueService;
class HeDlMuExchangeProvider;
class HeTriggeredUlExchangeService;
class HeSoundingService;

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

/** NED-paired composition contract for HCF amendment services. */
class INET_API IHcfFeatureSet
{
  public:
    virtual ~IHcfFeatureSet() {}
    virtual void configureFeatures(const HcfFeatureConfiguration&) {}
    virtual HcfAmendmentRuntimeKind getAmendmentRuntimeKind() const
        { return HcfAmendmentRuntimeKind::COMMON; }
    virtual HcfHeRuntimeServices getHeRuntimeServices() { return {}; }
};

} // namespace ieee80211
} // namespace inet

#endif
