//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.h"

#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ReceivedDataEncodingPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

namespace inet {

namespace physicallayer {

Ieee80211ErrorModelBase::Ieee80211ErrorModelBase()
{
}

double Ieee80211ErrorModelBase::computeHeaderPacketErrorRate(const ISnir *snir,
        IRadioSignal::SignalPart part, const IIeee80211Mode *mode) const
{
    Enter_Method("computeHeaderPacketErrorRate");
    if (snir == nullptr || mode == nullptr)
        throw cRuntimeError("Cannot compute an IEEE 802.11 PHY-header error rate without SNIR and mode");
    if (part == IRadioSignal::SIGNAL_PART_PREAMBLE)
        return 0;
    if (part != IRadioSignal::SIGNAL_PART_HEADER)
        throw cRuntimeError("Expected IEEE 802.11 preamble or header, got signal part '%s'",
                IRadioSignal::getSignalPartName(part));
    auto headerLength = mode->getHeaderMode()->getLength();
    return 1.0 - getHeaderSuccessRate(mode, headerLength.get<b>(), getScalarSnir(snir));
}

void Ieee80211ErrorModelBase::initialize(int stage)
{
    ErrorModelBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        auto module = getSubmodule("fecSuccessModel");
        if (module != nullptr) {
            fecSuccessModel = dynamic_cast<const IIeee80211FecSuccessModel *>(module);
            if (fecSuccessModel == nullptr)
                throw cRuntimeError("fecSuccessModel submodule does not implement IIeee80211FecSuccessModel");
        }
    }
}

double Ieee80211ErrorModelBase::computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const
{
    Enter_Method("computePacketErrorRate");
    auto transmission = check_and_cast<const Ieee80211Transmission *>(snir->getReception()->getTransmission());
    auto mode = transmission->getMode();
    auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
    auto headerLength = mode->getHeaderMode()->getLength();
    double scalarSnir = getScalarSnir(snir);

    // Preserve the legacy BCC calculation path, including calculating both
    // terms before selecting the requested signal part.
    if (mode->getDataMode()->getFecType() == Ieee80211FecType::BCC) {
        auto dataLength = b(mode->getDataMode()->getCompleteLength(B(phyHeader->getLengthField())));
        double headerSuccessRate = getHeaderSuccessRate(mode, headerLength.get<b>(), scalarSnir);
        double dataSuccessRate = getBccDataSuccessRate(mode, dataLength.get<b>(), scalarSnir);
        switch (part) {
            case IRadioSignal::SIGNAL_PART_WHOLE:
                return 1.0 - headerSuccessRate * dataSuccessRate;
            case IRadioSignal::SIGNAL_PART_PREAMBLE:
                return 0;
            case IRadioSignal::SIGNAL_PART_HEADER:
                return 1.0 - headerSuccessRate;
            case IRadioSignal::SIGNAL_PART_DATA:
                return 1.0 - dataSuccessRate;
            default:
                throw cRuntimeError("Unknown signal part: '%s'", IRadioSignal::getSignalPartName(part));
        }
    }

    // PHY headers remain BCC-coded. Header-only queries must not require an
    // LDPC data curve.
    if (part == IRadioSignal::SIGNAL_PART_PREAMBLE)
        return 0;
    double headerSuccessRate = NaN;
    if (part == IRadioSignal::SIGNAL_PART_WHOLE || part == IRadioSignal::SIGNAL_PART_HEADER)
        headerSuccessRate = getHeaderSuccessRate(mode, headerLength.get<b>(), scalarSnir);
    if (part == IRadioSignal::SIGNAL_PART_HEADER)
        return 1.0 - headerSuccessRate;

    if (fecSuccessModel == nullptr)
        throw cRuntimeError("LDPC selected without an IEEE 802.11 FEC success model");
    if (std::isnan(scalarSnir) || scalarSnir < 0)
        throw cRuntimeError("IEEE 802.11 LDPC success model requires a nonnegative SNIR");
    double snrDb = scalarSnir == 0 ? -INFINITY : 10 * std::log10(scalarSnir);
    auto dataEncodingPlan = reconstructIeee80211ReceivedDataEncodingPlan(
            mode->getDataMode(), phyHeader, transmission->getDataDuration());
    double dataSuccessRate = fecSuccessModel->computeDataSuccessRate(
            *mode->getDataMode(), dataEncodingPlan, snrDb);
    switch (part) {
        case IRadioSignal::SIGNAL_PART_WHOLE:
            return 1.0 - headerSuccessRate * dataSuccessRate;
        case IRadioSignal::SIGNAL_PART_DATA:
            return 1.0 - dataSuccessRate;
        default:
            throw cRuntimeError("Unknown signal part: '%s'", IRadioSignal::getSignalPartName(part));
    }
}

double Ieee80211ErrorModelBase::computeBitErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const
{
    Enter_Method("computeBitErrorRate");
    return NaN;
}

double Ieee80211ErrorModelBase::computeSymbolErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const
{
    Enter_Method("computeSymbolErrorRate");
    return NaN;
}

Packet *Ieee80211ErrorModelBase::computeCorruptedPacket(const Packet *packet, double ber) const
{
    if (corruptionMode == CorruptionMode::CM_PACKET)
        return ErrorModelBase::computeCorruptedPacket(packet, ber);
    else
        throw cRuntimeError("Unimplemented corruption mode");
}

double Ieee80211ErrorModelBase::getDsssDbpskSuccessRate(uint32_t bitLength, double snir) const
{
    double EbN0 = snir * spectralEfficiency1bit; // 1 bit per symbol with 1 MSPS
    double bitErrorRate = 0.5 * exp(-EbN0);
    return pow((1.0 - bitErrorRate), (int)bitLength);
}

double Ieee80211ErrorModelBase::getDsssDqpskSuccessRate(uint32_t bitLength, double snir) const
{
    double EbN0 = snir * spectralEfficiency2bit; // 2 bits per symbol, 1 MSPS
    double bitErrorRate = ((sqrt(2.0) + 1.0) / sqrt(8.0 * 3.1415926 * sqrt(2.0))) * (1.0 / sqrt(EbN0)) * exp(-(2.0 - sqrt(2.0)) * EbN0);
    return pow((1.0 - bitErrorRate), (int)bitLength);
}

double Ieee80211ErrorModelBase::getDsssDqpskCck5_5SuccessRate(uint32_t bitLength, double snir) const
{
    double bitErrorRate;
    if (snir > sirPerfect)
        bitErrorRate = 0.0;
    else if (snir < sirImpossible)
        bitErrorRate = 0.5;
    else {
        double a1 = 5.3681634344056195e-001;
        double a2 = 3.3092430025608586e-003;
        double a3 = 4.1654372361004000e-001;
        double a4 = 1.0288981434358866e+000;
        bitErrorRate = a1 * exp(-(pow((snir - a2) / a3, a4)));
    }
    return pow((1.0 - bitErrorRate), (int)bitLength);
}

double Ieee80211ErrorModelBase::getDsssDqpskCck11SuccessRate(uint32_t bitLength, double snir) const
{
    double bitErrorRate;
    if (snir > sirPerfect)
        bitErrorRate = 0.0;
    else if (snir < sirImpossible)
        bitErrorRate = 0.5;
    else {
        double a1 = 7.9056742265333456e-003;
        double a2 = -1.8397449399176360e-001;
        double a3 = 1.0740689468707241e+000;
        double a4 = 1.0523316904502553e+000;
        double a5 = 3.0552298746496687e-001;
        double a6 = 2.2032715128698435e+000;
        bitErrorRate = (a1 * snir * snir + a2 * snir + a3) / (snir * snir * snir + a4 * snir * snir + a5 * snir + a6);
    }
    return pow((1.0 - bitErrorRate), (int)bitLength);
}

} // namespace physicallayer

} // namespace inet
