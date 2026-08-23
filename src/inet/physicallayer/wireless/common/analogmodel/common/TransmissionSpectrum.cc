//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/common/TransmissionSpectrum.h"

#include <cmath>

namespace inet {
namespace physicallayer {

void TransmissionSpectrum::validate(const std::vector<PowerSpectralBand>& bands)
{
    if (bands.empty())
        throw cRuntimeError("Transmission spectrum must contain at least one band");
    double sum = 0;
    Hz previousUpper = Hz(NaN);
    for (const auto& band : bands) {
        if (!(band.upperFrequencyOffset > band.lowerFrequencyOffset))
            throw cRuntimeError("Transmission spectrum contains an empty or reversed band");
        if (!std::isfinite(band.lowerFrequencyOffset.get()) || !std::isfinite(band.upperFrequencyOffset.get()))
            throw cRuntimeError("Transmission spectrum contains a non-finite frequency offset");
        if (!std::isfinite(band.powerFraction) || band.powerFraction <= 0)
            throw cRuntimeError("Transmission spectrum power fractions must be finite and positive");
        if (!std::isnan(previousUpper.get()) && band.lowerFrequencyOffset < previousUpper)
            throw cRuntimeError("Transmission spectrum bands overlap or are not ordered");
        previousUpper = band.upperFrequencyOffset;
        sum += band.powerFraction;
    }
    if (std::abs(sum - 1.0) > 1e-12)
        throw cRuntimeError("Transmission spectrum power fractions must sum to one, got %.17g", sum);
}

TransmissionSpectrum::TransmissionSpectrum(const std::vector<PowerSpectralBand>& bands, Hz nominalBandwidth) :
    bands(bands)
{
    validate(this->bands);
    if (std::isfinite(nominalBandwidth.get())) {
        if (!(nominalBandwidth > Hz(0)))
            throw cRuntimeError("Transmission spectrum nominal bandwidth must be positive");
        for (const auto& band : this->bands)
            if (band.lowerFrequencyOffset < -nominalBandwidth / 2 || band.upperFrequencyOffset > nominalBandwidth / 2)
                throw cRuntimeError("Transmission spectrum band lies outside the nominal bandwidth");
    }
}

std::ostream& TransmissionSpectrum::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "TransmissionSpectrum";
    if (level <= PRINT_LEVEL_DETAIL)
        stream << EV_FIELD(bands.size());
    return stream;
}

} // namespace physicallayer
} // namespace inet
