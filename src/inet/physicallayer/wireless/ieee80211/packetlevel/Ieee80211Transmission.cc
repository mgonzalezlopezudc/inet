//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

namespace inet {

namespace physicallayer {

Ieee80211Transmission::Ieee80211Transmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, const ITransmissionPacketModel *packetModel, const ITransmissionBitModel *bitModel, const ITransmissionSymbolModel *symbolModel, const ITransmissionSampleModel *sampleModel, const ITransmissionAnalogModel *analogModel, const IIeee80211Mode *mode, const Ieee80211Channel *channel, std::shared_ptr<const Ieee80211HeTxVector> heTxVector, std::shared_ptr<const Ieee80211HePpduLayout> hePpduLayout, uint32_t heTriggerCorrelationId, std::shared_ptr<const Ieee80211VhtTxVector> vhtTxVector, std::shared_ptr<const Ieee80211HtTxVector> htTxVector) :
    TransmissionBase(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, packetModel, bitModel, symbolModel, sampleModel, analogModel),
    mode(mode),
    channel(channel),
    heTxVector(std::move(heTxVector)),
    hePpduLayout(std::move(hePpduLayout)),
    heTriggerCorrelationId(heTriggerCorrelationId),
    vhtTxVector(std::move(vhtTxVector)),
    htTxVector(std::move(htTxVector))
{
    const bool isHe = dynamic_cast<const Ieee80211HeMode *>(mode) != nullptr;
    const bool isVht = dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr;
    const bool isHt = dynamic_cast<const Ieee80211HtMode *>(mode) != nullptr;
    if ((this->heTxVector == nullptr) != (this->hePpduLayout == nullptr))
        throw cRuntimeError("HE TXVECTOR and PPDU layout must be present as a pair");
    if (isHe && this->heTxVector == nullptr)
        throw cRuntimeError("HE transmission is missing its canonical TXVECTOR/PPDU-layout handoff");
    if (!isHe && this->heTxVector != nullptr)
        throw cRuntimeError("Non-HE transmission cannot carry an HE TXVECTOR/PPDU layout");
    if (this->hePpduLayout != nullptr && !this->hePpduLayout->matches(*this->heTxVector))
        throw cRuntimeError("HE transmission received a mismatched TXVECTOR/PPDU-layout pair");
    if (this->heTxVector != nullptr &&
            mode->getDataMode()->getBandwidth() !=
                    this->heTxVector->getCommon().getParameters().channelBandwidth)
        throw cRuntimeError("HE carrier mode bandwidth disagrees with the canonical TXVECTOR");
    if (!isVht && this->vhtTxVector != nullptr)
        throw cRuntimeError("Non-VHT transmission cannot carry a VHT TXVECTOR");
    if (isVht && this->vhtTxVector == nullptr)
        throw cRuntimeError("VHT transmission is missing its canonical TXVECTOR handoff");
    if (!isHt && this->htTxVector != nullptr)
        throw cRuntimeError("Non-HT transmission cannot carry an HT TXVECTOR");
    if (this->htTxVector != nullptr &&
            (this->htTxVector->getChannelWidth() != mode->getDataMode()->getBandwidth() ||
             this->htTxVector->getNumberOfSpaceTimeStreams() != mode->getDataMode()->getNumberOfSpatialStreams()))
        throw cRuntimeError("HT TXVECTOR disagrees with the selected HT mode");
    if (this->vhtTxVector != nullptr &&
            this->vhtTxVector->getChannelWidth() != mode->getDataMode()->getBandwidth())
        throw cRuntimeError("VHT TXVECTOR channel width disagrees with the selected mode");
    if (this->vhtTxVector != nullptr) {
        auto header = dynamicPtrCast<const Ieee80211VhtPhyHeader>(
                Ieee80211Radio::peekIeee80211PhyHeaderAtFront(packet));
        if (header == nullptr)
            throw cRuntimeError("Canonical VHT TXVECTOR requires a VHT PHY header");
        auto dataMode = mode->getDataMode();
        if (this->vhtTxVector->getPsduLength() != B(header->getLengthField()) ||
                getIeee80211VhtBandwidthCode(this->vhtTxVector->getChannelWidth()) !=
                        header->getBandwidth() ||
                this->vhtTxVector->isNdp() != header->getNdp() ||
                this->vhtTxVector->getGroupId() != header->getGroupId() ||
                this->vhtTxVector->getPartialAid() != header->getPartialAid() ||
                this->vhtTxVector->getNumberOfSpaceTimeStreams() !=
                        header->getNumberOfSpaceTimeStreams() ||
                (!this->vhtTxVector->isMu() &&
                 this->vhtTxVector->getNumberOfSpaceTimeStreams() !=
                        dataMode->getNumberOfSpatialStreams()) ||
                this->vhtTxVector->getMcs() != header->getMcs() ||
                this->vhtTxVector->isLdpcCoding() != (header->getCoding() != 0) ||
                this->vhtTxVector->hasLdpcExtraOfdmSymbol() !=
                        header->getLdpcExtraOfdmSymbol() ||
                this->vhtTxVector->isBeamformed() != header->getBeamformed())
            throw cRuntimeError("VHT TXVECTOR disagrees with the PHY header or selected mode");
        if (!this->vhtTxVector->isNdp() && !this->vhtTxVector->isMu()) {
            auto vhtMode = check_and_cast<const Ieee80211VhtMode *>(mode);
            auto vhtDataMode = vhtMode->getDataMode();
            bool modeLdpc = vhtDataMode->getCode() != nullptr &&
                    vhtDataMode->getCode()->isLdpc();
            if (this->vhtTxVector->getMcs() != vhtDataMode->getMcsIndex() ||
                    this->vhtTxVector->isLdpcCoding() != modeLdpc ||
                    this->vhtTxVector->hasLdpcExtraOfdmSymbol() !=
                            vhtDataMode->getLdpcExtraOfdmSymbol(
                                    this->vhtTxVector->getPsduLength()))
                throw cRuntimeError("VHT TXVECTOR data parameters disagree with the selected mode");
        }
        if ((this->vhtTxVector->isNdp() && dataDuration != SIMTIME_ZERO) ||
                (!this->vhtTxVector->isNdp() && dataDuration == SIMTIME_ZERO))
            throw cRuntimeError("VHT TXVECTOR PSDU format disagrees with transmission timing");
    }
}

Hz Ieee80211Transmission::getPpduBandwidth() const
{
    return heTxVector != nullptr && hePpduLayout != nullptr &&
            hePpduLayout->matches(*heTxVector) ?
            heTxVector->getCommon().getParameters().channelBandwidth :
            mode->getDataMode()->getBandwidth();
}

std::ostream& Ieee80211Transmission::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Transmission";
    if (level <= PRINT_LEVEL_DETAIL)
        stream << EV_FIELD(mode, printFieldToString(mode, level + 1, evFlags))
               << EV_FIELD(channel, printFieldToString(channel, level + 1, evFlags));
    return TransmissionBase::printToStream(stream, level);
}

} // namespace physicallayer

} // namespace inet
