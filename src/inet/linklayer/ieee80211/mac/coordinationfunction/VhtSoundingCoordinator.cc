//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtSoundingCoordinator.h"

#include <algorithm>
#include <vector>

#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/aggregation/MpduAggregation.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

Define_Module(VhtSoundingCoordinator);

static int getVhtFeedbackSubcarrierCount(Hz bandwidth, uint8_t grouping)
{
    return bandwidth == MHz(20) ? (grouping == 1 ? 52 : grouping == 2 ? 30 : 16) :
            bandwidth == MHz(40) ? (grouping == 1 ? 108 : grouping == 2 ? 58 : 30) :
            bandwidth == MHz(80) ? (grouping == 1 ? 234 : grouping == 2 ? 122 : 62) :
            (grouping == 1 ? 468 : grouping == 2 ? 244 : 124);
}

static size_t getVhtFeedbackMatrixBytes(Hz bandwidth, uint8_t nr, uint8_t nc,
        uint8_t grouping, bool feedbackTypeMu)
{
    int angleBits = feedbackTypeMu ? 6 : 3;
    int angleCount = nc * (2 * nr - nc - 1);
    return (getVhtFeedbackSubcarrierCount(bandwidth, grouping) * angleCount * angleBits + 7) / 8;
}

static size_t getVhtMuExclusiveReportBytes(Hz bandwidth, uint8_t nc, uint8_t grouping)
{
    int ns = bandwidth == MHz(20) ? (grouping == 1 ? 30 : grouping == 2 ? 16 : 10) :
            bandwidth == MHz(40) ? (grouping == 1 ? 58 : grouping == 2 ? 30 : 16) :
            bandwidth == MHz(80) ? (grouping == 1 ? 122 : grouping == 2 ? 62 : 32) :
            (grouping == 1 ? 244 : grouping == 2 ? 124 : 64);
    return (ns * nc * 4 + 7) / 8;
}

