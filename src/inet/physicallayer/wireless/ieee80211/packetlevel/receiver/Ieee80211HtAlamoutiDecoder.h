//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTALAMOUTIDECODER_H
#define __INET_IEEE80211HTALAMOUTIDECODER_H

#include <complex>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"

namespace inet {
namespace physicallayer {

/**
 * Pure HT Alamouti detector.  It operates on a constant STS channel and
 * independent proper per-slot covariance; channel estimation, cross-slot
 * pseudocovariance, and symbol decisions remain outside this bounded seam.
 */
class INET_API Ieee80211HtAlamoutiDecoder final
{
  public:
    class INET_API Result final
    {
      private:
        ChannelMatrixDetectionResult detectionResult;
        std::vector<std::complex<double>> sourceSymbols;
        bool recovered;

      public:
        Result(const ChannelMatrixDetectionResult& detectionResult,
            const std::vector<std::complex<double>>& sourceSymbols, bool recovered);

        const ChannelMatrixDetectionResult& getDetectionResult() const { return detectionResult; }
        const std::vector<std::complex<double>>& getSourceSymbols() const { return sourceSymbols; }
        bool hasRecoveredSourceSymbols() const { return recovered; }
        double getEffectiveSinr() const;
    };

    static ChannelMatrixDetectionResult detect(const SpaceTimeCodeDescriptor& descriptor,
        const ComplexMatrix& effectivePowerScaledStsChannel, const ComplexMatrix& perSlotCovariance);

    static Result decode(const SpaceTimeCodeDescriptor& descriptor,
        const ComplexMatrix& effectivePowerScaledStsChannel, const ComplexMatrix& perSlotCovariance,
        const ComplexMatrix& slotObservations);
};

} // namespace physicallayer
} // namespace inet

#endif
