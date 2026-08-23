//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"

#include <algorithm>
#include <cmath>

namespace inet {
namespace physicallayer {

namespace {

constexpr double DESCRIPTOR_RELATIVE_TOLERANCE = 1e-10;

double maximumCoefficientMagnitude(const ComplexMatrix& matrix)
{
    double result = 0;
    for (const auto& coefficient : matrix.getCoefficients())
        result = std::max(result, std::abs(coefficient));
    return result;
}

void validateFiniteSymbols(const std::vector<std::complex<double>>& symbols)
{
    for (const auto& symbol : symbols)
        if (!std::isfinite(symbol.real()) || !std::isfinite(symbol.imag()))
            throw cRuntimeError("Space-time source symbols must be finite");
}

void validateFiniteMatrix(const ComplexMatrix& matrix, const char *operation)
{
    ChannelMatrixAlgebra::validateFinite(matrix, operation);
}

double trace(const ComplexMatrix& matrix)
{
    const int size = std::min(matrix.getNumRows(), matrix.getNumColumns());
    double result = 0;
    for (int index = 0; index < size; index++) {
        const auto diagonal = matrix.get(index, index);
        if (std::abs(diagonal.imag()) > DESCRIPTOR_RELATIVE_TOLERANCE * std::max(1.0, std::abs(diagonal)))
            throw cRuntimeError("Space-time code covariance has a non-real diagonal");
        result += diagonal.real();
    }
    return result;
}

bool closeTo(double value, double expected)
{
    const double scale = std::max(std::abs(value), std::abs(expected));
    return std::abs(value - expected) <= DESCRIPTOR_RELATIVE_TOLERANCE * scale;
}

} // namespace

SpaceTimeCodeDescriptor::Slot::Slot(const ComplexMatrix& directCoefficients,
    const ComplexMatrix& conjugateCoefficients, bool conjugateObservation) :
    directCoefficients(directCoefficients), conjugateCoefficients(conjugateCoefficients),
    conjugateObservation(conjugateObservation)
{
    validateFiniteMatrix(directCoefficients, "Space-time direct coefficients");
    validateFiniteMatrix(conjugateCoefficients, "Space-time conjugate coefficients");
    if (directCoefficients.getNumRows() != conjugateCoefficients.getNumRows() ||
        directCoefficients.getNumColumns() != conjugateCoefficients.getNumColumns())
        throw cRuntimeError("Space-time direct and conjugate coefficients must have equal dimensions");
}

SpaceTimeCodeDescriptor::SpaceTimeCodeDescriptor(int numberOfSpatialStreams, int numberOfSourceSymbols,
    int numberOfSpaceTimeStreams, double amplitudeScale, const std::vector<Slot>& slots,
    const std::vector<bool>& decodedSymbolConjugated) :
    numberOfSpatialStreams(numberOfSpatialStreams), numberOfSourceSymbols(numberOfSourceSymbols),
    numberOfSpaceTimeStreams(numberOfSpaceTimeStreams), amplitudeScale(amplitudeScale), slots(slots),
    decodedSymbolConjugated(decodedSymbolConjugated)
{
    if (numberOfSpatialStreams <= 0 || numberOfSourceSymbols <= 0 || numberOfSpaceTimeStreams <= 0)
        throw cRuntimeError("Space-time code dimensions must be positive");
    if (!std::isfinite(amplitudeScale) || amplitudeScale <= 0)
        throw cRuntimeError("Space-time code amplitude scale must be finite and positive");
    if (this->slots.empty())
        throw cRuntimeError("Space-time code requires at least one slot");
    if ((int)decodedSymbolConjugated.size() != numberOfSourceSymbols)
        throw cRuntimeError("Space-time decoded-symbol conjugation count must equal source-symbol count");
    for (const auto& slot : this->slots) {
        const ComplexMatrix& direct = slot.getDirectCoefficients();
        const ComplexMatrix& conjugate = slot.getConjugateCoefficients();
        if (direct.getNumRows() != numberOfSpaceTimeStreams || direct.getNumColumns() != numberOfSourceSymbols ||
            conjugate.getNumRows() != numberOfSpaceTimeStreams || conjugate.getNumColumns() != numberOfSourceSymbols)
            throw cRuntimeError("Space-time slot coefficient dimensions do not match descriptor dimensions");
    }
    validateCanonicalLinearity();
    for (int slotIndex = 0; slotIndex < (int)this->slots.size(); slotIndex++) {
        const double slotEnergy = trace(computeSlotSpaceTimeStreamCovariance(slotIndex));
        if (!closeTo(slotEnergy, 1.0))
            throw cRuntimeError("Space-time slot covariance must have unit trace, got %g", slotEnergy);
    }
    const double jointEnergy = trace(computeJointAugmentedSpaceTimeStreamCovariance());
    if (!closeTo(jointEnergy, numberOfSourceSymbols))
        throw cRuntimeError("Space-time joint covariance must have trace equal to source-symbol count, got %g", jointEnergy);
}

const SpaceTimeCodeDescriptor::Slot& SpaceTimeCodeDescriptor::getSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= (int)slots.size())
        throw cRuntimeError("Space-time slot index %d is outside [0,%d)", slotIndex, (int)slots.size());
    return slots[slotIndex];
}

