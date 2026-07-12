// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/ieee80211/mac/fragmentation/HeDynamicFragmentationPolicy.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/common/MacAddressTag_m.h"

namespace inet { namespace ieee80211 {

Define_Module(HeDynamicFragmentationPolicy);

void HeDynamicFragmentationPolicy::initialize() { BasicFragmentationPolicy::initialize(); requiredLevel = par("requiredLevel"); if (requiredLevel < 1 || requiredLevel > 3) throw cRuntimeError("requiredLevel must be 1..3"); }

std::vector<int> HeDynamicFragmentationPolicy::computeFragmentSizes(Packet *frame) {
    auto header = dynamicPtrCast<const Ieee80211DataHeader>(frame->peekAtFront<Ieee80211MacHeader>());
    auto mac = getParentModule() && getParentModule()->getParentModule() && getParentModule()->getParentModule()->getParentModule() ?
        dynamic_cast<Ieee80211Mac *>(getParentModule()->getParentModule()->getParentModule()) : nullptr;
    auto mib = mac ? mac->getMib() : nullptr;

    MacAddress receiverAddress = MacAddress::UNSPECIFIED_ADDRESS;
    if (header != nullptr) {
        receiverAddress = header->getReceiverAddress();
    }
    else if (mib != nullptr) {
        if (mib->bssStationData.isAssociated && !mib->bssData.bssid.isUnspecified()) {
            receiverAddress = mib->bssData.bssid;
        }
        else {
            auto req = frame->findTag<MacAddressReq>();
            if (req != nullptr) {
                receiverAddress = req->getDestAddress();
            }
        }
    }

    auto negotiated = !receiverAddress.isUnspecified() && mib != nullptr ? mib->findNegotiatedHeCapabilities(receiverAddress) : nullptr;

    if (negotiated == nullptr || !negotiated->valid || negotiated->intersection.dynamicFragmentationLevel < requiredLevel) {
        EV_INFO << "HE dynamic fragmentation suppressed: peer did not negotiate level " << requiredLevel << endl;
        return {};
    }
    return BasicFragmentationPolicy::computeFragmentSizes(frame);
}
} }
