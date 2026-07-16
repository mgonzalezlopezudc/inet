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
    struct Tap {
        simsec excessDelay;
        std::complex<double> coefficient;
    };

  protected:
    const std::vector<Tap> taps;

  public:
    explicit TgaxSisoChannel(std::vector<Tap> taps);

    const std::vector<Tap>& getTaps() const;
    std::complex<double> computeResponse(Hz frequencyOffset) const;
    double computePowerGain(Hz frequencyOffset) const;
};

} // namespace physicallayer

} // namespace inet

#endif
