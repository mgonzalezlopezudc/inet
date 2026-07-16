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
        for (const auto& component : tap.dopplerComponents) {
            if (!std::isfinite(component.frequency.get<Hz>()))
                throw cRuntimeError("TGax SISO channel Doppler frequency must be finite");
            if (!std::isfinite(component.coefficient.real()) || !std::isfinite(component.coefficient.imag()))
                throw cRuntimeError("TGax SISO channel Doppler coefficient must be finite");
        }
    }
}

const std::vector<TgaxSisoChannel::Tap>& TgaxSisoChannel::getTaps() const
{
    return taps;
}

std::vector<std::complex<double>> TgaxSisoChannel::computeTapCoefficients(simsec time) const
{
    auto timeSeconds = time.get<s>().dbl();
    if (!std::isfinite(timeSeconds))
        throw cRuntimeError("TGax SISO channel time must be finite");
    std::vector<std::complex<double>> result;
    result.reserve(taps.size());
    for (const auto& tap : taps) {
        auto tapCoefficient = tap.coefficient;
        for (const auto& component : tap.dopplerComponents) {
            long double dopplerPhaseArgument = 2.0L * M_PI *
                    (long double)component.frequency.get<Hz>() * (long double)timeSeconds;
            auto dopplerPhase = (double)std::remainder(dopplerPhaseArgument, 2.0L * M_PI);
            tapCoefficient += component.coefficient * std::polar(1.0, dopplerPhase);
        }
        result.push_back(tapCoefficient);
    }
    return result;
}

std::complex<double> TgaxSisoChannel::computeResponse(Hz frequencyOffset,
        const std::vector<std::complex<double>>& tapCoefficients) const
{
    auto frequency = frequencyOffset.get<Hz>();
    if (!std::isfinite(frequency))
        throw cRuntimeError("TGax SISO channel frequency offset must be finite");
    if (tapCoefficients.size() != taps.size())
        throw cRuntimeError("TGax SISO channel tap coefficient count mismatch");

    double realResponse = 0;
    double imaginaryResponse = 0;
    double realCorrection = 0;
    double imaginaryCorrection = 0;
    for (size_t i = 0; i < taps.size(); i++) {
        const auto& tap = taps[i];
        auto delay = tap.excessDelay.get<s>().dbl();
        long double phaseArgument = -2.0L * M_PI * (long double)frequency * (long double)delay;
        if (!std::isfinite(phaseArgument))
            throw cRuntimeError("TGax SISO channel phase argument must be finite");
        auto phase = (double)std::remainder(phaseArgument, 2.0L * M_PI);
        auto term = tapCoefficients[i] * std::polar(1.0, phase);

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

std::complex<double> TgaxSisoChannel::computeResponse(Hz frequencyOffset, simsec time) const
{
    return computeResponse(frequencyOffset, computeTapCoefficients(time));
}

double TgaxSisoChannel::computePowerGain(Hz frequencyOffset, simsec time) const
{
    return std::norm(computeResponse(frequencyOffset, time));
}

std::vector<double> TgaxSisoChannel::computePowerGains(const std::vector<Hz>& frequencyOffsets, simsec time) const
{
    auto tapCoefficients = computeTapCoefficients(time);
    std::vector<double> result;
    result.reserve(frequencyOffsets.size());
    for (auto frequencyOffset : frequencyOffsets)
        result.push_back(std::norm(computeResponse(frequencyOffset, tapCoefficients)));
    return result;
}

} // namespace physicallayer

} // namespace inet
