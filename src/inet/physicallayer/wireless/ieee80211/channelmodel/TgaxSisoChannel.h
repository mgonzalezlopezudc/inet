//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGAXSISOCHANNEL_H
#define __INET_TGAXSISOCHANNEL_H

#include <complex>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {

namespace physicallayer {

using namespace inet::units::values;

/**
 * Immutable SISO tapped-delay channel realization. Coefficients are expected
 * to already include their PDP amplitude scaling and are not renormalized.
 * Consequently, the instantaneous power gain may be greater than one.
 * Tap order is preserved and used as the stable summation order.
 */
class INET_API TgaxSisoChannel
{
  public:
    struct DopplerComponent {
        Hz frequency;
        std::complex<double> coefficient;
    };

    struct Tap {
        simsec excessDelay;
        // The constant component is also the complete coefficient for a
        // static tap. Dynamic NLOS taps add the Doppler components below.
        std::complex<double> coefficient;
        std::vector<DopplerComponent> dopplerComponents;
    };

  protected:
    const std::vector<Tap> taps;

  protected:
    std::vector<std::complex<double>> computeTapCoefficients(simsec time) const;
    std::complex<double> computeResponse(Hz frequencyOffset,
            const std::vector<std::complex<double>>& tapCoefficients) const;

  public:
    explicit TgaxSisoChannel(std::vector<Tap> taps);

    const std::vector<Tap>& getTaps() const;
    std::complex<double> computeResponse(Hz frequencyOffset, simsec time = simsec(0)) const;
    double computePowerGain(Hz frequencyOffset, simsec time = simsec(0)) const;
    std::vector<double> computePowerGains(const std::vector<Hz>& frequencyOffsets, simsec time) const;
};

} // namespace physicallayer

} // namespace inet

#endif
