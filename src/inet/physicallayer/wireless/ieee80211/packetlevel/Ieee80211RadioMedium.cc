//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.h"

#include <algorithm>

// IEEE 802.11ax RU-aware radio medium.
//
// Extends the generic radio medium to compute per-RU receptions and
// interference for HE MU and HE TB PPDUs.  It resolves the receiver's assigned
// RU from the HE MU PHY header, creates a narrowband reception analog model
// centered on that RU, and limits interference to frequency-overlapping signals.
// Relevant clauses:
//   - Clause 27.3.2: HE subcarriers and RUs.
//   - Clause 27.3.4: HE MU and HE TB PPDU formats.
//   - Clause 27.3.11.8: HE-SIG-B RU allocation and user identification.
//
// Approximations / simplifications:
//   - Per-RU receive power is approximated as total transmit power scaled by
//     RU bandwidth over channel bandwidth.  The standard specifies per-
//     subcarrier power and does not scale total TX power linearly with RU size.
//   - Single-user HE TB transmissions intentionally skip bandwidth scaling and
//     use the full aggregate power, which is inconsistent with multi-RU UL.
//   - Perfect RU isolation is assumed; adjacent-RU leakage and in-band
//     emissions are not modeled.
//   - Multiple simultaneous HE TB users are treated as independent receptions.
//     Co-triggered MU-MIMO users with disjoint spatial-stream ranges are assumed
//     to be perfectly spatially separated; multi-user synchronization, channel
//     estimation, CFO, and timing misalignment impairments are not modeled.

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/signal/Interference.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211RadioMedium);

static const Ieee80211HeMuPhyHeader *peekHeTbMuMimoHeader(const ITransmission *transmission)
{
    auto packet = transmission == nullptr ? nullptr : transmission->getPacket();
    if (packet == nullptr || transmission->getPacketProtocol() != &Protocol::ieee80211HePhy ||
            !packet->hasAtFront<Ieee80211HeMuPhyHeader>())
        return nullptr;
    auto header = packet->peekAtFront<Ieee80211HeMuPhyHeader>();
    return header->getPpduFormat() == HE_TRIGGER_BASED_UPLINK && header->getMuMimo() &&
            header->getUsersArraySize() == 1 ? header.get() : nullptr;
}

static bool areSpatiallyOrthogonalHeTbUsers(const ITransmission *desired, const ITransmission *other)
{
    auto desiredHeader = peekHeTbMuMimoHeader(desired);
    auto otherHeader = peekHeTbMuMimoHeader(other);
    if (desiredHeader == nullptr || otherHeader == nullptr ||
            desiredHeader->getTriggerId() != otherHeader->getTriggerId())
        return false;

    const auto& desiredUser = desiredHeader->getUsers(0);
    const auto& otherUser = otherHeader->getUsers(0);
    if (desiredUser.ruToneSize != otherUser.ruToneSize ||
            desiredUser.ruToneOffset != otherUser.ruToneOffset)
        return false;

    int desiredFirst = desiredUser.streamStartIndex;
    int desiredLast = desiredFirst + desiredUser.numberOfSpatialStreams;
    int otherFirst = otherUser.streamStartIndex;
    int otherLast = otherFirst + otherUser.numberOfSpatialStreams;
    return desiredLast <= otherFirst || otherLast <= desiredFirst;
}

void Ieee80211RadioMedium::addTransmission(const IRadio *transmitterRadio, const ITransmission *transmission)
{
    RadioMedium::addTransmission(transmitterRadio, transmission);
}

