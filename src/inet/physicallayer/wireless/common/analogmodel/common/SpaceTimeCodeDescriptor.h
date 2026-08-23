//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_SPACETIMECODEDESCRIPTOR_H
#define __INET_SPACETIMECODEDESCRIPTOR_H

#include <complex>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"

namespace inet {
namespace physicallayer {

/**
 * Technology-neutral linear-dispersion description of a small space-time
 * code.  A slot is represented as
 *
 *     x_t = amplitudeScale (A_t s + B_t conj(s)).
 *
 * The descriptor also records which received slots are conjugated when they
 * are placed in the canonical augmented observation vector.  The value has
 * no simulator ownership or mutable processing state.
 */
class INET_API SpaceTimeCodeDescriptor final
{
  public:
    class INET_API Slot final
    {
      private:
        ComplexMatrix directCoefficients;
        ComplexMatrix conjugateCoefficients;
        bool conjugateObservation;

      public:
        Slot(const ComplexMatrix& directCoefficients, const ComplexMatrix& conjugateCoefficients,
            bool conjugateObservation);

        const ComplexMatrix& getDirectCoefficients() const { return directCoefficients; }
        const ComplexMatrix& getConjugateCoefficients() const { return conjugateCoefficients; }
        bool isConjugateObservation() const { return conjugateObservation; }
    };

  private:
    int numberOfSpatialStreams;
    int numberOfSourceSymbols;
    int numberOfSpaceTimeStreams;
    double amplitudeScale;
    std::vector<Slot> slots;
    std::vector<bool> decodedSymbolConjugated;

    ComplexMatrix buildCanonicalCoefficientMatrix(int slotIndex) const;
    void validateCanonicalLinearity() const;

  public:
    SpaceTimeCodeDescriptor(int numberOfSpatialStreams, int numberOfSourceSymbols,
        int numberOfSpaceTimeStreams, double amplitudeScale, const std::vector<Slot>& slots,
        const std::vector<bool>& decodedSymbolConjugated);

    int getNumberOfSpatialStreams() const { return numberOfSpatialStreams; }
    int getNumberOfSourceSymbols() const { return numberOfSourceSymbols; }
    int getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
    double getAmplitudeScale() const { return amplitudeScale; }
    int getNumberOfSlots() const { return slots.size(); }
    const Slot& getSlot(int slotIndex) const;
    const std::vector<Slot>& getSlots() const { return slots; }
    const std::vector<bool>& getDecodedSymbolConjugated() const { return decodedSymbolConjugated; }

    std::vector<ComplexMatrix> encodeSourceBlock(const std::vector<std::complex<double>>& symbols) const;

    /** Builds the canonical augmented channel in slot/receive-row order. */
    ComplexMatrix buildCanonicalAugmentedChannel(const ComplexMatrix& effectiveStsChannel) const;
    /** Builds it from one independently sampled effective channel per code slot. */
    ComplexMatrix buildCanonicalAugmentedChannel(
        const std::vector<ComplexMatrix>& effectiveStsChannels) const;

    /** Builds independent proper-noise covariance in canonical observation order. */
    ComplexMatrix buildCanonicalAugmentedCovariance(const ComplexMatrix& perSlotCovariance) const;
    /** Builds block-diagonal slot-specific covariance in canonical observation order. */
    ComplexMatrix buildCanonicalAugmentedCovariance(
        const std::vector<ComplexMatrix>& perSlotCovariances) const;

    ComplexMatrix stackCanonicalObservations(const ComplexMatrix& slotObservations) const;

    std::vector<std::complex<double>> restoreSourceSymbols(const ComplexMatrix& canonicalEstimates) const;

    ComplexMatrix computeSlotSpaceTimeStreamCovariance(int slotIndex) const;
    ComplexMatrix computeJointAugmentedSpaceTimeStreamCovariance() const;
};

} // namespace physicallayer
} // namespace inet

#endif
