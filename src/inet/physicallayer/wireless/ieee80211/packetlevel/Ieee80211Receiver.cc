//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IMultibandReceiverAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IMultibandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/MultibandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ListeningDecision.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

#include <memory>

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Receiver);

namespace {

const Ieee80211Channel *cloneChannelForBand(const Ieee80211Channel *channel, const IIeee80211Band *band)
{
    if (channel == nullptr)
        return nullptr;
    if (band == nullptr)
        throw cRuntimeError("Cannot retain an IEEE 802.11 channel without a band");
    if (channel->isExplicitGeometry())
        return new Ieee80211Channel(band, channel->getChannelNumber(), channel->getChannelWidth(),
                channel->getCenterFrequencyIndex0(), channel->getCenterFrequencyIndex1());
    return new Ieee80211Channel(band, channel->getChannelNumber(), channel->getSecondaryChannelOffset());
}

bool overlaps(const FrequencyBand& first, const FrequencyBand& second)
{
    return first.getLowerFrequency() < second.getUpperFrequency() && second.getLowerFrequency() < first.getUpperFrequency();
}

bool isSignalOnPrimary20(const Ieee80211Channel *channel, const ITransmission *transmission)
{
    if (channel == nullptr)
        return true;
    const auto *multibandSignal = dynamic_cast<const IMultibandSignalAnalogModel *>(transmission->getAnalogModel());
    if (multibandSignal != nullptr) {
        for (const auto& band : multibandSignal->getOccupiedBands())
            if (Ieee80211Receiver::isPrimary20Overlapping(channel, band))
                return true;
        return false;
    }
    const auto *narrowbandSignal = dynamic_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
    return narrowbandSignal != nullptr && Ieee80211Receiver::isPrimary20Overlapping(channel,
            FrequencyBand(narrowbandSignal->getCenterFrequency(), narrowbandSignal->getBandwidth()));
}

bool isModeAccepted(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode)
{
    if (modeSet == nullptr || mode == nullptr || !modeSet->containsMode(mode))
        return false;
    return mode->getDataMode()->getBandwidth() < MHz(80) || dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr;
}

}

Ieee80211Receiver::~Ieee80211Receiver()
{
    delete channel;
}

bool Ieee80211Receiver::isPrimary20Overlapping(const Ieee80211Channel *channel, const FrequencyBand& signalBand)
{
    if (channel == nullptr)
        return true;
    const auto primary20 = channel->getPrimary20Band();
    return overlaps(primary20, signalBand);
}

bool Ieee80211Receiver::isPrimary20Overlapping(const Ieee80211Channel *channel, const std::vector<FrequencyBand>& signalBands)
{
    for (const auto& signalBand : signalBands)
        if (isPrimary20Overlapping(channel, signalBand))
            return true;
    return false;
}