bool Ieee80211RadioMedium::findHeMuRuForReceiver(const IRadio *receiver, const ITransmission *transmission, Ieee80211HeRu& ru) const
{
    auto packet = transmission->getPacket();
    if (transmission->getPacketProtocol() != &Protocol::ieee80211HePhy || packet == nullptr || !packet->hasAtFront<Ieee80211HeMuPhyHeader>())
        return false;
    auto heMuPhyHeader = packet->peekAtFront<Ieee80211HeMuPhyHeader>();
    auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
    auto receiverStaId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
    if (!receiverStaId.has_value() && heMuPhyHeader->getPpduFormat() != HE_TRIGGER_BASED_UPLINK)
        return false;
    auto scalarTransmissionAnalogModel = dynamic_cast<const ScalarSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (scalarTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return false;
    if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
            heMuPhyHeader->getUsersArraySize() == 1) {
        // HE TB transmitters are centered on their assigned RU by the
        // transmitter model. Reconstruct the full-channel center from the RU
        // offset, then return the user RU for reception/interference filtering.
        const auto& user = heMuPhyHeader->getUsers(0);
        constexpr double HE_TONE_SPACING = 78125;
        auto channelBandwidth = ieee80211Transmission->getChannel()->getBand()->getSpacing();
        int channelTones = getHeChannelToneCount(channelBandwidth);
        ru.index = user.ruIndex;
        ru.toneSize = user.ruToneSize;
        ru.toneOffset = user.ruToneOffset;
        ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
        ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
        ru.bandwidth = Hz(ru.toneSize * HE_TONE_SPACING);
        double centerTone = ru.toneOffset + ru.toneSize / 2.0 - channelTones / 2.0;
        auto fullChannelCenter = scalarTransmissionAnalogModel->getCenterFrequency() -
                Hz(centerTone * HE_TONE_SPACING);
        ru.centerFrequency = fullChannelCenter + Hz(centerTone * HE_TONE_SPACING);
        return true;
    }
    for (unsigned int i = 0; i < heMuPhyHeader->getUsersArraySize(); ++i) {
        const auto& user = heMuPhyHeader->getUsers(i);
        if (receiverStaId.has_value() && user.staId == *receiverStaId) {
            // DL HE MU receiver filtering follows the STA-ID and RU location
            // carried in the HE-SIG-B User field (Clause 27.3.11.8.4).
            constexpr double HE_TONE_SPACING = 78125;
            auto channelBandwidth = ieee80211Transmission->getChannel()->getBand()->getSpacing();
            if (user.ruToneSize == 0) {
                auto legacyRus = calculateHeRus(scalarTransmissionAnalogModel->getCenterFrequency(),
                        channelBandwidth, heMuPhyHeader->getUsersArraySize());
                if (user.ruIndex >= 0 && user.ruIndex < (int)legacyRus.size()) {
                    ru = legacyRus[user.ruIndex];
                    return true;
                }
                return false;
            }
            int channelTones = getHeChannelToneCount(channelBandwidth);
            ru.index = user.ruIndex;
            ru.toneSize = user.ruToneSize;
            ru.toneOffset = user.ruToneOffset;
            ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * HE_TONE_SPACING);
            double centerTone = ru.toneOffset + ru.toneSize / 2.0 - channelTones / 2.0;
            ru.centerFrequency = scalarTransmissionAnalogModel->getCenterFrequency() + Hz(centerTone * HE_TONE_SPACING);
            return true;
        }
    }
    return false;
}

