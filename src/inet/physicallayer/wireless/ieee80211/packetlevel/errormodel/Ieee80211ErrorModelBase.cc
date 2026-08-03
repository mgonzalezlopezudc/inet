//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.h"

#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"
#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"


namespace inet {

namespace physicallayer {

Ieee80211ErrorModelBase::Ieee80211ErrorModelBase()
{
}

Ieee80211HeMpduErrorRateResult Ieee80211ErrorModelBase::computeHeDataErrorRate(
        const ISnir *snir, size_t userIndex,
        const Ieee80211HeUserPhyParameters& parameters,
        unsigned int bitLength) const noexcept
{
    Ieee80211HeMpduErrorRateResult result;
    try {
        if (snir == nullptr || snir->getReception() == nullptr) {
            result.error = "missing SNIR reception context";
            return result;
        }
        auto transmission = dynamic_cast<const Ieee80211Transmission *>(
                snir->getReception()->getTransmission());
        if (transmission == nullptr || transmission->getHePpduLayout() == nullptr) {
            result.error = "missing canonical HE PPDU layout";
            return result;
        }
        const auto& users = transmission->getHePpduLayout()->getUsers();
        if (userIndex >= users.size()) {
            result.error = "canonical HE user index is out of range";
            return result;
        }
        auto phyHeader = dynamicPtrCast<const Ieee80211HePhyHeader>(
                Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket()));
        if (phyHeader == nullptr) {
            result.error = "missing HE PHY header";
            return result;
        }
        double userSnir = getScalarSnir(snir);
        auto dimensionalSnir = dynamic_cast<const DimensionalSnir *>(snir);
        bool channelMatrixLmmse = dimensionalSnir != nullptr && dimensionalSnir->isChannelMatrixLmmse();
        if (phyHeader->getMuMimo() && phyHeader->getTotalNsts() > 0 && !channelMatrixLmmse) {
            double signalShare = parameters.numberOfSpatialStreams /
                    static_cast<double>(phyHeader->getTotalNsts());
            double interferenceShare = parameters.leakageSum /
                    static_cast<double>(phyHeader->getTotalNsts());
            userSnir = (userSnir * signalShare) /
                    (1.0 + userSnir * interferenceShare);
        }
        if (parameters.dcm)
            userSnir *= 2.0;
        double successRate = getHeDataSuccessRate(parameters, bitLength, snir, userSnir);
        if (!std::isfinite(successRate) || successRate < 0 || successRate > 1) {
            result.error = "HE error model returned an invalid success rate";
            return result;
        }
        result.valid = true;
        result.packetErrorRate = 1.0 - successRate;
        return result;
    }
    catch (const std::exception& exception) {
        result.error = exception.what();
        return result;
    }
    catch (...) {
        result.error = "unknown HE error-model failure";
        return result;
    }
}

Ieee80211HeMpduErrorRateResult Ieee80211ErrorModelBase::computeHeMpduErrorRate(
        const ISnir *snir, size_t userIndex, unsigned int bitLength) const noexcept
{
    Ieee80211HeMpduErrorRateResult result;
    if (bitLength == 0 || bitLength % 8 != 0) {
        result.error = "HE MPDU bit length is zero or not byte aligned";
        return result;
    }
    if (snir == nullptr || snir->getReception() == nullptr) {
        result.error = "missing SNIR reception context";
        return result;
    }
    auto transmission = dynamic_cast<const Ieee80211Transmission *>(
            snir->getReception()->getTransmission());
    if (transmission == nullptr || transmission->getHePpduLayout() == nullptr ||
            userIndex >= transmission->getHePpduLayout()->getUsers().size()) {
        result.error = "missing canonical HE MPDU user";
        return result;
    }
    // The delimiter MPDU Length covers the MAC header, frame body and the
    // four-octet FCS. It excludes the delimiter and A-MPDU padding. Some
    // derived models (notably RBIR) consult psduLength, so make this per-MPDU
    // copy authoritative as well as passing the exact bit length.
    auto parameters = transmission->getHePpduLayout()->getUsers()[userIndex];
    parameters.psduLength = B(bitLength / 8);
    return computeHeDataErrorRate(snir, userIndex, parameters, bitLength);
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
    const auto& vhtTxVector = transmission->getVhtTxVector();
    const Ieee80211VhtMuUser *vhtMuUser = nullptr;
    if (vhtTxVector != nullptr && vhtTxVector->isMu()) {
        auto receiver = snir->getReception()->getReceiverRadio();
        auto radio = dynamic_cast<const IIeee80211VhtPacketRadio *>(receiver);
        if (radio != nullptr) {
            auto selection = radio->getVhtMuRxSelection();
            if (selection.active && selection.groupId == vhtTxVector->getGroupId() &&
                    selection.channelWidth == vhtTxVector->getChannelWidth())
                vhtMuUser = vhtTxVector->findMuUser(selection.userPosition);
        }
        if (vhtMuUser != nullptr) {
            auto acModeSet = Ieee80211ModeSet::getModeSet("ac");
            mode = acModeSet->findVhtMode(vhtMuUser->mcs,
                    vhtMuUser->numberOfSpatialStreams,
                    vhtTxVector->getChannelWidth(), vhtMuUser->ldpcCoding);
            if (mode == nullptr)
                throw cRuntimeError("Canonical VHT MU user has no matching VHT mode");
        }
    }
    auto headerLength = mode->getHeaderMode()->getLength();
    B selectedPsduLength = vhtMuUser == nullptr ? B(phyHeader->getLengthField()) :
            vhtMuUser->psduLength;
    unsigned int dataLength = mode->getDataMode()->getCompleteLength(selectedPsduLength).get<b>();
    // TODO check header length and data length for OFDM (signal) field
    double snr = getScalarSnir(snir);
    // The receiver consumes the immutable PHY handoff, never the sender's
    // mutable VHT request tag (AR-WLAN-PHY-AUTHORITY).
    if (vhtMuUser != nullptr)
        snr *= std::pow(10.0,
                (vhtMuUser->beamformingGainDb - vhtMuUser->leakagePenaltyDb) / 10.0);
    else if (vhtTxVector != nullptr && vhtTxVector->isBeamformed() && !vhtTxVector->isMu())
        snr *= std::pow(10.0, vhtTxVector->getBeamformingGainDb() / 10.0);

    double headerSnr = snr;
    // IEEE 802.11-2024, 27.3.6.6: HE ER-SU repeats the HE-SIG-A field (two copies of each OFDM
    // symbol). The receiver can combine the repeated copies, giving approximately 3 dB of additional
    // robustness for the common signaling field. The data field is constructed identically to HE-SU,
    // so this gain applies only to the header/preamble part of the packet-error calculation.
    if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode)) {
        if (heMode->getPreambleMode()->getPreambleFormat() == Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU)
            headerSnr *= std::pow(10.0, 3.0 / 10.0);
    }
    double headerSuccessRate = getHeaderSuccessRate(mode, headerLength.get<b>(), headerSnr);
    double dataSuccessRate;
    auto hePhyHeader = dynamicPtrCast<const Ieee80211HePhyHeader>(phyHeader);
    auto allocationPhyHeader = hePhyHeader != nullptr &&
            (dynamicPtrCast<const Ieee80211HeMuPhyHeader>(hePhyHeader) != nullptr ||
             dynamicPtrCast<const Ieee80211HeTbPhyHeader>(hePhyHeader) != nullptr) ? hePhyHeader : nullptr;
    if (vhtTxVector != nullptr && vhtTxVector->isMu() && vhtMuUser == nullptr) {
        // A nonmember, stale member, or wrong-position STA has no PSDU in
        // this PPDU (IEEE Std 802.11-2024 21.3.11.4).
        dataSuccessRate = 0;
    }
    else if (vhtTxVector != nullptr && vhtTxVector->isNdp()) {
        // IEEE Std 802.11-2024 Figure 21-28: VHT NDP has no DATA field.
        // Its whole-PPDU result is therefore exactly the preamble/header result.
        dataSuccessRate = 1;
    }
    else if (allocationPhyHeader != nullptr) {
        const auto& layout = transmission->getHePpduLayout();
        if (layout != nullptr && layout->isNdp()) {
            // An NDP has no DATA field; the common HE signaling decision is
            // the complete packet outcome.
            dataSuccessRate = 1;
        }
        else {
            std::optional<size_t> selectedUserIndex;
            bool ambiguousUser = false;
            if (dynamicPtrCast<const Ieee80211HeTbPhyHeader>(phyHeader) != nullptr) {
                // One STA emits each HE-TB PPDU. Shared-RU UL MU-MIMO layouts also
                // retain zero-PSDU peer users to describe the common spatial
                // geometry, so select the sole active PSDU instead of relying on
                // header user count or the receiving AP's STA-ID.
                if (layout != nullptr)
                    for (const auto& range : layout->getPsduBitRanges()) {
                        if (range.getBitLength() == b(0))
                            continue;
                        if (selectedUserIndex.has_value()) {
                            ambiguousUser = true;
                            break;
                        }
                        selectedUserIndex = range.getUserIndex();
                    }
            }
            else {
                const Ieee80211HeMuUserInfo *selectedUser = nullptr;
                auto receiver = snir->getReception()->getReceiverRadio();
                auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
                auto staId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
                if (staId.has_value())
                    for (unsigned int i = 0; i < allocationPhyHeader->getUsersArraySize(); ++i)
                        if (allocationPhyHeader->getUsers(i).staId == *staId) {
                            selectedUser = &allocationPhyHeader->getUsers(i);
                            break;
                        }
                if (selectedUser != nullptr && layout != nullptr) {
                    const auto& canonicalUsers = layout->getUsers();
                    for (size_t i = 0; i < canonicalUsers.size(); ++i)
                        if (canonicalUsers[i].staId == selectedUser->staId &&
                                canonicalUsers[i].ru.toneSize == selectedUser->ruToneSize &&
                                canonicalUsers[i].ru.toneOffset == selectedUser->ruToneOffset) {
                            if (selectedUserIndex.has_value()) {
                                ambiguousUser = true;
                                break;
                            }
                            selectedUserIndex = i;
                        }
                }
            }
            if (!selectedUserIndex.has_value() || ambiguousUser || layout == nullptr ||
                    *selectedUserIndex >= layout->getUsers().size())
                dataSuccessRate = 0;
            else {
                const auto& parameters = layout->getUsers()[*selectedUserIndex];
                dataLength = parameters.serviceBits +
                        parameters.psduLength.get<B>() * 8 + parameters.tailBits;
                auto mpduError = computeHeDataErrorRate(snir, *selectedUserIndex,
                        parameters, dataLength);
                dataSuccessRate = mpduError ? 1.0 - mpduError.packetErrorRate : 0;
            }
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
