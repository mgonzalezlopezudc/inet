//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandNoiseBase.h"
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
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211CcaListening.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

#include <cmath>
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

bool isAlignedCcaSignal(const FrequencyBand& listeningBand, const FrequencyBand& signalBand)
{
    if (!listeningBand.contains(signalBand))
        return false;
    // Secondary preamble detection is defined on aligned 20 MHz subchannels,
    // not on an arbitrary narrowband signal placed somewhere inside the CCA
    // group.  A 40/80 MHz signal must likewise start on its corresponding
    // 40/80 MHz subchannel boundary.
    const double slotWidth = MHz(20).get();
    const double groupSlotsReal = listeningBand.bandwidth.get() / slotWidth;
    const double signalSlotsReal = signalBand.bandwidth.get() / slotWidth;
    const auto groupSlots = static_cast<int>(std::llround(groupSlotsReal));
    const auto signalSlots = static_cast<int>(std::llround(signalSlotsReal));
    if (groupSlots <= 0 || signalSlots <= 0 ||
            std::fabs(groupSlotsReal - groupSlots) > 1e-9 ||
            std::fabs(signalSlotsReal - signalSlots) > 1e-9 ||
            groupSlots % signalSlots != 0)
        return false;
    const double offsetSlotsReal = (signalBand.getLowerFrequency() - listeningBand.getLowerFrequency()).get() / slotWidth;
    const auto offsetSlots = static_cast<int>(std::llround(offsetSlotsReal));
    return std::fabs(offsetSlotsReal - offsetSlots) <= 1e-9 && offsetSlots % signalSlots == 0;
}

bool isRecognizableCcaMode(const IIeee80211Mode *mode)
{
    // CCA preamble recognition is a PHY classification decision.  It must
    // not depend on whether the receiver's configured operation-mode set
    // happens to contain the exact transmitting mode.  ERP-OFDM is an OFDM
    // mode and is therefore covered by the OFDM base class; HT and VHT are
    // listed explicitly for clarity and for mode implementations that do not
    // share that inheritance.
    return dynamic_cast<const Ieee80211OfdmMode *>(mode) != nullptr ||
            dynamic_cast<const Ieee80211HtMode *>(mode) != nullptr ||
            dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr;
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
    if (modeSet == nullptr || mode == nullptr || !modeSet->supportsMode(mode))
        return false;
    return mode->getDataMode()->getBandwidth() < MHz(80) || dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr;
}

bool isPrimaryComponentDetectable(const Ieee80211Channel *channel, const IReception *reception, W sensitivity)
{
    const auto *multibandSignal = dynamic_cast<const IMultibandSignalAnalogModel *>(reception->getAnalogModel());
    if (multibandSignal == nullptr || multibandSignal->getOccupiedBands().size() <= 1)
        return true;

    // A composite PPDU is eligible only when the component that contains the
    // configured primary 20 MHz is itself detectable.  Testing the aggregate
    // envelope would allow a strong remote 80 MHz segment to make a weak
    // primary segment appear as RXSTART-capable.
    const auto *dimensionalSignal = dynamic_cast<const DimensionalSignalAnalogModel *>(reception->getAnalogModel());
    if (dimensionalSignal == nullptr || dimensionalSignal->getComponentPowers().size() != multibandSignal->getOccupiedBands().size())
        return false;
    const auto primary20 = channel->getPrimary20Band();
    auto bands = multibandSignal->getOccupiedBands();
    const auto& components = dimensionalSignal->getComponentPowers();
    for (size_t index = 0; index < bands.size(); ++index) {
        const auto& componentBand = bands[index];
        const auto lower = std::max(componentBand.getLowerFrequency(), primary20.getLowerFrequency());
        const auto upper = std::min(componentBand.getUpperFrequency(), primary20.getUpperFrequency());
        if (lower < upper) {
            Point<simsec, Hz> primaryStart(simsec(reception->getStartTime()), lower);
            Point<simsec, Hz> primaryEnd(simsec(reception->getEndTime()), upper);
            auto primaryInterval = Interval<simsec, Hz>(primaryStart, primaryEnd, 0b11, 0b11, 0b00);
            auto componentInterval = components[index]->getDomain().getIntersected(primaryInterval);
            if (componentInterval.isEmpty())
                return false;
            auto primaryComponent = makeShared<DomainLimitedFunction<WpHz, Domain<simsec, Hz>>>(components[index], componentInterval);
            Point<simsec> componentStart(simsec(reception->getStartTime()));
            Point<simsec> componentEnd(simsec(reception->getEndTime()));
            auto primaryPower = integrate<WpHz, Domain<simsec, Hz>, 0b10, W, Domain<simsec>>(primaryComponent)->getMin(
                    Interval<simsec>(componentStart, componentEnd, 0b1, 0b1, 0b0));
            return primaryPower >= sensitivity;
        }
    }
    return false;
}

