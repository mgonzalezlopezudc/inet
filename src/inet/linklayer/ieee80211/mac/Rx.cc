//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Rx.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/checksum/Checksum.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/contract/IContention.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"

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
}

void Rx::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        endNavTimer = new cMessage("NAV");
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
    s += ")";
    return s;
}

void Rx::handleMessage(cMessage *msg)
{
    if (msg == endNavTimer) {
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

    bool isFrameOk = isFcsOk(packet);
    if (isFrameOk) {
        EV_INFO << "Received frame from PHY: " << packet << endl;
        const auto& header = packet->peekAtFront<Ieee80211MacHeader>();
        if (header->getReceiverAddress() != address)
            setOrExtendNav(header->getDurationField());
        return true;
    }
    else {
        EV_INFO << "Received an erroneous frame from PHY, dropping it." << std::endl;
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
    // note: the duration of mode switching (rx-to-tx or tx-to-rx) should also count as busy
    bool primaryPhysicallyIdle = (receptionState != IRadio::RECEPTION_STATE_RECEIVING) &&
            (!ccaEnabled ? (receptionState == IRadio::RECEPTION_STATE_IDLE) : !primary20CcaBusy);
    mediumFree = primaryPhysicallyIdle && transmissionState == IRadio::TRANSMISSION_STATE_UNDEFINED && !endNavTimer->isScheduled();
    if (mediumFree != oldMediumFree) {
        for (auto contention : contentions)
            contention->mediumStateChanged(mediumFree);
    }
}

bool Rx::isSecondaryChannelIdleFor(simtime_t interval) const
{
    return isChannelIdleForTransmission(physicallayer::IEEE80211_CHANNEL_WIDTH_40MHZ, interval);
}

bool Rx::isChannelIdleForTransmission(physicallayer::Ieee80211ChannelWidth channelWidth, simtime_t interval) const
{
    if (!ccaEnabled)
        return true; // legacy radios without a grouped CCA provider
    auto idleFor = [&] (bool available, bool busy, simtime_t idleSince) {
        return available && !busy && idleSince >= SIMTIME_ZERO && simTime() - idleSince >= interval;
    };
    if (channelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ)
        return true;
    if (channelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_40MHZ)
        return idleFor(ccaChannelWidth != physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ,
                secondary20CcaBusy, secondary20CcaIdleSince);
    if (channelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ)
        return idleFor(ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ ||
                       ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ ||
                       ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ,
                secondary20CcaBusy, secondary20CcaIdleSince) &&
                idleFor(ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ ||
                        ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ ||
                        ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ,
                        secondary40CcaBusy, secondary40CcaIdleSince);
    if (channelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ)
        return isChannelIdleForTransmission(physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ, interval) &&
                idleFor(ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ ||
                        ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ,
                        secondary80CcaBusy, secondary80CcaIdleSince);
    return false;
}

void Rx::ccaStateChanged(const Ieee80211CcaSnapshot& snapshot)
{
    Enter_Method("ccaStateChanged");
    bool oldEnabled = ccaEnabled;
    auto oldWidth = ccaChannelWidth;
    uint64_t oldRevision = ccaConfigurationRevision;
    bool oldSecondary20Available = ccaEnabled && oldWidth != physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ;
    bool oldSecondary40Available = ccaEnabled && (oldWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ ||
            oldWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ || oldWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ);
    bool oldSecondary80Available = ccaEnabled && (oldWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ ||
            oldWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ);
    ccaEnabled = snapshot.isEnabled();
    ccaChannelWidth = snapshot.getChannelWidth();
    ccaConfigurationRevision = snapshot.getConfigurationRevision();
    primary20CcaBusy = snapshot.isPrimary20Busy();
    secondary20CcaBusy = snapshot.isSecondary20Busy();
    secondary40CcaBusy = snapshot.isSecondary40Busy();
    secondary80CcaBusy = snapshot.isSecondary80Busy();
    bool resetHistories = oldEnabled != ccaEnabled || oldWidth != ccaChannelWidth || oldRevision != ccaConfigurationRevision;
    bool secondary20Available = ccaEnabled && ccaChannelWidth != physicallayer::IEEE80211_CHANNEL_WIDTH_20MHZ;
    bool secondary40Available = ccaEnabled && (ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80MHZ ||
            ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ || ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ);
    bool secondary80Available = ccaEnabled && (ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_160MHZ ||
            ccaChannelWidth == physicallayer::IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ);
    auto updateHistory = [&] (bool available, bool oldAvailable, bool busy, simtime_t& idleSince) {
        if (!available || busy)
            idleSince = -1;
        else if (resetHistories || !oldAvailable || idleSince < SIMTIME_ZERO)
            idleSince = simTime();
    };
    updateHistory(secondary20Available, oldSecondary20Available, secondary20CcaBusy, secondary20CcaIdleSince);
    updateHistory(secondary40Available, oldSecondary40Available, secondary40CcaBusy, secondary40CcaIdleSince);
    updateHistory(secondary80Available, oldSecondary80Available, secondary80CcaBusy, secondary80CcaIdleSince);
    recomputeMediumFree();
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
    ASSERT(navInterval >= 0);
    if (navInterval > 0) {
        simtime_t endNav = simTime() + navInterval;
        if (endNavTimer->isScheduled()) {
            simtime_t oldEndNav = endNavTimer->getArrivalTime();
            if (endNav < oldEndNav)
                return; // never decrease NAV
            emit(navChangedSignal, endNavTimer->getArrivalTime() - simTime());
            cancelEvent(endNavTimer);
        }
        else
            emit(navChangedSignal, SimTime::ZERO);
        EV_INFO << "Setting NAV to " << navInterval << std::endl;
        scheduleAt(endNav, endNavTimer);
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
