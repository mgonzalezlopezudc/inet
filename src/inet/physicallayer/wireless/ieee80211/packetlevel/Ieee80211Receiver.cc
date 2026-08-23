//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ReceivedDataEncodingPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Receiver);

Ieee80211Receiver::~Ieee80211Receiver()
{
    delete channel;
}

void Ieee80211Receiver::initialize(int stage)
{
    FlatReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        const char *opMode = par("opMode");
        setModeSet(*opMode ? Ieee80211ModeSet::getModeSet(opMode) : nullptr);
        const char *bandName = par("bandName");
        setBand(*bandName != '\0' ? Ieee80211CompliantBands::getBand(bandName) : nullptr);
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
    }
}

std::ostream& Ieee80211Receiver::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Receiver";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(modeSet, printFieldToString(modeSet, level + 1, evFlags))
               << EV_FIELD(band, printFieldToString(band, level + 1, evFlags));
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(channel, printFieldToString(channel, level + 1, evFlags));
    return FlatReceiverBase::printToStream(stream, level);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) && NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    return ieee80211Transmission && modeSet->containsMode(ieee80211Transmission->getMode()) && getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

const IReceptionResult *Ieee80211Receiver::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto receptionResult = FlatReceiverBase::computeReceptionResult(listening, reception, interference, snir, decisions);
    auto packet = const_cast<Packet *>(receptionResult->getPacket());
    packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
    packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
    auto phyFormat = transmission->getMode()->getDataMode()->getPhyFormat();
    if (phyFormat == Ieee80211PhyFormat::HT || phyFormat == Ieee80211PhyFormat::VHT_SU) {
        try {
            auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet,
                    b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                    Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
            if (!phyHeader->isIncorrect() && !phyHeader->isIncomplete() && !phyHeader->isImproperlyRepresented()) {
                packet->addTagIfAbsent<Ieee80211DataEncodingPlanTag>()->setPlan(
                        reconstructIeee80211ReceivedDataEncodingPlan(
                                transmission->getMode()->getDataMode(), phyHeader,
                                transmission->getDataDuration()));
                if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
                    vhtHeader != nullptr && transmission->getMode()->getDataMode()->getFecType() == Ieee80211FecType::LDPC) {
                    // This RXVECTOR indication is reconstructed solely from
                    // the received VHT-SIG-B Length field. It is rounded to
                    // four octets and contains no sender-side exact APEP
                    // metadata; the MAC recovers the exact MPDU length from
                    // the decoded A-MPDU delimiter.
                    packet->addTagIfAbsent<Ieee80211VhtApepInd>()->setApepLength(
                            decodeVhtSuSigBLength(vhtHeader->getVhtSigBLength()).get<B>());
                }
            }
        }
        catch (const cRuntimeError& error) {
            // Missing plan context makes validateHtOrVhtHeader mark the
            // received packet erroneous during radio decapsulation.
            EV_DEBUG << "Cannot reconstruct received IEEE 802.11 data encoding plan: "
                     << error.what() << endl;
        }
    }
    return receptionResult;
}

void Ieee80211Receiver::setModeSet(const Ieee80211ModeSet *modeSet)
{
    this->modeSet = modeSet;
}

void Ieee80211Receiver::setBand(const IIeee80211Band *band)
{
    if (this->band != band) {
        this->band = band;
        if (channel != nullptr)
            setChannel(new Ieee80211Channel(band, channel->getChannelNumber()));
    }
}

void Ieee80211Receiver::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        delete this->channel;
        this->channel = channel;
        setCenterFrequency(channel->getCenterFrequency());
    }
}

void Ieee80211Receiver::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(new Ieee80211Channel(band, channelNumber));
}

} // namespace physicallayer

} // namespace inet
