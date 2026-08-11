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
//   - With the scalar analog representation, per-RU receive power is
//     approximated as total transmit power scaled by RU bandwidth over channel
//     bandwidth. The dimensional representation retains the power spectral
//     density and limits it to the selected RU band instead.
//   - Single-user HE TB transmissions intentionally skip bandwidth scaling and
//     use the full aggregate power, which is inconsistent with multi-RU UL.
//   - Perfect RU isolation is assumed; adjacent-RU leakage and in-band
//     emissions are not modeled.
//   - Multiple simultaneous HE TB users are treated as independent receptions.
//     Co-triggered MU-MIMO users with disjoint spatial-stream ranges are assumed
//     to be perfectly separated unless covariance-aware channel-matrix L-MMSE
//     is active; multi-user synchronization, channel estimation, CFO, and
//     timing misalignment impairments are not modeled.

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyHeader.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/signal/Interference.h"
#include "inet/physicallayer/wireless/common/signal/PowerFunctions.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211RadioMedium);

static Ptr<const Ieee80211HePhyHeader> peekHeMuOrTbPhyHeader(const ITransmission *transmission)
{
    auto packet = transmission == nullptr ? nullptr : transmission->getPacket();
    if (packet == nullptr || transmission->getPacketProtocol() != &Protocol::ieee80211HePhy ||
            !packet->hasAtFront<Ieee80211HePhyHeader>())
        return nullptr;
    auto hePhyHeader = packet->peekAtFront<Ieee80211HePhyHeader>();
    return dynamicPtrCast<const Ieee80211HeMuPhyHeader>(hePhyHeader) != nullptr ||
            dynamicPtrCast<const Ieee80211HeTbPhyHeader>(hePhyHeader) != nullptr ? hePhyHeader : nullptr;
}

