//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgnChannelModel.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>

#include <omnetpp/cmersennetwister.h>
#include <omnetpp/distrib.h>

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/INarrowbandSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

namespace inet {
namespace physicallayer {

Define_Module(TgnChannelModel);

namespace {

constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double COMPLEX_NORMAL_STANDARD_DEVIATION = 0.707106781186547524400844362104849039;

void verifyMersenneTwisterSeedAdapter()
{
    static const bool verified = []() {
        MTRand::uint32 words[2] = {0x01234567UL, 0x89abcdefUL};
        MTRand test(words, 2);
        static const uint32_t expected[] = {
            UINT32_C(1753520470), UINT32_C(1541709090), UINT32_C(1130731796), UINT32_C(3230678913),
            UINT32_C(4056221769), UINT32_C(3478207629), UINT32_C(4029113932), UINT32_C(4187665763)
        };
        for (uint32_t value : expected)
            if ((uint32_t)test.randInt() != value)
                throw cRuntimeError("TGn two-word MTRand seed-adapter self-test failed");
        MTRand uniformGenerator(words, 2);
        if (std::abs(uniformGenerator.randExc() - 0.40827329969033599) > 1e-15)
            throw cRuntimeError("TGn MTRand uniform golden-vector self-test failed");
        MTRand normalGenerator(words, 2);
        const double u = 1.0 - normalGenerator.randExc();
        const double v = 1.0 - normalGenerator.randExc();
        const double normal = std::sqrt(-2.0 * std::log(u)) * std::cos(2 * PI * v);
        if (std::abs(normal - (-0.64779894410282612)) > 1e-15)
            throw cRuntimeError("TGn MTRand normal golden-vector self-test failed");
        return true;
    }();
    (void)verified;
}

// Use the kernel's cRNG implementation and expose only deterministic
// constructor seeding for private, purpose-derived channel streams.
class TgnDerivedMersenneTwister : public cMersenneTwister
{
  public:
    explicit TgnDerivedMersenneTwister(uint64_t seed)
    {
        verifyMersenneTwisterSeedAdapter();
        MTRand::uint32 words[2] = {
            MTRand::uint32(seed >> 32),
            MTRand::uint32(seed & UINT64_C(0xffffffff))
        };
        rng.seed(words, 2);
    }

