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
        if (mib->isAssociated() && !mib->getBssid().isUnspecified()) {
            receiverAddress = mib->getBssid();
        }
        else {
            auto req = frame->findTag<MacAddressReq>();
            if (req != nullptr) {
                receiverAddress = req->getDestAddress();
            }
        }
    }

    auto negotiated = !receiverAddress.isUnspecified() && mib != nullptr ? mib->getNegotiatedHeCapabilities(receiverAddress) : std::optional<Ieee80211NegotiatedHeCapabilities>();

    if (!negotiated || !negotiated->localTxPeerRx.valid ||
            negotiated->localTxPeerRx.receiverDynamicFragmentationLevel < requiredLevel) {
        EV_INFO << "HE dynamic fragmentation suppressed: peer did not negotiate level " << requiredLevel << endl;
        return {};
    }
    if (header != nullptr && header->getAMsduPresent()) {
        // IEEE Std 802.11-2024, 26.3.2 and 26.3.3 require additional
        // A-MSDU Fragmentation Support and ADDBA HE Fragmentation Operation
        // gating. These fields are not modeled, so support is treated as
        // absent and A-MSDU dynamic fragmentation is suppressed.
        EV_INFO << "HE dynamic A-MSDU fragmentation suppressed: required support and ADDBA operation are not modeled" << endl;
        return {};
    }
    // IEEE Std 802.11-2024, 26.3.2 and 26.3.3: only the negotiated,
    // directional HE dynamic-fragmentation path may fragment an MSDU.
    return computeFragmentSizesRegardlessOfAmsdu(frame);
}
} }