W computeDimensionalMinimumPower(const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& power,
        simtime_t startTime, simtime_t endTime)
{
    Point<simsec> startPoint{simsec(startTime)};
    Point<simsec> endPoint{simsec(endTime)};
    return integrate<WpHz, Domain<simsec, Hz>, 0b10, W, Domain<simsec>>(power)->getMin(
            Interval<simsec>(startPoint, endPoint, 0b1, 0b1, 0b0));
}

std::vector<FrequencyBand> getSignalBands(const IReception *reception)
{
    const auto *analogModel = reception->getAnalogModel();
    if (const auto *multiband = dynamic_cast<const IMultibandSignalAnalogModel *>(analogModel))
        return multiband->getOccupiedBands();
    if (const auto *narrowband = dynamic_cast<const INarrowbandSignalAnalogModel *>(analogModel))
        return {FrequencyBand(narrowband->getCenterFrequency(), narrowband->getBandwidth())};
    return {};
}

W computeSignalPowerInRange(const IReception *reception, const FrequencyBand& measurementBand)
{
    const auto *analogModel = reception->getAnalogModel();
    const auto *multiband = dynamic_cast<const IMultibandSignalAnalogModel *>(analogModel);
    if (multiband != nullptr) {
        const auto *dimensional = dynamic_cast<const DimensionalSignalAnalogModel *>(analogModel);
        if (dimensional == nullptr || dimensional->getComponentPowers().size() != multiband->getOccupiedBands().size())
            return W(NaN);
        const auto& occupiedBands = multiband->getOccupiedBands();
        for (size_t index = 0; index < occupiedBands.size(); ++index) {
            const auto& signalBand = occupiedBands[index];
            const auto lower = std::max(signalBand.getLowerFrequency(), measurementBand.getLowerFrequency());
            const auto upper = std::min(signalBand.getUpperFrequency(), measurementBand.getUpperFrequency());
            if (lower >= upper)
                continue;
            Point<simsec, Hz> startPoint(simsec(reception->getStartTime()), lower);
            Point<simsec, Hz> endPoint(simsec(reception->getEndTime()), upper);
            auto requestedInterval = Interval<simsec, Hz>(startPoint, endPoint, 0b11, 0b11, 0b00);
            auto componentInterval = dimensional->getComponentPowers()[index]->getDomain().getIntersected(requestedInterval);
            if (componentInterval.isEmpty())
                return W(0);
            auto component = makeShared<DomainLimitedFunction<WpHz, Domain<simsec, Hz>>>(dimensional->getComponentPowers()[index], componentInterval);
            return computeDimensionalMinimumPower(component, reception->getStartTime(), reception->getEndTime());
        }
        return W(0);
    }
    if (const auto *dimensional = dynamic_cast<const IDimensionalSignalAnalogModel *>(analogModel)) {
        const auto lower = std::max(std::get<1>(dimensional->getPower()->getDomain().getLower()), measurementBand.getLowerFrequency());
        const auto upper = std::min(std::get<1>(dimensional->getPower()->getDomain().getUpper()), measurementBand.getUpperFrequency());
        if (lower >= upper)
            return W(0);
        Point<simsec, Hz> startPoint(simsec(reception->getStartTime()), lower);
        Point<simsec, Hz> endPoint(simsec(reception->getEndTime()), upper);
        auto requestedInterval = Interval<simsec, Hz>(startPoint, endPoint, 0b11, 0b11, 0b00);
        auto componentInterval = dimensional->getPower()->getDomain().getIntersected(requestedInterval);
        if (componentInterval.isEmpty())
            return W(0);
        auto component = makeShared<DomainLimitedFunction<WpHz, Domain<simsec, Hz>>>(dimensional->getPower(), componentInterval);
        return computeDimensionalMinimumPower(component, reception->getStartTime(), reception->getEndTime());
    }
    if (const auto *narrowband = dynamic_cast<const INarrowbandSignalAnalogModel *>(analogModel)) {
        FrequencyBand signalBand(narrowband->getCenterFrequency(), narrowband->getBandwidth());
        const auto lower = std::max(signalBand.getLowerFrequency(), measurementBand.getLowerFrequency());
        const auto upper = std::min(signalBand.getUpperFrequency(), measurementBand.getUpperFrequency());
        if (lower >= upper || signalBand.bandwidth <= Hz(0))
            return W(0);
        return narrowband->computeMinPower(reception->getStartTime(), reception->getEndTime()) *
                ((upper - lower).get() / signalBand.bandwidth.get());
    }
    return W(NaN);
}

