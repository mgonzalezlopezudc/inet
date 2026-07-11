//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Rx.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/checksum/Checksum.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211MldMac.h"
#include "inet/linklayer/ieee80211/mac/contract/IContention.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

simsignal_t Rx::navChangedSignal = cComponent::registerSignal("navChanged");

Define_Module(Rx);

Rx::Rx()
{
}

Rx::~Rx()
{
    cancelAndDelete(endNavTimer);
    cancelAndDelete(endIntraBssNavTimer);
}

void Rx::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        endNavTimer = new cMessage("NAV");
        endIntraBssNavTimer = new cMessage("Intra-BSS NAV");
        WATCH(address);
        WATCH(receptionState);
        WATCH(transmissionState);
        WATCH(receivedPart);
        WATCH(mediumFree);
        WATCH_EXPR("rxStatus", getRxStatusTxt());
    }
    // TODO INITSTAGE
    else if (stage == INITSTAGE_NETWORK_INTERFACE_CONFIGURATION) {
        address = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this)->getSubmodule("mac"))->getAddress();
        recomputeMediumFree();
    }
}

std::string Rx::getRxStatusTxt() const
{
    if (mediumFree)
        return "FREE";
    std::string s = "BUSY (";
    bool addSpace = false;
    if (transmissionState != IRadio::TRANSMISSION_STATE_UNDEFINED) {
        switch (transmissionState) {
            case IRadio::TRANSMISSION_STATE_IDLE: s += "Tx-Idle"; break;
            case IRadio::TRANSMISSION_STATE_TRANSMITTING: s += "Tx"; break;
            default: break;
        }
        addSpace = true;
    }
    else {
        switch (receptionState) {
            case IRadio::RECEPTION_STATE_UNDEFINED: s += "Switching"; break;
            case IRadio::RECEPTION_STATE_IDLE: s += "Rx-Idle"; break;
            case IRadio::RECEPTION_STATE_BUSY: s += "Noise"; break;
            case IRadio::RECEPTION_STATE_RECEIVING: s += "Recv"; break;
            default: break;
        }
        addSpace = true;
    }
    if (endNavTimer->isScheduled())
        s += std::string(addSpace ? " " : "") + "NAV";
    if (endIntraBssNavTimer->isScheduled())
        s += std::string(addSpace ? " " : "") + "Intra-BSS-NAV";
    s += ")";
    return s;
}

void Rx::handleMessage(cMessage *msg)
{
    if (msg == endNavTimer || msg == endIntraBssNavTimer) {
        EV_INFO << "The radio channel has become free according to the NAV" << std::endl;
        emit(navChangedSignal, SimTime::ZERO);
        recomputeMediumFree();
    }
    else
        throw cRuntimeError("Unexpected self message");
}

bool Rx::lowerFrameReceived(Packet *packet)
{
    Enter_Method("lowerFrameReceived(\"%s\")", packet->getName());
    take(packet);

    bool selfInterference = false;
    auto macModule = check_and_cast<Ieee80211Mac *>(getParentModule());
    if (macModule->getMldMac() != nullptr) {
        auto mib = macModule->getMib();
        if (mib->localEhtCapabilities.mlo && mib->localEhtCapabilities.nstr) {
            simtime_t rxStart = simTime();
            simtime_t rxEnd = simTime();
            if (auto signalTimeInd = packet->findTag<SignalTimeInd>()) {
                rxStart = signalTimeInd->getStartTime();
                rxEnd = signalTimeInd->getEndTime();
            }
            if (macModule->getMldMac()->isOtherLinkTransmittingDuring(macModule, rxStart, rxEnd)) {
                selfInterference = true;
            }
        }
    }

    bool isFrameOk = isFcsOk(packet) && !selfInterference;
    if (isFrameOk) {
        EV_INFO << "Received frame from PHY: " << packet << endl;
        const auto& header = packet->peekAtFront<Ieee80211MacHeader>();
        if (header->getReceiverAddress() != address)
            setOrExtendNav(header->getDurationField(), isIntraBssFrame(header));
        return true;
    }
    else {
        if (selfInterference) {
            EV_INFO << "Received frame corrupted due to MLO NSTR self-interference: " << packet << std::endl;
        } else {
            EV_INFO << "Received an erroneous frame from PHY, dropping it." << std::endl;
        }
        PacketDropDetails details;
        details.setReason(INCORRECTLY_RECEIVED);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        for (auto contention : contentions)
            contention->corruptedFrameReceived();
        return false;
    }
}

void Rx::frameTransmitted(simtime_t durationField)
{
    Enter_Method("frameTransmitted");
    // the txIndex that transmitted the frame should already own the TXOP, so
    // it has no need to (and should not) check the NAV.
    setOrExtendNav(durationField);
}

void Rx::legacySignalReceived(simtime_t durationField)
{
    Enter_Method("legacySignalReceived");
    setOrExtendNav(durationField);
}

