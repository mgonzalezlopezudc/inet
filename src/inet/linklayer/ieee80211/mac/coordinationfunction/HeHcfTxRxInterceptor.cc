//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfFeature.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRxInterceptor.h"

#include <algorithm>
#include <set>
#include <sstream>

#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF transmit/receive callbacks.

namespace inet {
namespace ieee80211 {

static bool isHeNdpPacket(const Packet *packet)
{
    auto request = packet == nullptr ? nullptr :
            packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    return packet != nullptr && packet->getDataLength() == B(0) &&
            request != nullptr && request->getPpduLayout() != nullptr &&
            request->getPpduLayout()->isNdp();
}

static bool isHeTbPacket(const Packet *packet)
{
    if (packet == nullptr)
        return false;
    auto request = packet->findTag<physicallayer::Ieee80211HeTxVectorReq>();
    if (request != nullptr && request->getTxVector() != nullptr &&
            request->getTxVector()->getCommon().getParameters().ppduFormat ==
                    physicallayer::HE_TRIGGER_BASED_UPLINK)
        return true;
    auto indication = packet->findTag<physicallayer::Ieee80211HeRxVectorInd>();
    if (indication != nullptr && indication->getRxVector() != nullptr &&
            indication->getRxVector()->getCommon().getPpduFormat() ==
                    physicallayer::HE_TRIGGER_BASED_UPLINK)
        return true;
    auto recipientContext =
            packet->findTag<physicallayer::Ieee80211HeTbRecipientContextInd>();
    if (recipientContext != nullptr &&
            recipientContext->getRecipientParameters() != nullptr)
        return true;
    // Preserve the Trigger correlation as the canonical fallback for the two
    // headerless HE-TB representations: an A-MPDU starts with a delimiter and
    // an NDP feedback report is empty.
    return packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>() != nullptr &&
            (packet->getDataLength() == B(0) ||
             dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                     packet->peekAtFront()) != nullptr);
}

bool HeHcfFeature::processHeSoundingFrame(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header)
{
    HeSoundingService::ReceiveSnapshot snapshot {
        static_cast<uint16_t>(mac->getMib()->getLocalAssociationId()), mac->getAddress(),
        mac->getMib()->heOperation.bssColor,
        mac->getMib()->localHeCapabilities.soundingDimensions,
        static_cast<int>(mac->getFcsMode()), modeSet->getSifsTime(),
        getIeee80211HeSolicitingTxopDuration(packet),
        getLinkPhyContext().getSnapshot(), {}};
    for (const auto& association : mac->getMib()->getPeerAssociationSnapshots()) {
        if (!association.hasAssociationId())
            continue;
        snapshot.peers.push_back({association.getAddress(),
                static_cast<uint16_t>(association.getAssociationId()),
                mac->getMib()->getNegotiatedHeCapabilities(association.getAddress())});
    }
    HeSoundingService::ReceiveActions actions;
    actions.publishCsiUpdate = [this, peers = snapshot.peers]
            (const HeSoundingService::CsiUpdateEvent& event) {
        std::vector<MacAddress> addresses;
        for (const auto& peer : peers)
            addresses.push_back(peer.address);
        getHePeerStateService().getCsiManager().updateCsi(event.peer,
                event.channelBandwidth, addresses,
                [peers] (const MacAddress& address) {
                    auto peer = std::find_if(peers.begin(), peers.end(),
                            [&] (const HeSoundingService::PeerSnapshot& value) {
                                return value.address == address;
                            });
                    return peer == peers.end() ? 0 : peer->associationId;
                });
    };
    actions.transmitResponse = [this] (Packet *response,
            const Ptr<const Ieee80211MacHeader>& responseHeader, simtime_t ifs) {
        tx->transmitFrame(response, responseHeader, ifs, hcf);
    };
    return check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"))->
            processSoundingFrame(packet, header, snapshot, actions);
}

void HeHcfFeature::rejectUnexpectedHeTb(Packet *packet)
{
    PacketDropDetails details;
    details.setReason(NOT_ADDRESSED_TO_US);
    hcf->emit(cComponent::registerSignal("packetDropped"), packet, &details);
    delete packet;
}

void HeHcfFeature::processHeTrigger(Packet *packet,
        const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    processReceivedTriggerFrame(packet, trigger);
}

void HeHcfFeature::processHeMultiStaBlockAck(Packet *packet,
        const Ptr<const Ieee80211MultiStaBlockAck>& blockAck)
{
    processReceivedMultiStaBlockAck(packet, blockAck);
}

void HeHcfFeature::observeBufferStatus(const Ptr<const Ieee80211DataHeader>& header)
{
    if (!ulCoordinator->isEnabled() || !header->getBufferStatusPresent())
        return;
    auto aid = getAssociationId(header->getTransmitterAddress());
    if (aid > 0)
        ulCoordinator->updateBufferStatus(aid, header->getTransmitterAddress(),
                static_cast<AccessCategory>(header->getBufferStatusAc()),
                header->getBufferStatusTid(), header->getBufferStatusQueueSize());
}

bool HeHcfFeature::isOperatingModeControlSupported() const
{
    return mac->getMib()->localHeCapabilities.omControl;
}

void HeHcfFeature::applyOperatingMode(const MacAddress& peer,
        const Ieee80211HeOperatingMode& mode)
{
    updatePeerOperatingMode(peer, mode);
}

void HeHcfFeature::startMuEdcaTimer(AccessCategory accessCategory)
{
    edca->getEdcaf(accessCategory)->startMuEdcaTimer();
}

void HeHcfFeature::notifyHeUlMuPacketTransmitted(Packet *packet)
{
    auto channelOwner = edca->getChannelOwner();
    if (channelOwner != nullptr)
        channelOwner->emit(cComponent::registerSignal("packetSentToPeer"), packet);
}

bool HeHcfFeature::isHeDlMuContainer(const Packet *packet) const
{
    return getHeDlMuExchangeCoordinator().isActiveContainer(packet);
}

void HeHcfFeature::routeHeDlMuContainer(Packet *packet)
{
    auto channelOwner = edca->getChannelOwner();
    if (channelOwner == nullptr)
        return;
    auto& coordinator = getDlMuExchangeCoordinator();
    ASSERT(!coordinator.getActiveMembers().empty());
    coordinator.routeTransmittedContainer(packet);
}

HeHcfTxRxInterceptor::LocalRole HeHcfFeature::getLocalRole() const
{
    switch (mac->getMib()->getStationType()) {
        case Ieee80211Mib::STATION: return HeHcfTxRxInterceptor::LocalRole::STATION;
        case Ieee80211Mib::ACCESS_POINT: return HeHcfTxRxInterceptor::LocalRole::ACCESS_POINT;
        default: return HeHcfTxRxInterceptor::LocalRole::OTHER;
    }
}

uint16_t HeHcfFeature::getLocalAssociationId() const
{
    return static_cast<uint16_t>(mac->getMib()->getLocalAssociationId());
}

bool HeHcfFeature::hasActiveHeDlMuMembers() const
{
    return !getHeDlMuExchangeCoordinator().getActiveMembers().empty();
}

MacAddress HeHcfFeature::getLocalAddress() const
{
    return mac->getAddress();
}

MacAddress HeHcfFeature::getBssid() const
{
    return mac->getMib()->getBssid();
}

void HeHcfFeature::transmitHeNdp(Packet *packet,
        const Ptr<const Ieee80211MacHeader>& header, simtime_t ifs)
{
    tx->transmitFrame(packet, header, ifs, hcf);
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processPhyIndication(Packet *packet)
{
    auto indication = packet->findTag<physicallayer::Ieee80211NdpInd>();
    if (indication == nullptr || indication->getPhyFormat() !=
            physicallayer::IEEE80211_NDP_PHY_HE_SU)
        return IHcfTxRxInterceptor::Result::continueCommon();
    return actions->processHeSoundingFrame(packet, nullptr) ?
            IHcfTxRxInterceptor::Result::consumed() :
            IHcfTxRxInterceptor::Result::continueCommon();
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processRejectedHeaderlessResponse(
        Packet *packet, IReceiveStep::HeaderlessResponseFamily family)
{
    if (family != IReceiveStep::HeaderlessResponseFamily::HE_TRIGGER_BASED &&
            !isHeTbPacket(packet))
        return IHcfTxRxInterceptor::Result::continueCommon();
    EV_INFO << "Discarding foreign or late HE-TB aggregate outside the active Trigger collection\n";
    actions->rejectUnexpectedHeTb(packet);
    return IHcfTxRxInterceptor::Result::rejected();
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processRecipientFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    // Capture the HE PHY/CSI snapshot only for frames that can participate in
    // the 26.7.3 sounding exchange. Ordinary data/control must remain usable
    // when the NED-compatible HeHcf adapter is selected with a non-HE mode.
    auto soundingTrigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header);
    bool mayBeSounding = header != nullptr && (header->getType() == ST_ACTION ||
            (soundingTrigger != nullptr && soundingTrigger->getTriggerType() == 1));
    if (mayBeSounding && actions->processHeSoundingFrame(packet, header))
        return IHcfTxRxInterceptor::Result::consumed();

    if (isHeTbPacket(packet)) {
        EV_INFO << "Discarding HE-TB response outside active Trigger collection\n";
        actions->rejectUnexpectedHeTb(packet);
        return IHcfTxRxInterceptor::Result::rejected();
    }

    if (auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(header)) {
        // 9.3.1.22 Trigger frames are control frames that solicit HE TB
        // responses; do not pass them through the legacy HCF recipient path.
        actions->processHeTrigger(packet, trigger);
        return IHcfTxRxInterceptor::Result::consumed();
    }
    if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(header)) {
        // 26.4.2 defines per-AID/TID Multi-STA BA records.  Triggered UL
        // responses retain their own pending exchange state, so handle them
        // before the base BlockAck path.
        actions->processHeMultiStaBlockAck(packet, multiStaBlockAck);
        return IHcfTxRxInterceptor::Result::consumed();
    }
    if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header)) {
        actions->observeBufferStatus(dataHeader);
        if (dataHeader->getOperatingModePresent() && actions->isOperatingModeControlSupported()) {
            Ieee80211HeOperatingMode mode;
            mode.channelWidth = dataHeader->getOperatingModeChannelWidth();
            mode.rxNss = dataHeader->getOperatingModeRxNss();
            mode.ulMuDisable = dataHeader->getOperatingModeUlMuDisable();
            actions->applyOperatingMode(dataHeader->getTransmitterAddress(), mode);
            EV_INFO << "Accepted HE OMI from " << dataHeader->getTransmitterAddress()
                    << ": width=" << (int)mode.channelWidth << " rxNss=" << (int)mode.rxNss
                    << " ulMuDisable=" << mode.ulMuDisable << endl;
        }
    }
    return IHcfTxRxInterceptor::Result::continueCommon();
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processTransmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    if (isHeTbPacket(packet)) {
        // IEEE Std 802.11-2024, 26.2.7: start MUEDCATimer[AC] at HE-TB PPDU
        // completion only for successful QoS Data that needs no immediate
        // acknowledgment. Immediate-ack data is deferred to its correlated
        // Multi-STA BA success decision, and QoS Null must not activate it.
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
        if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS &&
                dataHeader->getAckPolicy() == NO_ACK) {
            AccessCategory ac = actions->mapTidToAccessCategory(dataHeader->getTid());
            if (ac >= 0 && ac < 4)
                actions->startMuEdcaTimer(ac);
        }
        return IHcfTxRxInterceptor::Result::consumedRetained();
    }
    return IHcfTxRxInterceptor::Result::continueCommon();
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processTransmittedFrame(Packet *packet)
{
    if (isHeNdpPacket(packet)) {
        // IEEE 802.11-2024 26.7.3: the sounding NDP has a PHY preamble but no
        // PSDU, hence no MAC header to enter acknowledgement, BA, or
        // content-derived packet statistics.
        return IHcfTxRxInterceptor::Result::consumedRetained();
    }
    if (isHeTbPacket(packet)) {
        // IEEE Std 802.11-2024 10.3.2.13.3 and 26.4.4.5: Normal Ack in
        // this HE-TB MPDU solicits the terminal Multi-STA Block Ack. The
        // HeTriggeredUlExchangeService owns that response and retry state, so
        // the legacy single-user Ack state machine must not start in parallel.
        return IHcfTxRxInterceptor::Result::consumedRetained();
    }
    if (actions->isHeUlMuExchangeActive()) {
        actions->notifyHeUlMuPacketTransmitted(packet);
        return IHcfTxRxInterceptor::Result::consumedRetained();
    }
    if (actions->isHeDlMuContainer(packet)) {
        // The HE MU PPDU is one PHY transmission but contains per-user MPDUs.
        // 26.4/10.25 BlockAck state is per recipient/TID, so each contained
        // MPDU must enter the normal originator in-progress and BA state.
        actions->routeHeDlMuContainer(packet);
    }
    else
        return IHcfTxRxInterceptor::Result::continueCommon();
    return IHcfTxRxInterceptor::Result::consumedRetained();
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processTransmittedControl(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac)
{
    // IEEE 802.11-2024 9.3.1.22.4 MU-BAR responses:
    // When a STA transmits a BlockAck response as a SIFS reply to a MU-BAR
    // Trigger, the TX complete path invokes originatorProcessTransmittedControlFrame.
    // Base HCF only expects control frames that request a response (like RTS/BlockAckReq) to schedule
    // timeouts/recovery and throws "Unknown control frame" for a sent BlockAck. Since BlockAck is terminal
    // and does not expect SIFS feedback, we explicitly bypass it here.
    if (dynamicPtrCast<const Ieee80211BlockAck>(controlHeader) != nullptr) {
        return IHcfTxRxInterceptor::Result::consumedRetained();
    }
    return IHcfTxRxInterceptor::Result::continueCommon();
}

bool HeHcfFeature::reportHeDlMuTxResult(Packet *packet, AccessCategory ac, bool success)
{
    auto heRateControl = dynamic_cast<IIeee80211HeRateControl *>(dataAndMgmtRateControl);
    const auto& members = getHeDlMuExchangeCoordinator().getActiveMembers();
    if (packet == nullptr || members.empty() || heRateControl == nullptr)
        return false;

    if (ac < 0 || ac >= 4)
        return false;
    auto edcaf = edca->getEdcaf(ac);
    for (const auto& member : members) {
        if (member.packet != packet)
            continue;
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                packet->peekAtFront<Ieee80211MacHeader>());
        if (dataHeader == nullptr)
            return false;
        int retryCount = dataHeader->getRetry() ?
                edcaf->getRecoveryProcedure()->getRetryCount(packet, dataHeader) : 0;
        heRateControl->reportHeTxResult(member.peer, member.mcs,
                member.numberOfSpatialStreams, member.ruToneSize,
                retryCount, success, success ? packet->getByteLength() : 0);
        return true;
    }
    return false;
}