ComplexMatrix SpaceTimeCodeDescriptor::buildCanonicalCoefficientMatrix(int slotIndex) const
{
    const Slot& slot = getSlot(slotIndex);
    const ComplexMatrix& direct = slot.getDirectCoefficients();
    const ComplexMatrix& conjugate = slot.getConjugateCoefficients();
    ComplexMatrix transformedDirect = direct;
    ComplexMatrix transformedConjugate = conjugate;
    if (slot.isConjugateObservation()) {
        transformedDirect = ComplexMatrix(direct.getNumRows(), direct.getNumColumns());
        transformedConjugate = ComplexMatrix(conjugate.getNumRows(), conjugate.getNumColumns());
        for (int row = 0; row < direct.getNumRows(); row++)
            for (int column = 0; column < direct.getNumColumns(); column++) {
                transformedDirect.get(row, column) = std::conj(conjugate.get(row, column));
                transformedConjugate.get(row, column) = std::conj(direct.get(row, column));
            }
    }
    ComplexMatrix canonical(numberOfSpaceTimeStreams, numberOfSourceSymbols);
    for (int row = 0; row < numberOfSpaceTimeStreams; row++)
        for (int source = 0; source < numberOfSourceSymbols; source++) {
            canonical.get(row, source) = amplitudeScale *
                (decodedSymbolConjugated[source] ? transformedConjugate.get(row, source) : transformedDirect.get(row, source));
        }
    return canonical;
}

void SpaceTimeCodeDescriptor::validateCanonicalLinearity() const
{
    for (const auto& slot : slots) {
        const ComplexMatrix& direct = slot.getDirectCoefficients();
        const ComplexMatrix& conjugate = slot.getConjugateCoefficients();
        ComplexMatrix transformedDirect = direct;
        ComplexMatrix transformedConjugate = conjugate;
        if (slot.isConjugateObservation()) {
            transformedDirect = ComplexMatrix(direct.getNumRows(), direct.getNumColumns());
            transformedConjugate = ComplexMatrix(conjugate.getNumRows(), conjugate.getNumColumns());
            for (int row = 0; row < direct.getNumRows(); row++)
                for (int column = 0; column < direct.getNumColumns(); column++) {
                    transformedDirect.get(row, column) = std::conj(conjugate.get(row, column));
                    transformedConjugate.get(row, column) = std::conj(direct.get(row, column));
                }
        }
        const double scale = std::max(maximumCoefficientMagnitude(transformedDirect),
            maximumCoefficientMagnitude(transformedConjugate));
        const double tolerance = DESCRIPTOR_RELATIVE_TOLERANCE * scale;
        for (int row = 0; row < numberOfSpaceTimeStreams; row++)
            for (int source = 0; source < numberOfSourceSymbols; source++) {
                const auto unsupported = decodedSymbolConjugated[source] ?
                    transformedDirect.get(row, source) : transformedConjugate.get(row, source);
                if (std::abs(unsupported) > tolerance)
                    throw cRuntimeError("Space-time descriptor is not linear in its canonical source variables");
            }
    }
    ComplexMatrix stacked(slots.size() * numberOfSpaceTimeStreams, numberOfSourceSymbols);
    for (int slotIndex = 0; slotIndex < (int)slots.size(); slotIndex++) {
        const ComplexMatrix canonical = buildCanonicalCoefficientMatrix(slotIndex);
        for (int row = 0; row < numberOfSpaceTimeStreams; row++)
            for (int source = 0; source < numberOfSourceSymbols; source++)
                stacked.get(slotIndex * numberOfSpaceTimeStreams + row, source) = canonical.get(row, source);
    }
    if (ChannelMatrixAlgebra::computeRank(stacked, DESCRIPTOR_RELATIVE_TOLERANCE) < numberOfSourceSymbols)
        throw cRuntimeError("Space-time descriptor canonical transform is rank deficient");
}