void Ieee80211Receiver::initialize(int stage)
{
    FlatReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        htCca20Sensitivity = mW(math::dBmW2mW(par("htCca20Sensitivity")));
        htCca40Sensitivity = mW(math::dBmW2mW(par("htCca40Sensitivity")));
        htCcaEnergyDetection = mW(math::dBmW2mW(par("htCcaEnergyDetection")));
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

const IListening *Ieee80211Receiver::createListening(const IRadio *radio, const simtime_t startTime, const simtime_t endTime,
        const Coord& startPosition, const Coord& endPosition) const
{
    if (channel == nullptr)
        return NarrowbandReceiverBase::createListening(radio, startTime, endTime, startPosition, endPosition);
    if (channel->is80Plus80()) {
        auto *multibandFactory = dynamic_cast<const IMultibandReceiverAnalogModel *>(getAnalogModel());
        if (multibandFactory == nullptr)
            throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires a multiband receiver analog model");
        return multibandFactory->createListening(radio, startTime, endTime, startPosition, endPosition, channel->getOccupiedBands());
    }
    return getAnalogModel()->createListening(radio, startTime, endTime, startPosition, endPosition,
            channel->getOperatingCenterFrequency(), channel->getOperatingBandwidth());
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (ieee80211Transmission == nullptr || !modeSet->supportsMode(ieee80211Transmission->getMode()) || !isSignalOnPrimary20(channel, transmission))
        return false;
    auto *multibandListening = dynamic_cast<const MultibandListening *>(listening);
    auto *multibandSignal = dynamic_cast<const IMultibandSignalAnalogModel *>(transmission->getAnalogModel());
    if (multibandListening != nullptr) {
        if (multibandSignal != nullptr)
            return multibandListening->contains(multibandSignal->getOccupiedBands());
        auto *narrowbandSignal = dynamic_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
        return narrowbandSignal != nullptr && multibandListening->contains(FrequencyBand(narrowbandSignal->getCenterFrequency(), narrowbandSignal->getBandwidth()));
    }
    // A multiband signal must be paired with the explicit multiband listening
    // mask.  Its outer envelope is metadata only and must not hide a missing
    // receiver factory or make the spectral gap part of the decode domain.
    if (multibandSignal != nullptr)
        return false;
    return NarrowbandReceiverBase::computeIsReceptionPossible(listening, transmission);
}

bool Ieee80211Receiver::computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const
{
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
    return ieee80211Transmission && modeSet->supportsMode(ieee80211Transmission->getMode()) &&
            isSignalOnPrimary20(channel, reception->getTransmission()) && getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

const IListeningDecision *Ieee80211Receiver::computeListeningDecision(const IListening *listening, const IInterference *interference) const
{
    if (isHtCcaOperation() && dynamic_cast<const BandListening *>(listening) != nullptr &&
            dynamic_cast<const BandListening *>(listening)->getBandwidth() == MHz(20))
        return new ListeningDecision(listening, computeHtCcaBusy(listening, interference));
    return FlatReceiverBase::computeListeningDecision(listening, interference);
}

bool Ieee80211Receiver::isHtCcaOperation() const
{
    return modeSet != nullptr && modeSet->isHtOperationSupported() &&
            channel != nullptr && (bandwidth == MHz(20) ||
            (bandwidth == MHz(40) && channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE));
}

static bool isBandOverlapping(const BandListening *listening, const INarrowbandSignalAnalogModel *signal)
{
    auto listeningMin = listening->getCenterFrequency() - listening->getBandwidth() / 2;
    auto listeningMax = listening->getCenterFrequency() + listening->getBandwidth() / 2;
    auto signalMin = signal->getCenterFrequency() - signal->getBandwidth() / 2;
    auto signalMax = signal->getCenterFrequency() + signal->getBandwidth() / 2;
    return signalMin <= listeningMax && signalMax >= listeningMin;
}

static bool isPrimaryChannel(const Ieee80211Channel *channel, const BandListening *listening)
{
    return channel != nullptr && listening->getCenterFrequency() == channel->getCenterFrequency();
}

static bool isSecondaryChannel(const Ieee80211Channel *channel, const BandListening *listening)
{
    return channel != nullptr && channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE &&
            listening->getCenterFrequency() == channel->getSecondaryCenterFrequency();
}

static bool isHt40SignalOccupyingChannel(const Ieee80211Channel *channel, const INarrowbandSignalAnalogModel *signal)
{
    return channel != nullptr && signal->getBandwidth() == MHz(40) &&
            signal->getCenterFrequency() == channel->getBondedCenterFrequency();
}

bool Ieee80211Receiver::computeHtCcaBusy(const IListening *listening, const IInterference *interference) const
{
    const auto *bandListening = check_and_cast<const BandListening *>(listening);
    const auto *mediumAnalogModel = listening->getReceiverRadio()->getMedium()->getAnalogModel();
    bool busy = false;
    if (dynamic_cast<const ScalarMediumAnalogModel *>(mediumAnalogModel) != nullptr) {
        W totalPower = W(0);
        const auto *backgroundNoise = interference->getBackgroundNoise();
        if (backgroundNoise != nullptr)
            totalPower += backgroundNoise->computeMaxPower(listening->getStartTime(), listening->getEndTime());
        const auto listeningMin = bandListening->getCenterFrequency() - bandListening->getBandwidth() / 2;
        const auto listeningMax = bandListening->getCenterFrequency() + bandListening->getBandwidth() / 2;
        for (auto reception : *interference->getInterferingReceptions()) {
            const auto *signal = dynamic_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
            if (signal != nullptr) {
                const auto signalMin = signal->getCenterFrequency() - signal->getBandwidth() / 2;
                const auto signalMax = signal->getCenterFrequency() + signal->getBandwidth() / 2;
                const auto overlapMin = std::max(listeningMin, signalMin);
                const auto overlapMax = std::min(listeningMax, signalMax);
                if (overlapMin < overlapMax && signal->getBandwidth() > Hz(0)) {
                    double fraction = (overlapMax - overlapMin).get() / signal->getBandwidth().get();
                    totalPower += signal->computeMinPower(reception->getStartTime(), reception->getEndTime()) * fraction;
                }
            }
        }
        busy = totalPower >= htCcaEnergyDetection;
    }
    else {
        const INoise *noise = mediumAnalogModel->computeNoise(listening, interference);
        busy = noise->computeMaxPower(listening->getStartTime(), listening->getEndTime()) >= htCcaEnergyDetection;
        delete noise;
    }
    if (busy)
        return true;

    const bool primary = isPrimaryChannel(channel, bandListening);
    const bool secondary = isSecondaryChannel(channel, bandListening);
    if (!primary && !secondary)
        return false;

    for (auto reception : *interference->getInterferingReceptions()) {
        const auto *transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
        const auto *signal = dynamic_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
        if (transmission == nullptr || transmission->getMode() == nullptr || signal == nullptr ||
                !modeSet->supportsMode(transmission->getMode()) || !isBandOverlapping(bandListening, signal))
            continue;

        const W signalPower = signal->computeMinPower(reception->getStartTime(), reception->getEndTime());
        const IIeee80211Mode *mode = transmission->getMode();
        bool isHt = dynamic_cast<const Ieee80211HtMode *>(mode) != nullptr;
        bool isOfdmOrErp = (dynamic_cast<const Ieee80211OfdmMode *>(mode) != nullptr) ||
                           (dynamic_cast<const Ieee80211ErpOfdmMode *>(mode) != nullptr);
        const Hz signalBandwidth = mode->getDataMode()->getBandwidth();
        if (isHt) {
            // IEEE Std 802.11-2024, 19.3.19.6.4 and 19.3.19.6.5:
            // a 20 MHz HT signal is detected on the primary channel at
            // -82 dBm; a 40 MHz HT signal is detected on each occupied
            // channel at -79 dBm. The comparison uses the received signal
            // level over the PPDU bandwidth, not the power apportioned to a
            // 20 MHz slice by the analog interference model.
            if (signalBandwidth == MHz(40) && isHtCcaOperation() &&
                    isHt40SignalOccupyingChannel(channel, signal) && signalPower >= htCca40Sensitivity)
                return true;
            if (signalBandwidth == MHz(20) && primary && signalPower >= htCca20Sensitivity)
                return true;
        }
        else if (primary && isOfdmOrErp && signalPower >= htCca20Sensitivity) {
            // Clause 19.3.19.6.3 delegates non-HT CCA to the OFDM/ERP-OFDM
            // preamble-detection requirement, which uses the 20 MHz
            // sensitivity threshold.
            return true;
        }
    }
    return false;
}

const IReceptionResult *Ieee80211Receiver::computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const
{
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto receptionResult = FlatReceiverBase::computeReceptionResult(listening, reception, interference, snir, decisions);
    auto packet = const_cast<Packet *>(receptionResult->getPacket());
    packet->addTagIfAbsent<Ieee80211ModeInd>()->setMode(transmission->getMode());
    packet->addTagIfAbsent<Ieee80211ChannelInd>()->setChannel(transmission->getChannel());
    return receptionResult;
}

void Ieee80211Receiver::setModeSet(const Ieee80211ModeSet *modeSet)
{
    this->modeSet = modeSet;
}

std::function<void()> Ieee80211Receiver::saveChannelState()
{
    auto savedChannel = std::make_shared<std::unique_ptr<const Ieee80211Channel>>();
    if (channel != nullptr)
        savedChannel->reset(new Ieee80211Channel(*channel));
    return [this, savedChannel, oldBand = band, oldBandwidth = bandwidth, oldCenterFrequency = centerFrequency]() {
        delete channel;
        channel = savedChannel->release();
        band = oldBand;
        bandwidth = oldBandwidth;
        centerFrequency = oldCenterFrequency;
    };
}

void Ieee80211Receiver::setBand(const IIeee80211Band *band)
{
    if (this->band != band) {
        std::unique_ptr<const Ieee80211Channel> replacement(cloneChannelForBand(channel, band));
        this->band = band;
        if (replacement != nullptr)
            setChannel(replacement.release());
    }
}

void Ieee80211Receiver::setChannel(const Ieee80211Channel *channel)
{
    if (this->channel != channel) {
        // IEEE Std 802.11-2024, 19.3.15.4 and 19.3.19.6.5: the receiver
        // listens on the configured operating geometry; HT40 therefore uses
        // the bonded center and VHT widths use their primary hierarchy.
        auto centerFrequency = channel->getOperatingCenterFrequency();
        delete this->channel;
        this->channel = channel;
        this->band = channel->getBand();
        setCenterFrequency(centerFrequency);
    }
}

void Ieee80211Receiver::setChannelNumber(int channelNumber)
{
    if (channel == nullptr || channelNumber != channel->getChannelNumber())
        setChannel(channel != nullptr && channel->isExplicitGeometry() ?
                new Ieee80211Channel(band, channelNumber, channel->getChannelWidth(), channel->getCenterFrequencyIndex0(), channel->getCenterFrequencyIndex1()) :
                new Ieee80211Channel(band, channelNumber, channel == nullptr ? IEEE80211_SECONDARY_CHANNEL_NONE : channel->getSecondaryChannelOffset()));
}

bool Ieee80211Receiver::isHtChannelWidthSupported(Hz channelWidth) const
{
    bool channelConfigured = channelWidth == MHz(20) ||
            (channelWidth == MHz(40) && channel != nullptr &&
             channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE);
    return channelConfigured && channelWidth <= getBandwidth() && modeSet != nullptr &&
            modeSet->getHtSupportedChannelWidths().count(channelWidth) != 0;
}

} // namespace physicallayer

} // namespace inet