void HeHcfFeature::originatorProcessBlockAckResult(
        const Ptr<const Ieee80211BlockAck>& blockAck,
        const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>& ackedFrames,
        AccessCategory ac)
{
    const auto& members = getHeDlMuExchangeCoordinator().getActiveMembers();
    if (members.empty())
        return;

    for (const auto& member : members) {
        if (member.peer != blockAck->getTransmitterAddress())
            continue;
        auto packet = member.packet;
            auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    packet->peekAtFront<Ieee80211MacHeader>());
            if (dataHeader == nullptr || !dataHeader->getSequenceNumber().isValid())
                continue;
            auto key = std::make_pair(dataHeader->getReceiverAddress(),
                    std::make_pair(dataHeader->getTid(),
                            SequenceControlField(dataHeader->getSequenceNumber().get(),
                                    dataHeader->getFragmentNumber())));
            bool tidCovered = false;
            if (auto basic = dynamicPtrCast<const Ieee80211BasicBlockAck>(blockAck))
                tidCovered = dataHeader->getTid() == basic->getTidInfo();
            else if (auto compressed = dynamicPtrCast<const Ieee80211CompressedBlockAck>(blockAck))
                tidCovered = dataHeader->getTid() == compressed->getTidInfo();
            else if (auto multiTid = dynamicPtrCast<const Ieee80211MultiTidBlockAck>(blockAck))
                for (unsigned int i = 0; i < multiTid->getRecordsArraySize(); ++i)
                    tidCovered |= dataHeader->getTid() == multiTid->getRecords(i).tid;
            if (!tidCovered)
                continue;
            if (ackedFrames.count(key) != 0)
                reportHeDlMuTxResult(packet, ac, true);
            else
                reportHeDlMuTxResult(packet, ac, false);
    }
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processReceivedResponse(Packet *receivedPacket, Packet *lastTransmittedPacket)
{
    auto receivedHeader = receivedPacket->peekAtFront<Ieee80211MacHeader>();
    if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(receivedHeader)) {
        // Triggered UL has its own packet ledger and exact Trigger correlation.
        // Preserve that owner before considering the UL-SU BlockAck path.
        if (receivedPacket->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>() != nullptr) {
            actions->processHeMultiStaBlockAck(receivedPacket->dup(), multiStaBlockAck);
            return IHcfTxRxInterceptor::Result::consumedRetained();
        }
        auto lastTransmittedHeader = lastTransmittedPacket == nullptr ? nullptr :
                dynamicPtrCast<const Ieee80211MacHeader>(
                        lastTransmittedPacket->peekAtFront());
        auto multiTidBlockAckReq =
                dynamicPtrCast<const Ieee80211MultiTidBlockAckReq>(
                        lastTransmittedHeader);
        bool validUlSuResponse = multiTidBlockAckReq != nullptr;
        uint16_t expectedAid = 0;
        if (validUlSuResponse &&
                actions->getLocalRole() == LocalRole::STATION) {
            auto associationId = actions->getLocalAssociationId();
            validUlSuResponse = associationId > 0 && associationId <= 2007;
            if (validUlSuResponse)
                expectedAid = associationId;
        }
        else if (validUlSuResponse &&
                actions->getLocalRole() != LocalRole::ACCESS_POINT)
            validUlSuResponse = false;
        if (validUlSuResponse) {
            validUlSuResponse =
                    multiStaBlockAck->getReceiverAddress() ==
                            multiTidBlockAckReq->getTransmitterAddress() &&
                    multiStaBlockAck->getTransmitterAddress() ==
                            multiTidBlockAckReq->getReceiverAddress() &&
                    multiStaBlockAck->getRecordsArraySize() ==
                            multiTidBlockAckReq->getRecordsArraySize();
        }
        std::set<std::pair<Tid, uint16_t>> requestedRecords;
        if (validUlSuResponse) {
            for (unsigned int i = 0;
                    i < multiTidBlockAckReq->getRecordsArraySize(); ++i) {
                const auto& record = multiTidBlockAckReq->getRecords(i);
                validUlSuResponse &= requestedRecords.emplace(record.tid,
                        record.startingSequenceNumber).second;
            }
        }
        std::set<std::pair<Tid, uint16_t>> responseRecords;
        if (validUlSuResponse) {
            for (unsigned int i = 0;
                    i < multiStaBlockAck->getRecordsArraySize(); ++i) {
                const auto& record = multiStaBlockAck->getRecords(i);
                auto key = std::make_pair(static_cast<Tid>(record.tid),
                        record.startingSequenceNumber);
                validUlSuResponse &= record.aid == expectedAid &&
                        requestedRecords.count(key) == 1 &&
                        responseRecords.insert(key).second;
            }
            validUlSuResponse &= responseRecords == requestedRecords;
        }
        // IEEE Std 802.11-2024, 10.25.5, 26.4.2 and 26.4.5:
        // only a matching per-AID/TID Multi-STA response to the preceding
        // HE Multi-TID BAR completes the ordinary originator exchange.
        if (validUlSuResponse) {
            return IHcfTxRxInterceptor::Result::continueCommon();
        }
        if (multiTidBlockAckReq != nullptr)
            EV_WARN << "Discarding invalid UL-SU Multi-STA Block Ack response\n";
        // FrameSequenceHandler owns the received frame on the originator path;
        // the transaction processor consumes its argument.
        actions->processHeMultiStaBlockAck(receivedPacket->dup(), multiStaBlockAck);
        return IHcfTxRxInterceptor::Result::rejectedRetained();
    }
    return IHcfTxRxInterceptor::Result::continueCommon();
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processFailedFrame(Packet *failedPacket)
{
    ASSERT(failedPacket != nullptr);
    EV_WARN << "HE MU: transmission failed for frame " << failedPacket->getName()
            << " type = " << (failedPacket->peekAtFront<Ieee80211MacHeader>() != nullptr ? (int)failedPacket->peekAtFront<Ieee80211MacHeader>()->getType() : -1) << endl;
    if (actions->hasActiveHeDlMuMembers()) {
        actions->processHeDlMuFailedFrame(failedPacket);
        return IHcfTxRxInterceptor::Result::consumed();
    }
    else
        return IHcfTxRxInterceptor::Result::continueCommon();
}