bool Ieee80211RadioMedium::areSpatiallyOrthogonalHeTbUsers(const ITransmission *desired, const ITransmission *other)
{
    auto desiredTransmission = dynamic_cast<const Ieee80211Transmission *>(desired);
    auto otherTransmission = dynamic_cast<const Ieee80211Transmission *>(other);
    auto desiredLayout = desiredTransmission == nullptr ? nullptr : desiredTransmission->getHePpduLayout();
    auto otherLayout = otherTransmission == nullptr ? nullptr : otherTransmission->getHePpduLayout();
    if (desiredLayout == nullptr || otherLayout == nullptr ||
            desiredLayout->getPpduFormat() != HE_TRIGGER_BASED_UPLINK ||
            otherLayout->getPpduFormat() != HE_TRIGGER_BASED_UPLINK ||
            desiredLayout->getUsers().empty() || otherLayout->getUsers().empty() ||
            desiredTransmission->getHeTriggerCorrelationId() == 0 ||
            desiredTransmission->getHeTriggerCorrelationId() != otherTransmission->getHeTriggerCorrelationId())
        return false;

    // One STA emits each HE-TB packet. The immutable layout puts that local
    // user first; following zero-PSDU users only retain the common shared-RU
    // spatial geometry.
    const auto& desiredUser = desiredLayout->getUsers().front();
    const auto& otherUser = otherLayout->getUsers().front();
    // 27.3.18 maps simultaneous NFRP feedback NDPs onto orthogonal tone-set
    // and (when multiplexing is enabled) starting-STS resources.
    if (desiredUser.ndpFeedbackReport && otherUser.ndpFeedbackReport)
        return desiredUser.ndpRuToneSetIndex != otherUser.ndpRuToneSetIndex ||
                desiredUser.ndpStartingStsNumber != otherUser.ndpStartingStsNumber;
    if (desiredUser.ru.toneSize != otherUser.ru.toneSize ||
            desiredUser.ru.toneOffset != otherUser.ru.toneOffset)
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
    auto allocationPhyHeader = peekHeMuOrTbPhyHeader(transmission);
    if (allocationPhyHeader == nullptr)
        return false;
    auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
    auto receiverStaId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
    bool isTriggerBasedUplink = dynamicPtrCast<const Ieee80211HeTbPhyHeader>(allocationPhyHeader) != nullptr;
    if (!receiverStaId.has_value() && !isTriggerBasedUplink)
        return false;
    auto narrowbandTransmissionAnalogModel = dynamic_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (narrowbandTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return false;
    if (isTriggerBasedUplink && ieee80211Transmission->getHePpduLayout() != nullptr &&
            !ieee80211Transmission->getHePpduLayout()->getUsers().empty()) {
        // HE TB transmitters are centered on their assigned RU by the
        // transmitter model. Reconstruct the full-channel center from the RU
        // offset, then return the transmitting (first/local) user's RU for
        // reception/interference filtering. Additional zero-PSDU entries in
        // UL MU-MIMO retain peer stream geometry and are not transmitters in
        // this packet.
        const auto& user = ieee80211Transmission->getHePpduLayout()->getUsers().front();
        auto channelBandwidth = ieee80211Transmission->getPpduBandwidth();
        int channelTones = getHeChannelToneCount(channelBandwidth);
        auto relativeRu = makeHeRu(Hz(0), channelTones,
                user.ru.index, user.ru.toneSize, user.ru.toneOffset);
        auto fullChannelCenter = narrowbandTransmissionAnalogModel->getCenterFrequency() -
                relativeRu.centerFrequency;
        ru = makeHeRu(fullChannelCenter, channelTones,
                user.ru.index, user.ru.toneSize, user.ru.toneOffset);
        return true;
    }
    for (unsigned int i = 0; i < allocationPhyHeader->getUsersArraySize(); ++i) {
        const auto& user = allocationPhyHeader->getUsers(i);
        if (receiverStaId.has_value() && user.staId == *receiverStaId) {
            // DL HE MU receiver filtering follows the STA-ID and RU location
            // carried in the HE-SIG-B User field (Clause 27.3.11.8.4).
            auto channelBandwidth = ieee80211Transmission->getPpduBandwidth();
            if (user.ruToneSize == 0) {
                auto legacyRus = calculateHeRus(narrowbandTransmissionAnalogModel->getCenterFrequency(),
                        channelBandwidth, allocationPhyHeader->getUsersArraySize());
                if (user.ruIndex >= 0 && user.ruIndex < (int)legacyRus.size()) {
                    ru = legacyRus[user.ruIndex];
                    return true;
                }
                return false;
            }
            int channelTones = getHeChannelToneCount(channelBandwidth);
            ru = makeHeRu(narrowbandTransmissionAnalogModel->getCenterFrequency(), channelTones,
                    user.ruIndex, user.ruToneSize, user.ruToneOffset);
            return true;
        }
    }
    return false;
}

const IReception *Ieee80211RadioMedium::computeHeMuRuReception(const IRadio *receiver, const ITransmission *transmission, const Ieee80211HeRu& ru) const
{
    auto transmissionAnalogModel = dynamic_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (transmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return nullptr;

    auto arrival = getArrival(receiver, transmission);
    IReceptionAnalogModel *ruAnalogModel = nullptr;
    if (auto scalarMediumAnalogModel = dynamic_cast<const ScalarMediumAnalogModel *>(analogModel)) {
        auto totalBandwidth = ieee80211Transmission->getPpduBandwidth();
        auto aggregatePower = scalarMediumAnalogModel->computeReceptionPower(receiver, transmission, arrival);
        auto allocationPhyHeader = peekHeMuOrTbPhyHeader(transmission);
        bool isTriggerBasedUplink = dynamicPtrCast<const Ieee80211HeTbPhyHeader>(allocationPhyHeader) != nullptr;
        // The standard defines the RU's subcarrier allocation, not this packet-level
        // power scaling. For DL MU, scale the aggregate receive power by occupied
        // RU bandwidth; for UL TB, the transmitter already emits on the assigned RU.
        auto ruPower = isTriggerBasedUplink ? aggregatePower :
                aggregatePower * (ru.bandwidth.get() / totalBandwidth.get());
        ruAnalogModel = new ScalarReceptionAnalogModel(
                transmissionAnalogModel->getPreambleDuration(),
                transmissionAnalogModel->getHeaderDuration(),
                transmissionAnalogModel->getDataDuration(),
                ru.centerFrequency,
                ru.bandwidth,
                ruPower);
    }
    else if (auto dimensionalMediumAnalogModel = dynamic_cast<const DimensionalMediumAnalogModel *>(analogModel)) {
        if (dynamic_cast<const DimensionalSignalAnalogModel *>(transmission->getAnalogModel()) == nullptr)
            return nullptr;
        bool channelMatrixCombined = false;
        Ptr<const IFunction<WpHz, Domain<simsec, Hz>>> aggregateInterferencePower;
        std::shared_ptr<const ChannelMatrixSignal> aggregateChannelMatrixSignal;
        auto aggregatePower = dimensionalMediumAnalogModel->computeReceptionPower(
                receiver, transmission, arrival, &channelMatrixCombined, &aggregateInterferencePower,
                &aggregateChannelMatrixSignal);
        const auto& ruBandpassFilter = makeShared<Boxcar2DFunction<double, simsec, Hz>>(
                simsec(arrival->getStartTime()), simsec(arrival->getEndTime()),
                ru.centerFrequency - ru.bandwidth / 2, ru.centerFrequency + ru.bandwidth / 2, 1);
        ruAnalogModel = new DimensionalReceptionAnalogModel(
                transmissionAnalogModel->getPreambleDuration(),
                transmissionAnalogModel->getHeaderDuration(),
                transmissionAnalogModel->getDataDuration(),
                ru.centerFrequency,
                ru.bandwidth,
                aggregatePower->multiply(ruBandpassFilter),
                aggregateInterferencePower->multiply(ruBandpassFilter),
                channelMatrixCombined,
                aggregateChannelMatrixSignal == nullptr ? nullptr : aggregateChannelMatrixSignal->withInputPower(
                        aggregateChannelMatrixSignal->getInputPower()->multiply(ruBandpassFilter)));
    }
    if (ruAnalogModel == nullptr)
        return nullptr;
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
    auto dimensionalMediumAnalogModel = dynamic_cast<const DimensionalMediumAnalogModel *>(analogModel);
    auto desiredDimensionalAnalogModel = dynamic_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    bool useSpatialCovariance = dimensionalMediumAnalogModel != nullptr &&
            dimensionalMediumAnalogModel->isChannelMatrixLmmseEnabled() &&
            desiredDimensionalAnalogModel != nullptr &&
            desiredDimensionalAnalogModel->getChannelMatrixSignal() != nullptr;
    auto allInterferingReceptions = computeInterferingReceptions(reception);
    auto overlappingReceptions = new std::vector<const IReception *>();
    double desiredMin = desiredRu.centerFrequency.get() - desiredRu.bandwidth.get() / 2;
    double desiredMax = desiredRu.centerFrequency.get() + desiredRu.bandwidth.get() / 2;
    for (auto interferingReception : *allInterferingReceptions) {
        // Clause 27.3.3.2.4 assigns non-overlapping spatial streams to users in
        // one UL MU-MIMO exchange. The scalar analog model has no channel
        // matrix with which to separate those streams, so model ideal spatial
        // separation here unless L-MMSE has the desired channel vector and
        // can retain the other reception for covariance processing. Same-RU
        // transmissions from another Trigger, or with overlapping stream
        // ranges, remain ordinary interference in every mode.
        if (!useSpatialCovariance &&
                areSpatiallyOrthogonalHeTbUsers(transmission, interferingReception->getTransmission()))
            continue;
        auto analogModel = dynamic_cast<const INarrowbandSignalAnalogModel *>(
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

const INoise *Ieee80211RadioMedium::getNoise(const IRadio *receiver, const ITransmission *transmission) const
{
    Ieee80211HeRu desiredRu;
    if (!findHeMuRuForReceiver(receiver, transmission, desiredRu))
        return RadioMedium::getNoise(receiver, transmission);

    cacheNoiseGetCount++;
    auto noise = communicationCache->getCachedNoise(receiver, transmission);
    if (noise != nullptr) {
        cacheNoiseHitCount++;
        return noise;
    }

    auto listening = communicationCache->getCachedListening(receiver, transmission);
    BandListening ruListening(receiver, listening->getStartTime(), listening->getEndTime(),
            listening->getStartPosition(), listening->getEndPosition(),
            desiredRu.centerFrequency, desiredRu.bandwidth);
    // Background-noise modules are generally configured for the receiver's
    // aggregate channel and may require that listening bandwidth. Keep that
    // listening for interference construction, then project the resulting
    // signals onto the recipient RU during scalar/dimensional noise
    // integration below.
    auto interference = computeInterference(receiver, listening, transmission);
    noise = analogModel->computeNoise(&ruListening, interference);
    delete interference;
    communicationCache->setCachedNoise(receiver, transmission, noise);
    return noise;
}

} // namespace physicallayer
} // namespace inet