    virtual void initialize(int, int, int, int, int, cConfiguration *) override
    {
        throw cRuntimeError("TgnDerivedMersenneTwister is constructor-seeded and cannot be initialized by the environment");
    }
};

void appendUint64(std::vector<uint8_t>& bytes, uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
        bytes.push_back((uint8_t)(value >> shift));
}

void appendString(std::vector<uint8_t>& bytes, const std::string& value)
{
    appendUint64(bytes, value.size());
    bytes.insert(bytes.end(), value.begin(), value.end());
}

std::complex<double> drawProperComplexNormal(cRNG *rng)
{
    return {omnetpp::normal(rng, 0, COMPLEX_NORMAL_STANDARD_DEVIATION),
            omnetpp::normal(rng, 0, COMPLEX_NORMAL_STANDARD_DEVIATION)};
}

TgnLorentzianProcess createLorentzianProcess(double centerFrequencyHz, double halfWidthHz,
    int oscillatorCount, cRNG *rng)
{
    if (!std::isfinite(centerFrequencyHz) || !std::isfinite(halfWidthHz) || halfWidthHz < 0 || oscillatorCount <= 0)
        throw cRuntimeError("Invalid TGn Lorentzian oscillator-bank parameters");
    const int count = halfWidthHz == 0 ? 1 : oscillatorCount;
    std::vector<double> frequencies;
    std::vector<std::complex<double>> coefficients;
    frequencies.reserve(count);
    coefficients.reserve(count);
    for (int oscillator = 0; oscillator < count; oscillator++) {
        const double quantile = (oscillator + 0.5) / count;
        frequencies.push_back(centerFrequencyHz + halfWidthHz * std::tan(PI * (quantile - 0.5)));
        coefficients.push_back(drawProperComplexNormal(rng));
    }
    return TgnLorentzianProcess(frequencies, coefficients);
}

} // namespace

void TgnChannelModel::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        profile = TgnChannelProfile::create(TgnChannelProfile::parseModel(par("profile")));
        condition = TgnChannelProfile::parseCondition(par("condition"));
        reciprocal = par("reciprocal");
        timeVariation = par("timeVariation");
        vehicleEffect = par("vehicleEffect");
        fluorescentEffect = par("fluorescentEffect");
        shadowing = par("shadowing");
        ensembleNormalization = par("ensembleNormalization");
        fluorescentMainsFrequencyHz = Hz(par("fluorescentMainsFrequency")).get();
        environmentalSpeedMps = mps(par("environmentalSpeed")).get();
        vehicleSpeedMps = mps(par("vehicleSpeed")).get();
        antennaSpacingInWavelengths = par("antennaSpacing");
        oscillatorCount = par("oscillatorCount");
        if (!std::isfinite(environmentalSpeedMps) || environmentalSpeedMps < 0 ||
            !std::isfinite(vehicleSpeedMps) || vehicleSpeedMps < 0)
            throw cRuntimeError("TGn environmental and vehicle speeds must be finite and nonnegative");
        if (!std::isfinite(antennaSpacingInWavelengths) || antennaSpacingInWavelengths <= 0 || oscillatorCount <= 0)
            throw cRuntimeError("TGn antenna spacing and oscillator count must be positive");
        if (fluorescentEffect && (profile.getModel() == TgnModel::D || profile.getModel() == TgnModel::E) &&
            (!std::isfinite(fluorescentMainsFrequencyHz) || fluorescentMainsFrequencyHz <= 0))
            throw cRuntimeError("TGn fluorescent mains frequency must be finite and positive for Models D and E");
        if (strcmp(getParentModule()->par("rangeFilter"), "") != 0)
            throw cRuntimeError("TGn time-varying fading and shadowing require radio-medium rangeFilter=\"\"");

        const LinkKey goldenKey{"tx", "rx"};
        if (derivePurposeSeed(UINT64_C(0x0123456789abcdef), goldenKey, TgnModel::D, TgnCondition::NLOS,
                7, 1, 2, "diffuseBell") != UINT64_C(15695006415662829421))
            throw cRuntimeError("TGn INET-TGN-SEED-V1 derivation golden-vector self-test failed");

        // Extract exactly two 32-bit words from each configured master stream in fixed order.
        linkMasterSeed = extractMasterSeed(getRNG(par("linkSeedRng")));
        shadowingMasterSeed = extractMasterSeed(getRNG(par("shadowingRng")));
        diffuseMasterSeed = extractMasterSeed(getRNG(par("diffuseRng")));
        fluorescentMasterSeed = extractMasterSeed(getRNG(par("fluorescentRng")));
    }
}

std::string TgnChannelModel::stableRadioId(const IRadio *radio)
{
    const cModule *radioModule = check_and_cast<const cModule *>(radio);
    return radioModule->getFullPath();
}

TgnChannelModel::LinkKey TgnChannelModel::makeLinkKey(const IRadio *transmitter, const IRadio *receiver, bool& transpose) const
{
    const std::string transmitterId = stableRadioId(transmitter);
    const std::string receiverId = stableRadioId(receiver);
    transpose = reciprocal && receiverId < transmitterId;
    return transpose ? LinkKey{receiverId, transmitterId} : LinkKey{transmitterId, receiverId};
}

