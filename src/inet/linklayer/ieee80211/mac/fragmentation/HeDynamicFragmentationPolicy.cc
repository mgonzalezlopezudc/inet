// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/ieee80211/mac/fragmentation/HeDynamicFragmentationPolicy.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
namespace inet { namespace ieee80211 {
Define_Module(HeDynamicFragmentationPolicy);
void HeDynamicFragmentationPolicy::initialize() { BasicFragmentationPolicy::initialize(); requiredLevel = par("requiredLevel"); if (requiredLevel < 1 || requiredLevel > 3) throw cRuntimeError("requiredLevel must be 1..3"); }
std::vector<int> HeDynamicFragmentationPolicy::computeFragmentSizes(Packet *frame) {
    auto header = dynamicPtrCast<const Ieee80211DataHeader>(frame->peekAtFront<Ieee80211MacHeader>());
    auto mac = getParentModule() && getParentModule()->getParentModule() ? getParentModule()->getParentModule()->getParentModule() : nullptr;
    auto mib = mac ? dynamic_cast<Ieee80211Mib *>(mac->getSubmodule("mib")) : nullptr;
    auto negotiated = header != nullptr && mib != nullptr ? mib->findNegotiatedHeCapabilities(header->getReceiverAddress()) : nullptr;
    if (negotiated == nullptr || !negotiated->valid || negotiated->intersection.dynamicFragmentationLevel < requiredLevel) {
        EV_INFO << "HE dynamic fragmentation suppressed: peer did not negotiate level " << requiredLevel << endl;
        return {};
    }
    return BasicFragmentationPolicy::computeFragmentSizes(frame);
}
} }