static constexpr size_t VHT_MAX_FEEDBACK_MPDU_BYTES = 3895 - 24 - 4;

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
    feedbackTypeMu = false;
    requestedNc = 1;
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
            (operatingWidth == MHz(20) || operatingWidth == MHz(40) ||
             operatingWidth == MHz(80) || operatingWidth == MHz(160));
    bool matchingAid = false;
    bool requestedMu = false;
    uint8_t ndpaNc = 1;
    for (size_t i = 0; associatedAp && i < ndpa->getStationsArraySize(); i++) {
        const auto& station = ndpa->getStations(i);
        if (station.aid == mib->bssStationData.associationId) {
            matchingAid = true;
            requestedMu = station.muFeedback;
            ndpaNc = station.ncIndex + 1;
            break;
        }
    }
    auto negotiated = mib->findNegotiatedVhtCapabilities(ndpa->getTransmitterAddress());
    auto provenance = packet->findTag<physicallayer::Ieee80211PhyProvenanceInd>();
    const bool capable = negotiated != nullptr && negotiated->localRxPeerTx.valid &&
            (requestedMu ? negotiated->localRxPeerTx.muMimo :
                           negotiated->localRxPeerTx.suBeamforming) &&
            negotiated->localRxPeerTx.soundingNsts >= 2 &&
            ndpaNc <= negotiated->localRxPeerTx.soundingNsts;
    if (associatedAp && matchingAid && capable && provenance != nullptr &&
            provenance->getTransmitterRadioId() >= 0) {
        ndpAnnouncementAccepted = true;
        soundingAccessPoint = ndpa->getTransmitterAddress();
        dialogToken = ndpa->getSoundingDialogTokenNumber();
        associationId = mib->bssStationData.associationId;
        feedbackTypeMu = requestedMu;
        requestedNc = ndpaNc;
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
            indication->getNumberOfSpaceTimeStreams() >= 2 &&
            requestedNc <= indication->getNumberOfSpaceTimeStreams();
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
        const uint8_t nc = feedbackTypeMu ? requestedNc : 1;
        const uint8_t nr = indication->getNumberOfSpaceTimeStreams();
        const auto matrixBytes = getVhtFeedbackMatrixBytes(channelWidth, nr, nc, 4, feedbackTypeMu);
        const auto muBytes = feedbackTypeMu ? getVhtMuExclusiveReportBytes(channelWidth, nc, 4) : 0;
        const auto reportBytes = matrixBytes + muBytes;
        const auto maxSegmentReportBytes = VHT_MAX_FEEDBACK_MPDU_BYTES -
                (2 + 3 + 1 + (nc - 1));
        const auto segmentCount = std::max<size_t>(1,
                (reportBytes + maxSegmentReportBytes - 1) /
                maxSegmentReportBytes);
        if (segmentCount > 8)
            throw cRuntimeError("VHT compressed beamforming report requires %zu segments, maximum is 8", segmentCount);
        std::vector<Packet *> segments;
        size_t reportOffset = 0;
        for (size_t segment = 0; segment < segmentCount; ++segment) {
            const auto segmentBytes = std::min(maxSegmentReportBytes,
                    reportBytes - reportOffset);
            const auto segmentMatrixBytes = reportOffset < matrixBytes ?
                    std::min(segmentBytes, matrixBytes - reportOffset) : 0;
            const auto segmentMuOffset = reportOffset + segmentMatrixBytes;
            const auto segmentMuBytes = segmentBytes - segmentMatrixBytes;
            auto feedback = makeShared<Ieee80211VhtCompressedBeamformingFeedback>();
            feedback->setSoundingDialogTokenNumber(dialogToken);
            feedback->setAverageSnr(0);
            feedback->setFeedbackBandwidth(channelWidth.get());
            feedback->setNc(nc);
            feedback->setNr(nr);
            feedback->setGrouping(4);
            feedback->setCodebookInformation(false);
            feedback->setFeedbackTypeMu(feedbackTypeMu);
            feedback->setRemainingFeedbackSegments(segmentCount - segment - 1);
            feedback->setFirstFeedbackSegment(segment == 0);
            feedback->setAverageSnrAdditionalArraySize(nc - 1);
            for (size_t i = 0; i < feedback->getAverageSnrAdditionalArraySize(); i++)
                feedback->setAverageSnrAdditional(i, 0);
            feedback->setCompressedBeamformingReportLength(std::min<size_t>(12, segmentMatrixBytes));
            feedback->setCompressedBeamformingReportExtensionArraySize(segmentMatrixBytes > 12 ? segmentMatrixBytes - 12 : 0);
            feedback->setMuExclusiveBeamformingReportArraySize(segmentMuBytes);
            for (size_t i = 0; i < feedback->getCompressedBeamformingReportLength(); i++)
                feedback->setCompressedBeamformingReport(i, 0);
            for (size_t i = 0; i < feedback->getCompressedBeamformingReportExtensionArraySize(); i++)
                feedback->setCompressedBeamformingReportExtension(i, 0);
            for (size_t i = 0; i < feedback->getMuExclusiveBeamformingReportArraySize(); i++)
                feedback->setMuExclusiveBeamformingReport(i, 0);
            feedback->setChunkLength(B(2 + 3 + 1 + (nc - 1) + segmentBytes));
            auto header = makeShared<Ieee80211ActionFrame>();
            header->setType(ST_NOACKACTION);
            header->setCategory(21);
            header->setReceiverAddress(soundingAccessPoint);
            header->setTransmitterAddress(mac->getAddress());
            header->setAddress3(soundingAccessPoint);
            auto responseSegment = new Packet("VHT-Compressed-Beamforming", header);
            responseSegment->insertAtBack(feedback);
            responseSegment->insertAtBack(makeShared<Ieee80211MacTrailer>());
            if (segmentCount > 1) {
                auto trailer = responseSegment->removeAtBack<Ieee80211MacTrailer>(B(4));
                auto fcsMode = mac->getFcsMode();
                trailer->setFcsMode(fcsMode);
                if (fcsMode == FCS_COMPUTED)
                    trailer->setFcs(computeEthernetFcs(responseSegment, fcsMode));
                responseSegment->insertAtBack(trailer);
            }
            segments.push_back(responseSegment);
            reportOffset = segmentMuOffset + segmentMuBytes;
        }
        auto response = segments.size() == 1 ? segments.front() :
                (new MpduAggregation())->aggregateFrames(&segments);
        auto responseHeader = makeShared<Ieee80211ActionFrame>();
        responseHeader->setType(ST_NOACKACTION);
        responseHeader->setCategory(21);
        responseHeader->setReceiverAddress(soundingAccessPoint);
        responseHeader->setTransmitterAddress(mac->getAddress());
        responseHeader->setAddress3(soundingAccessPoint);
        response->addTag<physicallayer::Ieee80211ModeReq>()->setMode(
                modeSet->getSlowestMandatoryMode(MHz(20)));
        tx->transmitFrame(response, responseHeader, modeSet->getSifsTime(), callback);
        delete response;
    }
    reset();
    delete packet;
    return true;
}

} // namespace ieee80211
} // namespace inet