uint64_t TgnChannelModel::splitMix64Finalizer(uint64_t value)
{
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

uint64_t TgnChannelModel::derivePurposeSeed(uint64_t familySeed, const LinkKey& key, TgnModel model,
    TgnCondition condition, int component, int matrixRow, int matrixColumn, const char *temporalEffect)
{
    std::vector<uint8_t> bytes;
    const std::string prefix = "INET-TGN-SEED-V1";
    bytes.insert(bytes.end(), prefix.begin(), prefix.end());
    appendUint64(bytes, familySeed);
    appendString(bytes, key.transmitterId);
    appendString(bytes, key.receiverId);
    appendUint64(bytes, (uint64_t)model);
    appendUint64(bytes, (uint64_t)condition);
    appendUint64(bytes, component < 0 ? UINT64_MAX : (uint64_t)component);
    appendUint64(bytes, matrixRow < 0 ? UINT64_MAX : (uint64_t)matrixRow);
    appendUint64(bytes, matrixColumn < 0 ? UINT64_MAX : (uint64_t)matrixColumn);
    if (temporalEffect == nullptr)
        appendUint64(bytes, UINT64_MAX);
    else
        appendString(bytes, temporalEffect);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t byte : bytes) {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    return splitMix64Finalizer(hash ^ familySeed);
}

uint64_t TgnChannelModel::extractMasterSeed(cRNG *rng)
{
    const uint64_t highWord = rng->intRand();
    const uint64_t lowWord = rng->intRand();
    return (highWord << 32) | lowWord;
}

void TgnChannelModel::validateMode(const ITransmission *transmission) const
{
    const auto ieeeTransmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (ieeeTransmission == nullptr)
        throw cRuntimeError("TgnChannelModel supports only IEEE 802.11 transmissions");
    if (dynamic_cast<const DimensionalSignalAnalogModel *>(transmission->getAnalogModel()) == nullptr ||
        dynamic_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel()) == nullptr)
        throw cRuntimeError("TgnChannelModel requires a dimensional narrowband transmission analog representation");
    const IIeee80211Mode *mode = ieeeTransmission->getMode();
    const int numberOfSpatialStreams = mode->getDataMode()->getNumberOfSpatialStreams();
    if (numberOfSpatialStreams != 1)
        throw cRuntimeError("TgnChannelModel supports exactly one spatial stream, mode %s has %d", mode->getName(), numberOfSpatialStreams);
    if (const auto htMode = dynamic_cast<const Ieee80211HtMode *>(mode)) {
        const Hz bandwidth = htMode->getDataMode()->getBandwidth();
        if (bandwidth != MHz(20) && bandwidth != MHz(40))
            throw cRuntimeError("TgnChannelModel supports only HT20/HT40, mode %s uses %s", mode->getName(), bandwidth.str().c_str());
        const unsigned int stbc = htMode->getHeaderMode()->getSTBC();
        if (stbc != 0)
            throw cRuntimeError("TgnChannelModel does not support HT STBC, mode %s has STBC=%u", mode->getName(), stbc);
        if (numberOfSpatialStreams + (int)stbc != 1)
            throw cRuntimeError("TgnChannelModel supports exactly one space-time stream, mode %s has %d", mode->getName(), numberOfSpatialStreams + (int)stbc);
    }
    else if (dynamic_cast<const Ieee80211DsssMode *>(mode) == nullptr &&
             dynamic_cast<const Ieee80211HrDsssMode *>(mode) == nullptr &&
             dynamic_cast<const Ieee80211OfdmMode *>(mode) == nullptr)
        throw cRuntimeError("TgnChannelModel does not support IEEE 802.11 PPDU mode %s", mode->getName());
}