bool Rx::isReceptionInProgress() const
{
    return receptionState == IRadio::RECEPTION_STATE_RECEIVING &&
           (receivedPart == IRadioSignal::SIGNAL_PART_WHOLE || receivedPart == IRadioSignal::SIGNAL_PART_DATA);
}

bool Rx::isFcsOk(Packet *packet) const
{
    if (packet->hasBitError() || !packet->peekData()->isCorrect())
        return false;
    else {
        const auto& trailer = packet->peekAtBack<Ieee80211MacTrailer>(B(4));
        switch (trailer->getFcsMode()) {
            case FCS_DECLARED_INCORRECT:
                return false;
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_COMPUTED: {
                const auto& fcsBytes = packet->peekDataAt<BytesChunk>(B(0), packet->getDataLength() - trailer->getChunkLength());
                auto bufferLength = fcsBytes->getChunkLength().get<B>();
                auto buffer = new uint8_t[bufferLength];
                fcsBytes->copyToBuffer(buffer, bufferLength);
                auto computedFcs = ethernetFcs(buffer, bufferLength);
                delete[] buffer;
                return computedFcs == trailer->getFcs();
            }
            default:
                throw cRuntimeError("Unknown FCS mode");
        }
    }
}

void Rx::recomputeMediumFree()
{
    bool oldMediumFree = mediumFree;
    bool otherLinkTx = false;
    auto macModule = check_and_cast<Ieee80211Mac *>(getParentModule());
    if (macModule->getMldMac() != nullptr) {
        auto mib = macModule->getMib();
        if (mib->localEhtCapabilities.mlo && mib->localEhtCapabilities.nstr) {
            otherLinkTx = macModule->getMldMac()->isOtherLinkTransmitting(macModule);
        }
    }
    // note: the duration of mode switching (rx-to-tx or tx-to-rx) should also count as busy
    mediumFree = receptionState == IRadio::RECEPTION_STATE_IDLE && transmissionState == IRadio::TRANSMISSION_STATE_UNDEFINED && !endNavTimer->isScheduled() && !endIntraBssNavTimer->isScheduled() && !otherLinkTx;
    if (mediumFree != oldMediumFree) {
        for (auto contention : contentions)
            contention->mediumStateChanged(mediumFree);
    }
}

void Rx::receptionStateChanged(IRadio::ReceptionState state)
{
    Enter_Method("receptionStateChanged");
    receptionState = state;
    recomputeMediumFree();
}

void Rx::receivedSignalPartChanged(IRadioSignal::SignalPart part)
{
    Enter_Method("receivedSignalPartChanged");
    receivedPart = part;
    recomputeMediumFree();
}

void Rx::transmissionStateChanged(IRadio::TransmissionState state)
{
    Enter_Method("transmissionStateChanged");
    transmissionState = state;
    recomputeMediumFree();
}

void Rx::setOrExtendNav(simtime_t navInterval)
{
    setOrExtendNav(navInterval, false);
}

bool Rx::isIntraBssFrame(const Ptr<const Ieee80211MacHeader>& header) const
{
    auto mac = check_and_cast<Ieee80211Mac *>(getParentModule());
    auto mib = mac->getMib();
    if (!mib->localHeCapabilities.twoNav)
        return false;
    auto twoAddress = dynamicPtrCast<const Ieee80211TwoAddressHeader>(header);
    if (twoAddress == nullptr)
        return false;
    auto transmitter = twoAddress->getTransmitterAddress();
    if (transmitter == mib->bssData.bssid)
        return true;
    if (mib->bssStationData.stationType == Ieee80211Mib::ACCESS_POINT)
        return mib->bssAccessPointData.stations.count(transmitter) != 0;
    return false;
}

void Rx::setOrExtendNav(simtime_t navInterval, bool intraBss)
{
    if (navInterval == -1)
        return;
    ASSERT(navInterval >= 0);
    if (navInterval > 0) {
        simtime_t endNav = simTime() + navInterval;
        auto timer = intraBss ? endIntraBssNavTimer : endNavTimer;
        if (timer->isScheduled()) {
            simtime_t oldEndNav = timer->getArrivalTime();
            if (endNav < oldEndNav)
                return; // never decrease NAV
            emit(navChangedSignal, timer->getArrivalTime() - simTime());
            cancelEvent(timer);
        }
        else
            emit(navChangedSignal, SimTime::ZERO);
        EV_INFO << "Setting " << (intraBss ? "intra-BSS NAV" : "basic NAV") << " to " << navInterval << std::endl;
        scheduleAt(endNav, timer);
        emit(navChangedSignal, endNav - simTime());
        recomputeMediumFree();
    }
}

void Rx::registerContention(IContention *contention)
{
    contention->mediumStateChanged(mediumFree);
    contentions.push_back(contention);
}

} // namespace ieee80211
} // namespace inet