std::vector<ComplexMatrix> SpaceTimeCodeDescriptor::encodeSourceBlock(
    const std::vector<std::complex<double>>& symbols) const
{
    if ((int)symbols.size() != numberOfSourceSymbols)
        throw cRuntimeError("Space-time source-symbol count %zu does not match descriptor count %d",
            symbols.size(), numberOfSourceSymbols);
    validateFiniteSymbols(symbols);
    std::vector<ComplexMatrix> result;
    result.reserve(slots.size());
    for (const auto& slot : slots) {
        ComplexMatrix encoded(numberOfSpaceTimeStreams, 1);
        for (int row = 0; row < numberOfSpaceTimeStreams; row++) {
            std::complex<double> value = 0;
            for (int source = 0; source < numberOfSourceSymbols; source++)
                value += slot.getDirectCoefficients().get(row, source) * symbols[source] +
                    slot.getConjugateCoefficients().get(row, source) * std::conj(symbols[source]);
            encoded.get(row, 0) = amplitudeScale * value;
        }
        result.push_back(encoded);
    }
    return result;
}

ComplexMatrix SpaceTimeCodeDescriptor::buildCanonicalAugmentedChannel(const ComplexMatrix& effectiveStsChannel) const
{
    return buildCanonicalAugmentedChannel(
        std::vector<ComplexMatrix>(slots.size(), effectiveStsChannel));
}

ComplexMatrix SpaceTimeCodeDescriptor::buildCanonicalAugmentedChannel(
    const std::vector<ComplexMatrix>& effectiveStsChannels) const
{
    if (effectiveStsChannels.size() != slots.size())
        throw cRuntimeError("Space-time effective channel count %zu does not match slot count %zu",
            effectiveStsChannels.size(), slots.size());
    const int numberOfReceiveRows = effectiveStsChannels.front().getNumRows();
    for (const auto& effectiveStsChannel : effectiveStsChannels) {
        validateFiniteMatrix(effectiveStsChannel, "Space-time effective STS channel");
        if (effectiveStsChannel.getNumColumns() != numberOfSpaceTimeStreams)
            throw cRuntimeError("Space-time effective channel has %d columns, expected %d",
                effectiveStsChannel.getNumColumns(), numberOfSpaceTimeStreams);
        if (effectiveStsChannel.getNumRows() != numberOfReceiveRows)
            throw cRuntimeError("Space-time effective channels must have equal receive-row counts");
    }
    ComplexMatrix result(slots.size() * numberOfReceiveRows, numberOfSourceSymbols);
    for (int slotIndex = 0; slotIndex < (int)slots.size(); slotIndex++) {
        const auto& effectiveStsChannel = effectiveStsChannels[slotIndex];
        ComplexMatrix channel = effectiveStsChannel;
        if (slots[slotIndex].isConjugateObservation()) {
            channel = ComplexMatrix(effectiveStsChannel.getNumRows(), effectiveStsChannel.getNumColumns());
            for (int row = 0; row < effectiveStsChannel.getNumRows(); row++)
                for (int column = 0; column < effectiveStsChannel.getNumColumns(); column++)
                    channel.get(row, column) = std::conj(effectiveStsChannel.get(row, column));
        }
        const ComplexMatrix transformed = ChannelMatrixAlgebra::multiply(channel,
            buildCanonicalCoefficientMatrix(slotIndex));
        for (int row = 0; row < numberOfReceiveRows; row++)
            for (int source = 0; source < numberOfSourceSymbols; source++)
                result.get(slotIndex * numberOfReceiveRows + row, source) = transformed.get(row, source);
    }
    return result;
}

ComplexMatrix SpaceTimeCodeDescriptor::buildCanonicalAugmentedCovariance(const ComplexMatrix& perSlotCovariance) const
{
    return buildCanonicalAugmentedCovariance(
        std::vector<ComplexMatrix>(slots.size(), perSlotCovariance));
}