std::shared_ptr<const TgnChannelRealization> TgnChannelModel::createLinkState(const LinkKey& key,
    const IRadio *canonicalTransmitter, const IRadio *canonicalReceiver, Hz referenceFrequency,
    mps propagationSpeed) const
{
    const int numTransmitAntennas = canonicalTransmitter->getAntenna()->getNumAntennas();
    const int numReceiveAntennas = canonicalReceiver->getAntenna()->getNumAntennas();
    if (numTransmitAntennas < 1 || numTransmitAntennas > 8 || numReceiveAntennas < 1 || numReceiveAntennas > 8)
        throw cRuntimeError("TGn link %s -> %s has unsupported %d x %d antenna dimensions (allowed: 1 through 8)",
            key.transmitterId.c_str(), key.receiverId.c_str(), numReceiveAntennas, numTransmitAntennas);
    if (!std::isfinite(referenceFrequency.get()) || referenceFrequency <= Hz(0) ||
        !std::isfinite(propagationSpeed.get()) || propagationSpeed <= mps(0))
        throw cRuntimeError("TGn link creation requires a finite positive carrier frequency and propagation speed");

    const uint64_t linkSalt = splitMix64Finalizer(linkMasterSeed);
    const uint64_t shadowingFamilySeed = splitMix64Finalizer(linkSalt ^ shadowingMasterSeed);
    const uint64_t diffuseFamilySeed = splitMix64Finalizer(linkSalt ^ diffuseMasterSeed);
    const uint64_t fluorescentFamilySeed = splitMix64Finalizer(linkSalt ^ fluorescentMasterSeed);
    std::set<uint64_t> usedPurposeSeeds;
    auto makeRng = [&](uint64_t familySeed, int component, int row, int column, const char *effect) {
        uint64_t seed = derivePurposeSeed(familySeed, key, profile.getModel(), condition, component, row, column, effect);
        if (!usedPurposeSeeds.insert(seed).second)
            throw cRuntimeError("TGn purpose-seed collision on link %s -> %s", key.transmitterId.c_str(), key.receiverId.c_str());
        return std::make_unique<TgnDerivedMersenneTwister>(seed);
    };

    auto realization = std::make_shared<TgnChannelRealization>();
    realization->numReceiveAntennas = numReceiveAntennas;
    realization->numTransmitAntennas = numTransmitAntennas;
    realization->referenceFrequency = referenceFrequency;
    realization->timeVariation = timeVariation;
    realization->los = condition == TgnCondition::LOS;
    realization->firstTapKLinear = profile.getFirstTapKLinear();
    realization->firstTapDiffusePower = profile.getFirstTapComponent().normalizedLinearPower;
    {
        if (!shadowing) {
            realization->shadowingPowerGain = 1;
        }
        else {
            auto rng = makeRng(shadowingFamilySeed, -1, -1, -1, "shadowing");
            const double sigmaDb = realization->los ? profile.getShadowSigmaLosDb() : profile.getShadowSigmaNlosDb();
            const double shadowDb = omnetpp::normal(rng.get(), 0, sigmaDb);
            realization->shadowingPowerGain = std::pow(10.0, -shadowDb / 10.0);
        }
    }

    // The TGn LOS construction adds sqrt(K*p0) to the diffuse first tap.
    // Normalize the expected complete small-scale profile once, using the
    // deterministic profile factor 1/sqrt(1+K*p0). Individual realizations
    // retain their natural fading variance.
    realization->smallScalePowerNormalization = ensembleNormalization && realization->los ?
        1.0 / std::sqrt(1.0 + realization->firstTapKLinear * realization->firstTapDiffusePower) : 1.0;

    std::map<int, std::pair<ComplexMatrix, ComplexMatrix>> squareRoots;
    for (const auto& cluster : profile.getClusters()) {
        ComplexMatrix receiverCorrelation = TgnMimoChannel::createSpatialCorrelation(numReceiveAntennas,
            antennaSpacingInWavelengths, cluster.meanAoADegrees, cluster.receiverAngularSpreadDegrees);
        ComplexMatrix transmitterCorrelation = TgnMimoChannel::createSpatialCorrelation(numTransmitAntennas,
            antennaSpacingInWavelengths, cluster.meanAoDDegrees, cluster.transmitterAngularSpreadDegrees);
        squareRoots.emplace(cluster.reportClusterIndex, std::make_pair(
            TgnMimoChannel::principalSquareRoot(receiverCorrelation),
            TgnMimoChannel::principalSquareRoot(transmitterCorrelation)));
    }

    const double wavelength = propagationSpeed.get() / referenceFrequency.get();
    const double baseHalfWidth = environmentalSpeedMps / wavelength / 3.0;
    for (const auto& component : profile.getComponents()) {
        TgnComponentRealization componentRealization;
        componentRealization.stableComponentIndex = component.stableComponentIndex;
        componentRealization.excessDelay = profile.getTap(component.reportTapIndex).excessDelay;
        componentRealization.normalizedLinearPower = component.normalizedLinearPower;
        componentRealization.receiverSquareRoot = squareRoots.at(component.reportClusterIndex).first;
        componentRealization.transmitterSquareRoot = squareRoots.at(component.reportClusterIndex).second;
        componentRealization.fluorescent = fluorescentEffect && profile.hasFluorescentEffect(component);
        componentRealization.temporalProcesses.reserve(numReceiveAntennas * numTransmitAntennas);
        for (int row = 0; row < numReceiveAntennas; row++) {
            for (int column = 0; column < numTransmitAntennas; column++) {
                TgnTemporalProcess temporalProcess;
                if (vehicleEffect && profile.hasVehicleEffect(component)) {
                    const double spikeFrequency = vehicleSpeedMps / wavelength;
                    const double spikeHalfWidth = spikeFrequency / 300.0;
                    const double bellPower = PI * baseHalfWidth;
                    const double spikePower = PI * 0.5 * spikeHalfWidth;
                    const double totalPower = bellPower + spikePower;
                    if (totalPower == 0) {
                        auto rng = makeRng(diffuseFamilySeed, component.stableComponentIndex, row, column, "vehicleFixed");
                        temporalProcess.terms.push_back({1, createLorentzianProcess(0, 0, 1, rng.get())});
                    }
                    else {
                        if (bellPower > 0) {
                            auto rng = makeRng(diffuseFamilySeed, component.stableComponentIndex, row, column, "vehicleBell");
                            temporalProcess.terms.push_back({std::sqrt(bellPower / totalPower),
                                createLorentzianProcess(0, baseHalfWidth, oscillatorCount, rng.get())});
                        }
                        if (spikePower > 0) {
                            auto rng = makeRng(diffuseFamilySeed, component.stableComponentIndex, row, column, "vehicleSpike");
                            temporalProcess.terms.push_back({std::sqrt(spikePower / totalPower),
                                createLorentzianProcess(spikeFrequency, spikeHalfWidth, oscillatorCount, rng.get())});
                        }
                    }
                }
                else {
                    auto rng = makeRng(diffuseFamilySeed, component.stableComponentIndex, row, column,
                        baseHalfWidth == 0 ? "diffuseFixed" : "diffuseBell");
                    temporalProcess.terms.push_back({1, createLorentzianProcess(0, baseHalfWidth, oscillatorCount, rng.get())});
                }
                componentRealization.temporalProcesses.push_back(std::move(temporalProcess));
            }
        }
        realization->components.push_back(std::move(componentRealization));
    }

    if (realization->los)
        realization->fixedLosMatrix = TgnMimoChannel::createFixedLosMatrix(numReceiveAntennas,
            numTransmitAntennas, antennaSpacingInWavelengths, antennaSpacingInWavelengths);
    realization->fluorescent = fluorescentEffect && (profile.getModel() == TgnModel::D || profile.getModel() == TgnModel::E);
    if (realization->fluorescent) {
        realization->fluorescentMainsFrequencyHz = fluorescentMainsFrequencyHz;
        auto icRng = makeRng(fluorescentFamilySeed, -1, -1, -1, "fluorescentIc");
        const double icNormal = omnetpp::normal(icRng.get(), 0.0203, 0.0107);
        const double targetIc = icNormal * icNormal;
        double selectedPower = 0;
        for (const auto& component : profile.getComponents())
            if (profile.hasFluorescentEffect(component))
                selectedPower += component.normalizedLinearPower;
        const double carrierPower = 1 + (realization->los ?
            realization->firstTapKLinear * realization->firstTapDiffusePower : 0);
        const double meanFluorescentPower = 1 + std::pow(10.0, -15.0 / 10.0) + std::pow(10.0, -20.0 / 10.0);
        // INET policy: one link-level modulation function is shared by all
        // affected components, and LOS energy participates in the requested
        // interference-to-complete-carrier ratio.
        realization->fluorescentScale = std::sqrt(targetIc * carrierPower / (selectedPower * meanFluorescentPower));
        for (int harmonic = 0; harmonic < 3; harmonic++) {
            std::ostringstream effect;
            effect << "fluorescentPhase" << harmonic;
            auto phaseRng = makeRng(fluorescentFamilySeed, -1, -1, harmonic, effect.str().c_str());
            realization->fluorescentPhases[harmonic] = omnetpp::uniform(phaseRng.get(), 0, 2 * PI);
        }
    }
    return realization;
}

