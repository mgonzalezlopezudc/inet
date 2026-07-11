// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_HEDYNAMICFRAGMENTATIONPOLICY_H
#define __INET_HEDYNAMICFRAGMENTATIONPOLICY_H
#include "inet/linklayer/ieee80211/mac/fragmentation/BasicFragmentationPolicy.h"
namespace inet { namespace ieee80211 {
class INET_API HeDynamicFragmentationPolicy : public BasicFragmentationPolicy {
  protected: int requiredLevel = 1;
  protected: virtual void initialize() override;
  public: virtual std::vector<int> computeFragmentSizes(Packet *frame) override;
};
} }
#endif
