//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxSisoChannel.h"

#include <cmath>
#include <utility>

namespace inet {

namespace physicallayer {

TgaxSisoChannel::TgaxSisoChannel(std::vector<Tap> taps) :
    taps(std::move(taps))
{
    if (this->taps.empty())
        throw cRuntimeError("TGax SISO channel must contain at least one tap");
    for (const auto& tap : this->taps) {
        auto excessDelay = tap.excessDelay.get<s>().dbl();
        if (!std::isfinite(excessDelay) || excessDelay < 0)
            throw cRuntimeError("TGax SISO channel tap delay must be finite and non-negative");
        if (!std::isfinite(tap.coefficient.real()) || !std::isfinite(tap.coefficient.imag()))
            throw cRuntimeError("TGax SISO channel tap coefficient must be finite");
    }
}

const std::vector<TgaxSisoChannel::Tap>& TgaxSisoChannel::getTaps() const
{
    return taps;
}

std::complex<double> TgaxSisoChannel::computeResponse(Hz frequencyOffset) const
{
    auto frequency = frequencyOffset.get<Hz>();
    if (!std::isfinite(frequency))
        throw cRuntimeError("TGax SISO channel frequency offset must be finite");

    double realResponse = 0;
    double imaginaryResponse = 0;
    double realCorrection = 0;
    double imaginaryCorrection = 0;
    for (const auto& tap : taps) {
        auto delay = tap.excessDelay.get<s>().dbl();
        long double phaseArgument = -2.0L * M_PI * (long double)frequency * (long double)delay;
        if (!std::isfinite(phaseArgument))
            throw cRuntimeError("TGax SISO channel phase argument must be finite");
        auto phase = (double)std::remainder(phaseArgument, 2.0L * M_PI);
        auto term = tap.coefficient * std::polar(1.0, phase);

        auto correctedReal = term.real() - realCorrection;
        auto nextReal = realResponse + correctedReal;
        realCorrection = (nextReal - realResponse) - correctedReal;
        realResponse = nextReal;

        auto correctedImaginary = term.imag() - imaginaryCorrection;
        auto nextImaginary = imaginaryResponse + correctedImaginary;
        imaginaryCorrection = (nextImaginary - imaginaryResponse) - correctedImaginary;
        imaginaryResponse = nextImaginary;
    }
    return {realResponse, imaginaryResponse};
}

double TgaxSisoChannel::computePowerGain(Hz frequencyOffset) const
{
    return std::norm(computeResponse(frequencyOffset));
}

} // namespace physicallayer

} // namespace inet
