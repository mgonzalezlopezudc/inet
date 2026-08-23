//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/receiver/Ieee80211HtAlamoutiDecoder.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/MinimumMeanSquareErrorSpatialStreamDetector.h"

#include <algorithm>
#include <cmath>

namespace inet {
namespace physicallayer {

namespace {

constexpr double DECODER_RELATIVE_TOLERANCE = 1e-10;

void validateDescriptorLayout(const SpaceTimeCodeDescriptor& descriptor)
{
    if (descriptor.getNumberOfSpatialStreams() != 1 || descriptor.getNumberOfSourceSymbols() != 2 ||
        descriptor.getNumberOfSpaceTimeStreams() != 2 || descriptor.getNumberOfSlots() != 2)
        throw cRuntimeError("HT Alamouti decoder requires NSS=1, K=2, NSTS=2, and two slots");
}

std::vector<ChannelMatrixObservationCoordinate> makeCoordinates(
    const SpaceTimeCodeDescriptor& descriptor, int numberOfReceiveRows)
{
    if (numberOfReceiveRows <= 0)
        throw cRuntimeError("HT Alamouti decoding requires at least one receive row");
    std::vector<ChannelMatrixObservationCoordinate> result;
    result.reserve(descriptor.getNumberOfSlots() * numberOfReceiveRows);
    for (int slot = 0; slot < descriptor.getNumberOfSlots(); slot++)
        for (int row = 0; row < numberOfReceiveRows; row++)
            result.emplace_back(slot, row, descriptor.getSlot(slot).isConjugateObservation());
    return result;
}

void validateCommonSinr(const ChannelMatrixDetectionResult& detection)
{
    if (detection.getStatus() != ChannelMatrixDetectionStatus::SUCCESS)
        return;
    const auto& sinrs = detection.getSinrs();
    if (sinrs.empty())
        throw cRuntimeError("HT Alamouti successful detection has no SINR");
    for (size_t index = 1; index < sinrs.size(); index++) {
        const double scale = std::max(std::abs(sinrs[0]), std::abs(sinrs[index]));
        if (std::abs(sinrs[0] - sinrs[index]) > DECODER_RELATIVE_TOLERANCE * scale)
            throw cRuntimeError("HT Alamouti per-symbol SINRs are not equal");
    }
}

ChannelMatrixDetectionResult makeRankFailure(const std::vector<ChannelMatrixObservationCoordinate>& coordinates,
    const ComplexMatrix& channel, const ComplexMatrix& covariance, ChannelMatrixDetectionStatus status)
{
    ComplexMatrix zeroWeights(channel.getNumColumns(), channel.getNumRows());
    return ChannelMatrixDetectionResult::fromCanonicalWeights(status, coordinates, channel, covariance, zeroWeights);
}

ChannelMatrixDetectionResult detectPrepared(const SpaceTimeCodeDescriptor& descriptor,
    const ComplexMatrix& effectivePowerScaledStsChannel, const ComplexMatrix& perSlotCovariance,
    ComplexMatrix& canonicalChannel,
    ComplexMatrix& canonicalCovariance, std::vector<ChannelMatrixObservationCoordinate>& coordinates)
{
    validateDescriptorLayout(descriptor);
    ChannelMatrixAlgebra::validateFinite(effectivePowerScaledStsChannel, "HT Alamouti effective STS channel");
    if (effectivePowerScaledStsChannel.getNumColumns() != descriptor.getNumberOfSpaceTimeStreams())
        throw cRuntimeError("HT Alamouti effective STS channel has %d columns, expected %d",
            effectivePowerScaledStsChannel.getNumColumns(), descriptor.getNumberOfSpaceTimeStreams());
    ChannelMatrixAlgebra::validateDimensions(perSlotCovariance, effectivePowerScaledStsChannel.getNumRows(),
        effectivePowerScaledStsChannel.getNumRows(), "HT Alamouti per-slot covariance");
    ChannelMatrixAlgebra::validatePositiveDefinite(perSlotCovariance,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "HT Alamouti per-slot covariance");
    canonicalChannel = descriptor.buildCanonicalAugmentedChannel(effectivePowerScaledStsChannel);
    canonicalCovariance = descriptor.buildCanonicalAugmentedCovariance(perSlotCovariance);
    coordinates = makeCoordinates(descriptor, effectivePowerScaledStsChannel.getNumRows());
    if (ChannelMatrixAlgebra::computeRank(canonicalChannel) < descriptor.getNumberOfSourceSymbols())
        return makeRankFailure(coordinates, canonicalChannel, canonicalCovariance,
            ChannelMatrixDetectionStatus::RANK_DEFICIENT);
    const auto result = MinimumMeanSquareErrorSpatialStreamDetector::compute(
        canonicalChannel, canonicalCovariance, coordinates);
    validateCommonSinr(result);
    return result;
}

} // namespace

Ieee80211HtAlamoutiDecoder::Result::Result(const ChannelMatrixDetectionResult& detectionResult,
    const std::vector<std::complex<double>>& sourceSymbols, bool recovered) :
    detectionResult(detectionResult), sourceSymbols(sourceSymbols), recovered(recovered)
{
    if (recovered) {
        if (detectionResult.getStatus() != ChannelMatrixDetectionStatus::SUCCESS)
            throw cRuntimeError("Recovered HT Alamouti symbols require a successful detection result");
        if (sourceSymbols.size() != 2)
            throw cRuntimeError("Recovered HT Alamouti result requires exactly two source symbols");
        for (const auto& symbol : sourceSymbols)
            if (!std::isfinite(symbol.real()) || !std::isfinite(symbol.imag()))
                throw cRuntimeError("Recovered HT Alamouti source symbols must be finite");
    }
    else {
        if (detectionResult.getStatus() == ChannelMatrixDetectionStatus::SUCCESS)
            throw cRuntimeError("An unsuccessful HT Alamouti result is required when symbols are not recovered");
        if (!sourceSymbols.empty())
            throw cRuntimeError("Unrecovered HT Alamouti results must not contain source symbols");
    }
}

double Ieee80211HtAlamoutiDecoder::Result::getEffectiveSinr() const
{
    const auto& sinrs = detectionResult.getSinrs();
    return sinrs.empty() ? 0 : sinrs.front();
}

ChannelMatrixDetectionResult Ieee80211HtAlamoutiDecoder::detect(
    const SpaceTimeCodeDescriptor& descriptor, const ComplexMatrix& effectivePowerScaledStsChannel,
    const ComplexMatrix& perSlotCovariance)
{
    ComplexMatrix canonicalChannel;
    ComplexMatrix canonicalCovariance;
    std::vector<ChannelMatrixObservationCoordinate> coordinates;
    return detectPrepared(descriptor, effectivePowerScaledStsChannel, perSlotCovariance,
        canonicalChannel, canonicalCovariance, coordinates);
}

Ieee80211HtAlamoutiDecoder::Result Ieee80211HtAlamoutiDecoder::decode(
    const SpaceTimeCodeDescriptor& descriptor, const ComplexMatrix& effectivePowerScaledStsChannel,
    const ComplexMatrix& perSlotCovariance, const ComplexMatrix& slotObservations)
{
    // Validate the observation layout before detection can return a typed
    // rank/condition failure.  A malformed observation is always a caller
    // error, including for a physical zero channel.
    const ComplexMatrix canonicalObservation = descriptor.stackCanonicalObservations(slotObservations);
    ComplexMatrix canonicalChannel;
    ComplexMatrix canonicalCovariance;
    std::vector<ChannelMatrixObservationCoordinate> coordinates;
    const auto detection = detectPrepared(descriptor, effectivePowerScaledStsChannel, perSlotCovariance,
        canonicalChannel, canonicalCovariance, coordinates);
    if (canonicalObservation.getNumRows() != canonicalChannel.getNumRows() ||
        canonicalObservation.getNumColumns() != 1)
        throw cRuntimeError("HT Alamouti observations do not match the effective channel receive dimensions");
    if (detection.getStatus() != ChannelMatrixDetectionStatus::SUCCESS)
        return Result(detection, {}, false);
    const ComplexMatrix weightedObservation = ChannelMatrixAlgebra::multiply(
        detection.getWeights(), canonicalObservation);
    const ComplexMatrix response = ChannelMatrixAlgebra::multiply(detection.getWeights(), canonicalChannel);
    double responseScale = 0;
    for (const auto& coefficient : response.getCoefficients())
        responseScale = std::max(responseScale, std::abs(coefficient));
    ComplexMatrix canonicalEstimates(descriptor.getNumberOfSourceSymbols(), 1);
    for (int stream = 0; stream < descriptor.getNumberOfSourceSymbols(); stream++) {
        const std::complex<double> diagonal = response.get(stream, stream);
        if (!std::isfinite(diagonal.real()) || !std::isfinite(diagonal.imag()) ||
            std::abs(diagonal) <= DECODER_RELATIVE_TOLERANCE * responseScale) {
            const auto failure = ChannelMatrixDetectionResult::fromCanonicalWeights(
                ChannelMatrixDetectionStatus::ILL_CONDITIONED, coordinates, canonicalChannel,
                canonicalCovariance, ComplexMatrix(canonicalChannel.getNumColumns(), canonicalChannel.getNumRows()));
            return Result(failure, {}, false);
        }
        canonicalEstimates.get(stream, 0) = weightedObservation.get(stream, 0) / diagonal;
    }
    const auto sourceSymbols = descriptor.restoreSourceSymbols(canonicalEstimates);
    return Result(detection, sourceSymbols, true);
}

} // namespace physicallayer
} // namespace inet
