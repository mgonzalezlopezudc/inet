//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"


namespace inet {

namespace physicallayer {

Ieee80211ErrorModelBase::Ieee80211ErrorModelBase()
{
}

double Ieee80211ErrorModelBase::getDataSuccessRate(const IIeee80211Mode *mode,
        unsigned int bitLength, const ISnir *snir, double scalarSnir) const
{
    return getDataSuccessRate(mode, bitLength, scalarSnir);
}

double Ieee80211ErrorModelBase::getHeDataSuccessRate(
        const Ieee80211HeUserPhyParameters& parameters,
        unsigned int bitLength, double snir) const
{
    throw cRuntimeError("Per-user HE error evaluation is unsupported by this error model");
}

double Ieee80211ErrorModelBase::getHeDataSuccessRate(
        const Ieee80211HeUserPhyParameters& parameters,
        unsigned int bitLength, const ISnir *snir, double scalarSnir) const
{
    return getHeDataSuccessRate(parameters, bitLength, scalarSnir);
}

double Ieee80211ErrorModelBase::computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const
{
    Enter_Method("computePacketErrorRate");
    auto transmission = check_and_cast<const Ieee80211Transmission *>(snir->getReception()->getTransmission());
    auto mode = transmission->getMode();
    auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
    auto headerLength = mode->getHeaderMode()->getLength();
    unsigned int dataLength = mode->getDataMode()->getCompleteLength(B(phyHeader->getLengthField())).get<b>();
    // TODO check header length and data length for OFDM (signal) field
    double snr = getScalarSnir(snir);
    auto vhtTag = transmission->getPacket() ? transmission->getPacket()->findTag<Ieee80211VhtTransmissionTag>() : nullptr;
    if (vhtTag) {
        if (vhtTag->getBeamformed())
            snr *= std::pow(10.0, vhtTag->getBeamformingGainDb() / 10.0);
        if (vhtTag->getMuMimo())
            snr /= std::pow(10.0, vhtTag->getMuMimoPenaltyDb() / 10.0);
    }

    double headerSuccessRate = getHeaderSuccessRate(mode, headerLength.get<b>(), snr);
    double dataSuccessRate;
    if (auto heMuHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(phyHeader)) {
        const Ieee80211HeMuUserInfo *selectedUser = nullptr;
        if (heMuHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
                heMuHeader->getUsersArraySize() == 1)
            selectedUser = &heMuHeader->getUsers(0);
        else {
            auto receiver = snir->getReception()->getReceiverRadio();
            auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
            auto staId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
            if (staId.has_value())
                for (unsigned int i = 0; i < heMuHeader->getUsersArraySize(); ++i)
                    if (heMuHeader->getUsers(i).staId == *staId) {
                        selectedUser = &heMuHeader->getUsers(i);
                        break;
                    }
        }
        if (selectedUser == nullptr)
            dataSuccessRate = 0;
        else {
            Ieee80211HeRu ru;
            ru.index = selectedUser->ruIndex;
            ru.toneSize = std::max<int>(selectedUser->ruToneSize, 26);
            ru.toneOffset = selectedUser->ruToneOffset;
            ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * 78125.0);
            auto parameters = computeHeUserPhyParameters(selectedUser->psduLength, ru,
                    selectedUser->mcs, selectedUser->numberOfSpatialStreams,
                    selectedUser->dcm,
                    static_cast<Ieee80211HeGuardInterval>(heMuHeader->getGuardInterval()),
                    static_cast<Ieee80211HeCoding>(heMuHeader->getCoding()));
            dataLength = 16 + selectedUser->psduLength.get<B>() * 8 + 6;
            double userSnir = snr;
            if (heMuHeader->getMuMimo() && heMuHeader->getTotalNsts() > 0) {
                double desiredNsts = selectedUser->numberOfSpatialStreams;
                double totalNsts = heMuHeader->getTotalNsts();
                double signalShare = desiredNsts / totalNsts;
                double interferenceShare = selectedUser->leakageSum / totalNsts;
                userSnir = (snr * signalShare) / (1.0 + snr * interferenceShare);
            }
            if (selectedUser->dcm)
                userSnir *= 2.0;
            dataSuccessRate = getHeDataSuccessRate(parameters, dataLength, snir, userSnir);
        }
    }
    else
        dataSuccessRate = getDataSuccessRate(mode, dataLength, snir, snr);
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