W computeScalarCcaEnergy(const BandListening *listening, const IInterference *interference)
{
    const auto listeningMin = listening->getCenterFrequency() - listening->getBandwidth() / 2;
    const auto listeningMax = listening->getCenterFrequency() + listening->getBandwidth() / 2;
    auto overlapFraction = [&] (Hz signalCenter, Hz signalBandwidth) {
        const auto signalMin = signalCenter - signalBandwidth / 2;
        const auto signalMax = signalCenter + signalBandwidth / 2;
        const auto overlapMin = std::max(listeningMin, signalMin);
        const auto overlapMax = std::min(listeningMax, signalMax);
        return overlapMin < overlapMax && signalBandwidth > Hz(0) ?
                (overlapMax - overlapMin).get() / signalBandwidth.get() : 0.0;
    };
    W totalPower = W(0);
    if (const auto *background = dynamic_cast<const NarrowbandNoiseBase *>(interference->getBackgroundNoise()))
        totalPower += background->computeMaxPower(listening->getStartTime(), listening->getEndTime()) *
                overlapFraction(background->getCenterFrequency(), background->getBandwidth());
    for (const auto *reception : *interference->getInterferingReceptions()) {
        const auto *signal = dynamic_cast<const INarrowbandSignalAnalogModel *>(reception->getAnalogModel());
        if (signal != nullptr)
            totalPower += signal->computeMinPower(reception->getStartTime(), reception->getEndTime()) *
                    overlapFraction(signal->getCenterFrequency(), signal->getBandwidth());
    }
    return totalPower;
}

W computeGroupedCcaEnergyDetectionThreshold(Ieee80211CcaGroup group, W primaryThreshold)
{
    double deltaDb = group == IEEE80211_CCA_SECONDARY40 ? 3.0 :
            group == IEEE80211_CCA_SECONDARY80 ? 6.0 : 0.0;
    return primaryThreshold * std::pow(10.0, deltaDb / 10.0);
}

