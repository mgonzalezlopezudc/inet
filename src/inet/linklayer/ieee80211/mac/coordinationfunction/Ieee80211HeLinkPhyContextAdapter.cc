//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Ieee80211HeLinkPhyContextAdapter.h"

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

namespace inet {
namespace ieee80211 {

Ieee80211HeLinkPhySnapshot Ieee80211HeLinkPhyContextAdapter::getSnapshot() const
{
    auto nic = getContainingNicModule(owner);
    auto radio = check_and_cast<const physicallayer::IRadio *>(nic->getSubmodule("radio"));
    auto transmitter = check_and_cast<const physicallayer::Ieee80211Transmitter *>(radio->getTransmitter());
    auto receiver = check_and_cast<const physicallayer::FlatReceiverBase *>(radio->getReceiver());
    auto channel = transmitter->getChannel();
    auto activeMode = transmitter->getMode();
    if (channel == nullptr || activeMode == nullptr)
        throw cRuntimeError("HE planning requires an active IEEE 802.11 channel and mode");
    auto bandwidth = activeMode->getDataMode()->getBandwidth();
    auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(activeMode);
    if (heMode == nullptr) {
        auto modeSet = transmitter->getModeSet();
        auto matchingMode = modeSet == nullptr ? nullptr : modeSet->findHeMode(0, 1, bandwidth, bandwidth > MHz(20));
        heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(matchingMode);
    }
    if (heMode == nullptr)
        throw cRuntimeError("HE planning requires an HE mode matching the active channel bandwidth");
    physicallayer::Ieee80211HeGuardInterval guardInterval;
    switch (heMode->getDataMode()->getGuardIntervalType()) {
        case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT: guardInterval = physicallayer::HE_GI_0_8_US; break;
        case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM: guardInterval = physicallayer::HE_GI_1_6_US; break;
        case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG: guardInterval = physicallayer::HE_GI_3_2_US; break;
        default: throw cRuntimeError("Unsupported active HE guard interval");
    }
    auto puncturedSubchannels = resolveHePreamblePuncturing(owner, bandwidth);
    uint8_t puncturedSubchannelMask = 0;
    for (size_t i = 0; i < puncturedSubchannels.size(); ++i)
        if (puncturedSubchannels[i])
            puncturedSubchannelMask |= 1U << i;
    auto mib = mac->getMib();
    if (mib == nullptr)
        throw cRuntimeError("HE planning requires an initialized IEEE 802.11 MIB");
    return Ieee80211HeLinkPhySnapshot(channel->getChannelNumber(), channel->getCenterFrequency(), bandwidth,
            transmitter->getPower(), transmitter->getMaxPower(), receiver->getSensitivity(),
            owner->par("receiverNoiseFigure").doubleValue(), radio->getAntenna()->getNumAntennas(),
            guardInterval, physicallayer::getHeDefaultLtfType(guardInterval),
            mib->heOperation.defaultPeDurationUs, puncturedSubchannels, puncturedSubchannelMask,
            mib->localHeCapabilities);
}

Ieee80211HePeerLinkSnapshot Ieee80211HeLinkPhyContextAdapter::getPeerSnapshot(
        const MacAddress& address, simtime_t maximumLinkEstimateAge) const
{
    auto mib = mac->getMib();
    if (mib == nullptr)
        throw cRuntimeError("HE peer projection requires an initialized IEEE 802.11 MIB");
    auto capabilities = mib->getPeerCapabilitySnapshot(address);
    auto advertisement = capabilities.getAdvertisedHe();
    auto negotiated = capabilities.getNegotiatedHe();
    auto link = mib->getPeerLinkSnapshot(address);
    auto pathLossDb = !link ? NaN : link->getPathLossDb();
    auto hasFreshPathLoss = link && link->isValid() && simTime() - link->getLastUpdate() <= maximumLinkEstimateAge;
    return Ieee80211HePeerLinkSnapshot(advertisement.has_value(),
            advertisement.value_or(Ieee80211HeCapabilities()), negotiated.has_value(),
            negotiated.value_or(Ieee80211NegotiatedHeCapabilities()), pathLossDb, hasFreshPathLoss);
}

} // namespace ieee80211
} // namespace inet