void TgnChannelModel::addRadio(const IRadio *radio)
{
    const int antennaCount = radio->getAntenna()->getNumAntennas();
    if (antennaCount < 1 || antennaCount > 8)
        throw cRuntimeError("TGn radio %s has %d antennas; supported counts are 1 through 8", stableRadioId(radio).c_str(), antennaCount);
    registeredRadioIds.insert(stableRadioId(radio));
}

void TgnChannelModel::removeRadio(const IRadio *radio)
{
    const std::string radioId = stableRadioId(radio);
    registeredRadioIds.erase(radioId);
    for (auto it = linkStates.begin(); it != linkStates.end(); ) {
        if (it->first.transmitterId == radioId || it->first.receiverId == radioId)
            it = linkStates.erase(it);
        else
            ++it;
    }
}

std::shared_ptr<const IChannelMatrixSnapshot> TgnChannelModel::computeChannel(const IRadio *receiver,
    const ITransmission *transmission, const IArrival *arrival) const
{
    validateMode(transmission);
    const IRadio *transmitter = transmission->getTransmitterRadio();
    if (transmitter == nullptr)
        throw cRuntimeError("TGn transmission has no transmitter radio");
    if (!registeredRadioIds.count(stableRadioId(transmitter)) || !registeredRadioIds.count(stableRadioId(receiver)))
        throw cRuntimeError("TGn channel queried for an unregistered radio pair");
    const auto narrowband = check_and_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
    const Hz referenceFrequency = narrowband->getCenterFrequency();
    const m distance = m(transmission->getStartPosition().distance(arrival->getStartPosition()));
    if (condition == TgnCondition::LOS && distance.get() > profile.getBreakpointDistanceMeters())
        throw cRuntimeError("TGn profile %s LOS link distance %s exceeds breakpoint %g m",
            TgnChannelProfile::getModelName(profile.getModel()), distance.str().c_str(), profile.getBreakpointDistanceMeters());

    bool transpose;
    const LinkKey key = makeLinkKey(transmitter, receiver, transpose);
    const IRadio *canonicalTransmitter = transpose ? receiver : transmitter;
    const IRadio *canonicalReceiver = transpose ? transmitter : receiver;
    auto it = linkStates.find(key);
    if (it == linkStates.end()) {
        const mps propagationSpeed = transmission->getMedium()->getPropagation()->getPropagationSpeed();
        it = linkStates.emplace(key, createLinkState(key, canonicalTransmitter, canonicalReceiver,
            referenceFrequency, propagationSpeed)).first;
    }
    const auto& realization = it->second;
    if (realization->referenceFrequency != referenceFrequency)
        throw cRuntimeError("TGn runtime carrier-frequency switching is unsupported on link %s -> %s: cached %s, requested %s",
            key.transmitterId.c_str(), key.receiverId.c_str(), realization->referenceFrequency.str().c_str(), referenceFrequency.str().c_str());
    auto response = std::make_shared<const ChannelMatrixResponse>(realization->numReceiveAntennas,
        realization->numTransmitAntennas, [realization](simtime_t time, Hz frequency) {
            return TgnMimoChannel::evaluate(*realization, time, frequency);
        });
    std::shared_ptr<const IChannelMatrixSnapshot> snapshot = std::make_shared<const ChannelMatrixSnapshot>(
        realization->numReceiveAntennas, realization->numTransmitAntennas, referenceFrequency,
        arrival->getStartTime(), arrival->getEndTime(), realization->shadowingPowerGain,
        TgnMimoChannel::getActualMaximumTemporalFrequency(*realization), profile.getMaximumExcessDelay(), response);
    return transpose ? snapshot->transpose() : snapshot;
}

std::ostream& TgnChannelModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "TgnChannelModel, profile = " << TgnChannelProfile::getModelName(profile.getModel())
           << ", condition = " << (condition == TgnCondition::LOS ? "los" : "nlos")
           << ", cached links = " << linkStates.size();
    return stream;
}

} // namespace physicallayer
} // namespace inet