const IReception *Ieee80211RadioMedium::computeHeMuRuReception(const IRadio *receiver, const ITransmission *transmission, const Ieee80211HeRu& ru) const
{
    auto scalarMediumAnalogModel = dynamic_cast<const ScalarMediumAnalogModel *>(analogModel);
    auto scalarTransmissionAnalogModel = dynamic_cast<const ScalarSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (scalarMediumAnalogModel == nullptr || scalarTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return nullptr;

    auto arrival = getArrival(receiver, transmission);
    auto totalBandwidth = ieee80211Transmission->getChannel()->getBand()->getSpacing();
    auto aggregatePower = scalarMediumAnalogModel->computeReceptionPower(receiver, transmission, arrival);
    auto packet = transmission->getPacket();
    auto heMuHeader = packet != nullptr && packet->hasAtFront<Ieee80211HeMuPhyHeader>() ?
            packet->peekAtFront<Ieee80211HeMuPhyHeader>() : nullptr;
    bool isTriggerBasedUplink = heMuHeader != nullptr &&
            heMuHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK;
    // The standard defines the RU's subcarrier allocation, not this packet-level
    // power scaling. For DL MU, scale the aggregate receive power by occupied
    // RU bandwidth; for UL TB, the transmitter already emits on the assigned RU.
    auto ruPower = isTriggerBasedUplink ? aggregatePower :
            aggregatePower * (ru.bandwidth.get() / totalBandwidth.get());
    auto ruAnalogModel = new ScalarReceptionAnalogModel(
            scalarTransmissionAnalogModel->getPreambleDuration(),
            scalarTransmissionAnalogModel->getHeaderDuration(),
            scalarTransmissionAnalogModel->getDataDuration(),
            ru.centerFrequency,
            ru.bandwidth,
            ruPower);
    return new Reception(receiver, transmission, arrival->getStartTime(), arrival->getEndTime(),
            arrival->getStartPosition(), arrival->getEndPosition(),
            arrival->getStartOrientation(), arrival->getEndOrientation(), ruAnalogModel);
}

const IReception *Ieee80211RadioMedium::computeReception(const IRadio *receiver, const ITransmission *transmission) const
{
    Ieee80211HeRu ru;
    if (findHeMuRuForReceiver(receiver, transmission, ru)) {
        if (auto reception = computeHeMuRuReception(receiver, transmission, ru))
            return reception;
    }
    return RadioMedium::computeReception(receiver, transmission);
}

const IInterference *Ieee80211RadioMedium::computeInterference(const IRadio *receiver,
        const IListening *listening, const ITransmission *transmission) const
{
    Ieee80211HeRu desiredRu;
    if (!findHeMuRuForReceiver(receiver, transmission, desiredRu))
        return RadioMedium::computeInterference(receiver, listening, transmission);

    // Clause 27.3.2 makes HE RUs orthogonal in frequency. The model therefore
    // admits only overlapping analog receptions as interference for this RU.
    interferenceComputationCount++;
    auto reception = getReception(receiver, transmission);
    auto allInterferingReceptions = computeInterferingReceptions(reception);
    auto overlappingReceptions = new std::vector<const IReception *>();
    double desiredMin = desiredRu.centerFrequency.get() - desiredRu.bandwidth.get() / 2;
    double desiredMax = desiredRu.centerFrequency.get() + desiredRu.bandwidth.get() / 2;
    for (auto interferingReception : *allInterferingReceptions) {
        // Clause 27.3.3.2.4 assigns non-overlapping spatial streams to users in
        // one UL MU-MIMO exchange. The scalar analog model has no channel
        // matrix with which to separate those streams, so model ideal spatial
        // separation here. Same-RU transmissions from another Trigger, or with
        // overlapping stream ranges, remain ordinary interference.
        if (areSpatiallyOrthogonalHeTbUsers(transmission, interferingReception->getTransmission()))
            continue;
        auto analogModel = dynamic_cast<const ScalarReceptionAnalogModel *>(
                interferingReception->getAnalogModel());
        if (analogModel == nullptr) {
            overlappingReceptions->push_back(interferingReception);
            continue;
        }
        double interferingMin = analogModel->getCenterFrequency().get() -
                analogModel->getBandwidth().get() / 2;
        double interferingMax = analogModel->getCenterFrequency().get() +
                analogModel->getBandwidth().get() / 2;
        if (std::min(desiredMax, interferingMax) > std::max(desiredMin, interferingMin))
            overlappingReceptions->push_back(interferingReception);
    }
    delete allInterferingReceptions;
    auto noise = backgroundNoise ? backgroundNoise->computeNoise(listening) : nullptr;
    return new Interference(noise, overlappingReceptions);
}

} // namespace physicallayer
} // namespace inet