void HeHcfFeature::processHeDlMuFailedFrame(Packet *failedPacket)
{
        hcf->aggregationService.discardTransmission(failedPacket);
        // 26.5.1 extends EDCA success/failure semantics for DL MU, but retry
        // state is still per MPDU/TID.  Requeue a failed subframe to the
        // destination's per-STA queue so the next DL MU scheduler run can choose
        // a standard-valid subset again.
        ASSERT(edca->getChannelOwner() != nullptr);
        auto failedHeader = failedPacket->peekAtFront<Ieee80211MacHeader>();
        auto edcaf = edca->getChannelOwner();
        if (edcaf) {
            bool retryLimitReached = false;
            if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader)) {
                edcaf->getRecoveryProcedure()->dataFrameTransmissionFailed(failedPacket, dataHeader);
                retryLimitReached = edcaf->getRecoveryProcedure()->isRetryLimitReached(failedPacket, dataHeader);
                bool heResultReported = reportHeDlMuTxResult(
                        failedPacket, edcaf->getAccessCategory(), false);
                if (dataAndMgmtRateControl && !heResultReported) {
                    int retryCount = edcaf->getRecoveryProcedure()->getRetryCount(failedPacket, dataHeader);
                    dataAndMgmtRateControl->frameTransmitted(failedPacket, retryCount, false, retryLimitReached);
                }
                edcaf->getAckHandler()->processFailedFrame(dataHeader);
            }
            else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(failedHeader)) {
                edca->getMgmtAndNonQoSRecoveryProcedure()->dataOrMgmtFrameTransmissionFailed(failedPacket, mgmtHeader, edcaf->getStationRetryCounters());
                retryLimitReached = edca->getMgmtAndNonQoSRecoveryProcedure()->isRetryLimitReached(failedPacket, mgmtHeader);
                if (dataAndMgmtRateControl) {
                    int retryCount = edca->getMgmtAndNonQoSRecoveryProcedure()->getRetryCount(failedPacket, mgmtHeader);
                    dataAndMgmtRateControl->frameTransmitted(failedPacket, retryCount, false, retryLimitReached);
                }
                edcaf->getAckHandler()->processFailedFrame(mgmtHeader);
            }
            else if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(failedHeader)) {
                processFailedBlockAckReq(edcaf, blockAckReq, true);
                return;
            }

            if (retryLimitReached) {
                if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(failedHeader))
                    edcaf->getRecoveryProcedure()->retryLimitReached(failedPacket, dataHeader);
                else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(failedHeader))
                    edca->getMgmtAndNonQoSRecoveryProcedure()->retryLimitReached(failedPacket, mgmtHeader);
                edcaf->getInProgressFrames()->dropFrame(failedPacket);
                edcaf->getAckHandler()->dropFrame(dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(failedHeader));
            }
            else {
                EV_INFO << "HE DL MU retrying frame: " << failedPacket->getName() << ", re-queuing.\n";
                auto h = failedPacket->removeAtFront<Ieee80211DataOrMgmtHeader>();
                ASSERT(h != nullptr);
                h->setRetry(true);
                failedPacket->insertAtFront(h);

                // Remove from inProgressFrames
                edcaf->getInProgressFrames()->removeInProgressFrame(failedPacket);

                // Re-enqueue into the destination STA's queue bank when available.
                auto pendingQueue = getPerStaQueue(failedHeader->getReceiverAddress(), edcaf->getAccessCategory());
                ASSERT(pendingQueue != nullptr);
                pendingQueue->pushPacket(failedPacket, nullptr);
            }
        }
}