ComplexMatrix SpaceTimeCodeDescriptor::buildCanonicalAugmentedCovariance(
    const std::vector<ComplexMatrix>& perSlotCovariances) const
{
    if (perSlotCovariances.size() != slots.size())
        throw cRuntimeError("Space-time covariance count %zu does not match slot count %zu",
            perSlotCovariances.size(), slots.size());
    const int numberOfReceiveRows = perSlotCovariances.front().getNumRows();
    for (const auto& covariance : perSlotCovariances) {
        ChannelMatrixAlgebra::validateDimensions(covariance, numberOfReceiveRows,
            numberOfReceiveRows, "Space-time per-slot covariance");
        ChannelMatrixAlgebra::validatePositiveDefinite(covariance,
            ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "Space-time per-slot covariance");
    }
    ComplexMatrix result(slots.size() * numberOfReceiveRows, slots.size() * numberOfReceiveRows);
    for (int slotIndex = 0; slotIndex < (int)slots.size(); slotIndex++)
        for (int row = 0; row < numberOfReceiveRows; row++)
            for (int column = 0; column < numberOfReceiveRows; column++)
                result.get(slotIndex * numberOfReceiveRows + row, slotIndex * numberOfReceiveRows + column) =
                    slots[slotIndex].isConjugateObservation() ?
                        std::conj(perSlotCovariances[slotIndex].get(row, column)) :
                        perSlotCovariances[slotIndex].get(row, column);
    ChannelMatrixAlgebra::validatePositiveDefinite(result,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, "Space-time augmented covariance");
    return result;
}

ComplexMatrix SpaceTimeCodeDescriptor::stackCanonicalObservations(const ComplexMatrix& slotObservations) const
{
    validateFiniteMatrix(slotObservations, "Space-time slot observations");
    if (slotObservations.getNumRows() != (int)slots.size())
        throw cRuntimeError("Space-time observation matrix must have one row per slot");
    const int numberOfReceiveRows = slotObservations.getNumColumns();
    if (numberOfReceiveRows <= 0)
        throw cRuntimeError("Space-time observation matrix requires at least one receive column");
    ComplexMatrix result(slots.size() * numberOfReceiveRows, 1);
    for (int slotIndex = 0; slotIndex < (int)slots.size(); slotIndex++)
        for (int row = 0; row < numberOfReceiveRows; row++)
            result.get(slotIndex * numberOfReceiveRows + row, 0) = slots[slotIndex].isConjugateObservation() ?
                std::conj(slotObservations.get(slotIndex, row)) : slotObservations.get(slotIndex, row);
    return result;
}

std::vector<std::complex<double>> SpaceTimeCodeDescriptor::restoreSourceSymbols(
    const ComplexMatrix& canonicalEstimates) const
{
    ChannelMatrixAlgebra::validateDimensions(canonicalEstimates, numberOfSourceSymbols, 1,
        "Space-time canonical estimates");
    std::vector<std::complex<double>> result(numberOfSourceSymbols);
    for (int source = 0; source < numberOfSourceSymbols; source++)
        result[source] = decodedSymbolConjugated[source] ?
            std::conj(canonicalEstimates.get(source, 0)) : canonicalEstimates.get(source, 0);
    return result;
}

ComplexMatrix SpaceTimeCodeDescriptor::computeSlotSpaceTimeStreamCovariance(int slotIndex) const
{
    const Slot& slot = getSlot(slotIndex);
    const ComplexMatrix direct = ChannelMatrixAlgebra::multiply(slot.getDirectCoefficients(),
        ChannelMatrixAlgebra::conjugateTranspose(slot.getDirectCoefficients()));
    const ComplexMatrix conjugate = ChannelMatrixAlgebra::multiply(slot.getConjugateCoefficients(),
        ChannelMatrixAlgebra::conjugateTranspose(slot.getConjugateCoefficients()));
    return ChannelMatrixAlgebra::scale(ChannelMatrixAlgebra::add(direct, conjugate),
        std::complex<double>(amplitudeScale * amplitudeScale, 0));
}

ComplexMatrix SpaceTimeCodeDescriptor::computeJointAugmentedSpaceTimeStreamCovariance() const
{
    const int blockSize = numberOfSpaceTimeStreams;
    ComplexMatrix result(slots.size() * blockSize, slots.size() * blockSize);
    std::vector<ComplexMatrix> coefficients;
    coefficients.reserve(slots.size());
    for (int slotIndex = 0; slotIndex < (int)slots.size(); slotIndex++)
        coefficients.push_back(buildCanonicalCoefficientMatrix(slotIndex));
    for (int first = 0; first < (int)slots.size(); first++)
        for (int second = 0; second < (int)slots.size(); second++) {
            const ComplexMatrix block = ChannelMatrixAlgebra::multiply(coefficients[first],
                ChannelMatrixAlgebra::conjugateTranspose(coefficients[second]));
            for (int row = 0; row < blockSize; row++)
                for (int column = 0; column < blockSize; column++)
                    result.get(first * blockSize + row, second * blockSize + column) = block.get(row, column);
        }
    return result;
}

} // namespace physicallayer
} // namespace inet