W computeGroupedCcaSignalDetectionThreshold(Ieee80211CcaGroup group, Hz bandwidth, W primaryThreshold,
        W secondary20Threshold, W secondary80Threshold)
{
    if (group == IEEE80211_CCA_PRIMARY20)
        return bandwidth == MHz(20) ? primaryThreshold : W(NaN);
    if (group == IEEE80211_CCA_SECONDARY20)
        return bandwidth == MHz(20) ? secondary20Threshold : W(NaN);
    if (group == IEEE80211_CCA_SECONDARY40)
        return (bandwidth == MHz(20) || bandwidth == MHz(40)) ? secondary20Threshold : W(NaN);
    if (group == IEEE80211_CCA_SECONDARY80) {
        if (bandwidth == MHz(80))
            return secondary80Threshold;
        if (bandwidth == MHz(20) || bandwidth == MHz(40))
            return secondary20Threshold;
    }
    return W(NaN);
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

W Ieee80211Receiver::getGroupedCcaEnergyDetectionThreshold(Ieee80211CcaGroup group, W primaryThreshold)
{
    return computeGroupedCcaEnergyDetectionThreshold(group, primaryThreshold);
}

W Ieee80211Receiver::getGroupedCcaSignalDetectionThreshold(Ieee80211CcaGroup group, Hz bandwidth,
        W primaryThreshold, W secondary20Threshold, W secondary80Threshold)
{
    return computeGroupedCcaSignalDetectionThreshold(group, bandwidth, primaryThreshold, secondary20Threshold, secondary80Threshold);
}

void Ieee80211Receiver::initialize(int stage)
{
    FlatReceiverBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        htCca20Sensitivity = mW(math::dBmW2mW(par("htCca20Sensitivity")));
        htCca40Sensitivity = mW(math::dBmW2mW(par("htCca40Sensitivity")));
        htCcaEnergyDetection = mW(math::dBmW2mW(par("htCcaEnergyDetection")));
        ccaSecondary20Sensitivity = mW(math::dBmW2mW(par("ccaSecondary20Sensitivity")));
        ccaSecondary80Sensitivity = mW(math::dBmW2mW(par("ccaSecondary80Sensitivity")));
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
        if (radio->getMedium() == nullptr || dynamic_cast<const DimensionalMediumAnalogModel *>(radio->getMedium()->getAnalogModel()) == nullptr)
            throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires a dimensional medium analog model");
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
    if (ieee80211Transmission == nullptr || !isModeAccepted(modeSet, ieee80211Transmission->getMode()) || !isSignalOnPrimary20(channel, transmission))
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
    return ieee80211Transmission && isModeAccepted(modeSet, ieee80211Transmission->getMode()) &&
            isSignalOnPrimary20(channel, reception->getTransmission()) &&
            isPrimaryComponentDetectable(channel, reception, sensitivity) &&
            getAnalogModel()->computeIsReceptionPossible(listening, reception, sensitivity);
}

const IListeningDecision *Ieee80211Receiver::computeListeningDecision(const IListening *listening, const IInterference *interference) const
{
    if (const auto *ccaListening = dynamic_cast<const Ieee80211CcaListening *>(listening)) {
        bool busy = ccaListening->isLegacyHt40() ? computeHtCcaBusy(listening, interference) : computeGroupedCcaBusy(listening, interference);
        return new ListeningDecision(listening, busy);
    }
    if (isHtCcaOperation() && dynamic_cast<const BandListening *>(listening) != nullptr &&
            dynamic_cast<const BandListening *>(listening)->getBandwidth() == MHz(20))
        return new ListeningDecision(listening, computeHtCcaBusy(listening, interference));
    return FlatReceiverBase::computeListeningDecision(listening, interference);
}

bool Ieee80211Receiver::computeGroupedCcaBusy(const IListening *listening, const IInterference *interference) const
{
    const auto *ccaListening = check_and_cast<const Ieee80211CcaListening *>(listening);
    const auto *medium = listening->getReceiverRadio() == nullptr ? nullptr : listening->getReceiverRadio()->getMedium();
    const auto *mediumAnalogModel = medium == nullptr ? nullptr : medium->getAnalogModel();
    W energy = W(0);
    if (dynamic_cast<const ScalarMediumAnalogModel *>(mediumAnalogModel) != nullptr)
        energy = computeScalarCcaEnergy(ccaListening, interference);
    else if (mediumAnalogModel != nullptr) {
        const INoise *noise = mediumAnalogModel->computeNoise(listening, interference);
        energy = noise->computeMaxPower(listening->getStartTime(), listening->getEndTime());
        delete noise;
    }
    if (energy >= getGroupedCcaEnergyDetectionThreshold(ccaListening->getGroup(), htCcaEnergyDetection))
        return true;

    const FrequencyBand listeningBand(ccaListening->getCenterFrequency(), ccaListening->getBandwidth());
    for (const auto *reception : *interference->getInterferingReceptions()) {
        const auto *transmission = dynamic_cast<const Ieee80211Transmission *>(reception->getTransmission());
        if (transmission == nullptr || !isRecognizableCcaMode(transmission->getMode()))
            continue;
        for (const auto& signalBand : getSignalBands(reception)) {
            bool primaryGroup = ccaListening->getGroup() == IEEE80211_CCA_PRIMARY20;
            if (primaryGroup) {
                // A wide PPDU is recognized on the primary 20 MHz slice.  Its
                // component is larger than the listening slice, so full-band
                // containment is intentionally not used here.
                if (signalBand.bandwidth < MHz(20) || !overlaps(listeningBand, signalBand))
                    continue;
            }
            else if (!isAlignedCcaSignal(listeningBand, signalBand))
                continue; // Partial overlap contributes only to aggregate ED.
            const auto measuredBand = primaryGroup ? listeningBand : signalBand;
            W threshold = getGroupedCcaSignalDetectionThreshold(ccaListening->getGroup(), measuredBand.bandwidth,
                    htCca20Sensitivity, ccaSecondary20Sensitivity, ccaSecondary80Sensitivity);
            if (!std::isnan(threshold.get())) {
                W signalPower = computeSignalPowerInRange(reception, measuredBand);
                if (!std::isnan(signalPower.get()) && signalPower >= threshold)
                    return true;
            }
        }
    }
    return false;
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