IHcfTxRxInterceptor::Result HeHcfTxRxInterceptor::processTransmitRequest(Packet *packet, simtime_t ifs)
{
    if (isHeNdpPacket(packet)) {
        // Frame-sequence transmission normally derives the Tx header by
        // peeking packet content. A sounding NDP is intentionally empty, so
        // retain a detached header only for the local Tx callback/address
        // contract, as is already done for triggered NDP feedback.
        auto ndpHeader = makeShared<Ieee80211DataHeader>();
        ndpHeader->setType(ST_QOS_NULL);
        ndpHeader->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);
        ndpHeader->setTransmitterAddress(actions->getLocalAddress());
        ndpHeader->setAddress3(actions->getBssid());
        ndpHeader->setDurationField(SIMTIME_ZERO);
        ndpHeader->setChunkLength(B(30));
        actions->transmitHeNdp(packet, ndpHeader, ifs);
        return IHcfTxRxInterceptor::Result::consumed();
    }
    return IHcfTxRxInterceptor::Result::continueCommon();
}

void HeHcfFeature::legacyPreambleReceived(const Packet *packet)
{
    auto soundingCoordinator = check_and_cast<HeSoundingCoordinator *>(getSubmodule("soundingCoordinator"));
    soundingCoordinator->processLegacyPreamble(packet);
}
} // namespace ieee80211
} // namespace inet
