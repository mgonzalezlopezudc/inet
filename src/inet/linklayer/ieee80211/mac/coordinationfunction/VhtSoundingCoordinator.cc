//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtSoundingCoordinator.h"

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

Define_Module(VhtSoundingCoordinator);

void VhtSoundingCoordinator::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        retryInterval = par("retryInterval");
        WATCH(ndpAnnouncementAccepted);
        WATCH(dialogToken);
        WATCH(associationId);
    }
}

bool VhtSoundingCoordinator::mayAttempt(const MacAddress& peer) const
{
    auto it = nextAttemptTimes.find(peer);
    return it == nextAttemptTimes.end() || simTime() >= it->second;
}

void VhtSoundingCoordinator::recordAttempt(const MacAddress& peer)
{
    nextAttemptTimes[peer] = simTime() + retryInterval;
}

void VhtSoundingCoordinator::reset()
{
    ndpAnnouncementAccepted = false;
    soundingAccessPoint = MacAddress::UNSPECIFIED_ADDRESS;
    dialogToken = 0;
    associationId = 0;
    channelWidth = Hz(0);
    soundingTransmitterRadioId = -1;
    ndpaReceptionEnd = -1;
}

void VhtSoundingCoordinator::invalidatePeer(const MacAddress& peer)
{
    nextAttemptTimes.erase(peer);
    if (soundingAccessPoint == peer)
        reset();
}

bool VhtSoundingCoordinator::processNdpAnnouncement(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header, Ieee80211Mac *mac,
        bool enabled, Hz operatingWidth)
{
    auto ndpa = dynamicPtrCast<const Ieee80211VhtNdpAnnouncementFrame>(header);
    if (ndpa == nullptr)
        return false;
    reset();
    auto mib = mac->getMib();
    const bool associatedAp = enabled && mib->bssStationData.isAssociated &&
            !mib->bssData.bssid.isUnspecified() &&
            ndpa->getTransmitterAddress() == mib->bssData.bssid &&
            ndpa->getReceiverAddress() == mac->getAddress() &&
            operatingWidth == MHz(20);
    bool matchingAid = false;
    for (size_t i = 0; associatedAp && i < ndpa->getStationsArraySize(); i++)
        matchingAid |= ndpa->getStations(i).aid == mib->bssStationData.associationId &&
                !ndpa->getStations(i).muFeedback;
    auto negotiated = mib->findNegotiatedVhtCapabilities(ndpa->getTransmitterAddress());
    auto provenance = packet->findTag<physicallayer::Ieee80211PhyProvenanceInd>();
    const bool capable = negotiated != nullptr && negotiated->localRxPeerTx.valid &&
            negotiated->localRxPeerTx.suBeamforming &&
            negotiated->localRxPeerTx.soundingNsts >= 2;
    if (associatedAp && matchingAid && capable && provenance != nullptr &&
            provenance->getTransmitterRadioId() >= 0) {
        ndpAnnouncementAccepted = true;
        soundingAccessPoint = ndpa->getTransmitterAddress();
        dialogToken = ndpa->getSoundingDialogTokenNumber();
        associationId = mib->bssStationData.associationId;
        channelWidth = operatingWidth;
        soundingTransmitterRadioId = provenance->getTransmitterRadioId();
        ndpaReceptionEnd = provenance->getEndTime();
    }
    delete packet;
    return true;
}

bool VhtSoundingCoordinator::processHeaderlessNdp(Packet *packet,
        Ieee80211Mac *mac, physicallayer::Ieee80211ModeSet *modeSet, ITx *tx,
        ITx::ICallback *callback, bool enabled)
{
    auto indication = packet->findTag<physicallayer::Ieee80211NdpInd>();
    if (indication == nullptr || indication->getPhyFormat() !=
            physicallayer::IEEE80211_NDP_PHY_VHT)
        return false;
    auto provenance = packet->findTag<physicallayer::Ieee80211PhyProvenanceInd>();
    auto expectedNdpStart = modeSet == nullptr ? SIMTIME_ZERO :
            ndpaReceptionEnd + modeSet->getSifsTime();
    bool valid = enabled && ndpAnnouncementAccepted && modeSet != nullptr && provenance != nullptr &&
            provenance->getTransmitterRadioId() == soundingTransmitterRadioId &&
            provenance->getStartTime() - expectedNdpStart >= -SimTime::fromRaw(1) &&
            provenance->getStartTime() - expectedNdpStart <= SimTime::fromRaw(1) &&
            Hz(indication->getChannelWidth()) == channelWidth &&
            indication->getNumberOfSpaceTimeStreams() >= 2;
    auto mib = mac->getMib();
    valid &= mib->bssStationData.isAssociated &&
            mib->bssData.bssid == soundingAccessPoint &&
            mib->bssStationData.associationId == associationId;
    if (!valid)
        EV_INFO << "Rejecting VHT NDP: enabled=" << enabled
                << ", ndpaAccepted=" << ndpAnnouncementAccepted
                << ", provenance=" << (provenance != nullptr)
                << ", transmitterRadio=" << (provenance == nullptr ? -1 : provenance->getTransmitterRadioId())
                << ", expectedTransmitterRadio=" << soundingTransmitterRadioId
                << ", start=" << (provenance == nullptr ? SIMTIME_ZERO : provenance->getStartTime())
                << ", expectedStart=" << expectedNdpStart
                << ", width=" << Hz(indication->getChannelWidth())
                << ", expectedWidth=" << channelWidth
                << ", nsts=" << indication->getNumberOfSpaceTimeStreams() << EV_ENDL;
    if (valid) {
        auto feedback = makeShared<Ieee80211VhtCompressedBeamformingFeedback>();
        feedback->setSoundingDialogTokenNumber(dialogToken);
        feedback->setAverageSnr(0);
        for (size_t i = 0; i < feedback->getCompressedBeamformingReportArraySize(); i++)
            feedback->setCompressedBeamformingReport(i, 0);
        auto header = makeShared<Ieee80211ActionFrame>();
        header->setType(ST_NOACKACTION);
        header->setCategory(21);
        header->setReceiverAddress(soundingAccessPoint);
        header->setTransmitterAddress(mac->getAddress());
        header->setAddress3(soundingAccessPoint);
        auto response = new Packet("VHT-Compressed-Beamforming", header);
        response->insertAtBack(feedback);
        response->insertAtBack(makeShared<Ieee80211MacTrailer>());
        response->addTag<physicallayer::Ieee80211ModeReq>()->setMode(
                modeSet->getSlowestMandatoryMode(MHz(20)));
        tx->transmitFrame(response, header, modeSet->getSifsTime(), callback);
        delete response;
    }
    reset();
    delete packet;
    return true;
}

} // namespace ieee80211
} // namespace inet
