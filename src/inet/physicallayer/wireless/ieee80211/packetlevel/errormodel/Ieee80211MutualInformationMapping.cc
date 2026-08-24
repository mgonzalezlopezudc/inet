//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211MutualInformationMapping.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

#include "inet/physicallayer/wireless/common/base/packetlevel/ApskModulationBase.h"

namespace inet {
namespace physicallayer {

namespace {

constexpr int NUMBER_OF_QUADRATURE_POINTS = 32;
constexpr double LOG_TWO = 0.693147180559945309417232121458176568;
constexpr double UNIQUE_COORDINATE_TOLERANCE = 1e-12;

// Nodes and weights for E[ f(Z) ], Z~N(0,1), obtained from the 32-point
// physicists' Gauss-Hermite rule: z=sqrt(2)*x and w=w_H/sqrt(pi).  Keeping
// this table in source makes the mapping deterministic across libm/library
// versions and avoids an adaptive integration path in packet processing.
constexpr std::array<double, NUMBER_OF_QUADRATURE_POINTS> QUADRATURE_NODES = {
    -10.077422674229467, -9.0643992107024065, -8.2197287653822464, -7.4607557541215197,
    -6.7559308305407049, -6.0889643090769878, -5.4500332736234292, -4.8326046132444889,
    -4.2320211099954097, -3.6447812498808334, -3.0681351690131216, -2.4998404151873954,
    -1.9380049059257174, -1.3809801992721442, -0.82728490377976516, -0.27554641923027584,
    0.27554641923027584, 0.82728490377976516, 1.3809801992721442, 1.9380049059257174,
    2.4998404151873954, 3.0681351690131216, 3.6447812498808334, 4.2320211099954097,
    4.8326046132444889, 5.4500332736234292, 6.0889643090769878, 6.7559308305407049,
    7.4607557541215197, 8.2197287653822464, 9.0643992107024065, 10.077422674229467};
constexpr std::array<double, NUMBER_OF_QUADRATURE_POINTS> QUADRATURE_WEIGHTS = {
    4.1246074890182323e-23, 5.2084495919608421e-19, 6.7552902236701287e-16, 2.3780648557777909e-13,
    3.3475012398012285e-11, 2.312518412074231e-09, 8.8812907131058625e-08, 2.0596221039534300e-06,
    3.0559803060896349e-05, 0.00030255702581706253, 0.0020620510513078838, 0.0099034617023205963,
    0.034109847726092095, 0.085344808272080672, 0.1565389937575985, 0.21170556988047937,
    0.21170556988047937, 0.1565389937575985, 0.085344808272080672, 0.034109847726092095,
    0.0099034617023205963, 0.0020620510513078838, 0.00030255702581706253, 3.0559803060896349e-05,
    2.0596221039534300e-06, 8.8812907131058625e-08, 2.312518412074231e-09, 3.3475012398012285e-11,
    2.3780648557777909e-13, 6.7552902236701287e-16, 5.2084495919608421e-19, 4.1246074890182323e-23};

struct AxisGeometry {
    std::vector<double> levels;
    std::vector<std::vector<int>> labels;
};

struct ConstellationGeometry {
    AxisGeometry real;
    AxisGeometry imag;
    unsigned int codeWordSize = 0;
};

static bool close(double a, double b)
{
    return std::abs(a - b) <= UNIQUE_COORDINATE_TOLERANCE * std::max({1.0, std::abs(a), std::abs(b)});
}

static int findCoordinate(const std::vector<double>& coordinates, double value)
{
    for (int i = 0; i < int(coordinates.size()); i++)
        if (close(coordinates[i], value))
            return i;
    return -1;
}

static int integerLog2(unsigned int value)
{
    int result = 0;
    while ((1u << result) < value)
        result++;
    return (1u << result) == value ? result : -1;
}

static void reject(const char *reason)
{
    throw cRuntimeError("Ieee80211MutualInformationMapping: %s", reason);
}

static ConstellationGeometry buildGeometry(const ApskModulationBase *modulation)
{
    if (modulation == nullptr || modulation->getConstellation() == nullptr)
        reject("requires a non-null finite constellation");
    const auto *constellation = modulation->getConstellation();
    const unsigned int constellationSize = modulation->getConstellationSize();
    const unsigned int codeWordSize = modulation->getCodeWordSize();
    if (constellationSize == 0 || constellation->size() != constellationSize)
        reject("constellation size is empty or inconsistent");
    if (integerLog2(constellationSize) < 0 || codeWordSize != unsigned(integerLog2(constellationSize)))
        reject("constellation size must be a power of two matching code-word size");

    std::vector<double> realCoordinates;
    std::vector<double> imagCoordinates;
    std::vector<int> realIndex(constellationSize), imagIndex(constellationSize);
    for (unsigned int i = 0; i < constellationSize; i++) {
        const auto& symbol = constellation->at(i);
        if (!std::isfinite(symbol.real()) || !std::isfinite(symbol.imag()))
            reject("constellation contains a non-finite coordinate");
        realIndex[i] = findCoordinate(realCoordinates, symbol.real());
        if (realIndex[i] < 0) {
            realIndex[i] = realCoordinates.size();
            realCoordinates.push_back(symbol.real());
        }
        imagIndex[i] = findCoordinate(imagCoordinates, symbol.imag());
        if (imagIndex[i] < 0) {
            imagIndex[i] = imagCoordinates.size();
            imagCoordinates.push_back(symbol.imag());
        }
    }
    if (realCoordinates.size() * imagCoordinates.size() != constellationSize)
        reject("constellation must be a complete rectangular I/Q grid");

    std::vector<int> axisOfBit(codeWordSize, -1);
    std::vector<int> localBitIndex(codeWordSize, -1);
    int realBitCount = 0;
    int imagBitCount = 0;
    for (unsigned int bit = 0; bit < codeWordSize; bit++) {
        bool dependsOnlyOnReal = true;
        bool dependsOnlyOnImag = true;
        for (unsigned int i = 0; i < constellationSize; i++) {
            for (unsigned int j = i + 1; j < constellationSize; j++) {
                const bool bitI = ((i >> bit) & 1u) != 0;
                const bool bitJ = ((j >> bit) & 1u) != 0;
                if (realIndex[i] == realIndex[j] && bitI != bitJ)
                    dependsOnlyOnReal = false;
                if (imagIndex[i] == imagIndex[j] && bitI != bitJ)
                    dependsOnlyOnImag = false;
            }
        }
        if (dependsOnlyOnReal == dependsOnlyOnImag)
            reject("constellation labels must assign every nonconstant bit to exactly one I/Q axis");
        axisOfBit[bit] = dependsOnlyOnReal ? 0 : 1;
        localBitIndex[bit] = dependsOnlyOnReal ? realBitCount++ : imagBitCount++;
    }
    if (realBitCount == 0 && imagBitCount == 0)
        reject("constellation has no information bits");
    if ((1u << realBitCount) != realCoordinates.size() || (1u << imagBitCount) != imagCoordinates.size())
        reject("constellation labels do not cover the rectangular axes exactly");

    ConstellationGeometry result;
    result.codeWordSize = codeWordSize;
    result.real.levels.assign(1u << realBitCount, std::numeric_limits<double>::quiet_NaN());
    result.imag.levels.assign(1u << imagBitCount, std::numeric_limits<double>::quiet_NaN());
    result.real.labels.assign(result.real.levels.size(), std::vector<int>(realBitCount));
    result.imag.labels.assign(result.imag.levels.size(), std::vector<int>(imagBitCount));
    std::vector<int> realLabelForCoordinate(realCoordinates.size(), -1);
    std::vector<int> imagLabelForCoordinate(imagCoordinates.size(), -1);
    std::vector<bool> seenPairs(constellationSize, false);
    for (unsigned int i = 0; i < constellationSize; i++) {
        int realLabel = 0;
        int imagLabel = 0;
        for (unsigned int bit = 0; bit < codeWordSize; bit++) {
            const int label = ((i >> bit) & 1u) != 0;
            if (axisOfBit[bit] == 0)
                realLabel |= label << localBitIndex[bit];
            else
                imagLabel |= label << localBitIndex[bit];
        }
        for (unsigned int bit = 0; bit < codeWordSize; bit++) {
            const int label = ((i >> bit) & 1u) != 0;
            if (axisOfBit[bit] == 0)
                result.real.labels[realLabel][localBitIndex[bit]] = label;
            else
                result.imag.labels[imagLabel][localBitIndex[bit]] = label;
        }
        if (realLabelForCoordinate[realIndex[i]] >= 0 && realLabelForCoordinate[realIndex[i]] != realLabel)
            reject("a real coordinate has more than one bit label");
        if (imagLabelForCoordinate[imagIndex[i]] >= 0 && imagLabelForCoordinate[imagIndex[i]] != imagLabel)
            reject("an imaginary coordinate has more than one bit label");
        realLabelForCoordinate[realIndex[i]] = realLabel;
        imagLabelForCoordinate[imagIndex[i]] = imagLabel;
        if (!std::isnan(result.real.levels[realLabel]) && !close(result.real.levels[realLabel], constellation->at(i).real()))
            reject("a real bit label maps to more than one coordinate");
        if (!std::isnan(result.imag.levels[imagLabel]) && !close(result.imag.levels[imagLabel], constellation->at(i).imag()))
            reject("an imaginary bit label maps to more than one coordinate");
        result.real.levels[realLabel] = constellation->at(i).real();
        result.imag.levels[imagLabel] = constellation->at(i).imag();
        const unsigned int pair = unsigned(realLabel) * result.imag.levels.size() + unsigned(imagLabel);
        if (pair >= seenPairs.size() || seenPairs[pair])
            reject("constellation has duplicate or incomplete I/Q label pairs");
        seenPairs[pair] = true;
    }
    for (double level : result.real.levels)
        if (!std::isfinite(level))
            reject("real axis labels are incomplete");
    for (double level : result.imag.levels)
        if (!std::isfinite(level))
            reject("imaginary axis labels are incomplete");
    return result;
}

static double logSumExp(const std::vector<double>& values)
{
    double maximum = -std::numeric_limits<double>::infinity();
    for (double value : values)
        maximum = std::max(maximum, value);
    double sum = 0;
    for (double value : values)
        sum += std::exp(value - maximum);
    return maximum + std::log(sum);
}

static double axisSymbolMutualInformation(const AxisGeometry& axis, double snr)
{
    if (axis.levels.size() <= 1 || snr == 0)
        return 0;
    const double noiseScale = std::sqrt(2 * snr);
    const double logCardinality = std::log(double(axis.levels.size()));
    std::vector<double> logLikelihoods(axis.levels.size());
    double mutualInformation = 0;
    for (size_t input = 0; input < axis.levels.size(); input++) {
        for (int node = 0; node < NUMBER_OF_QUADRATURE_POINTS; node++) {
            const double received = axis.levels[input] + QUADRATURE_NODES[node] / noiseScale;
            for (size_t candidate = 0; candidate < axis.levels.size(); candidate++) {
                const double distance = received - axis.levels[candidate];
                logLikelihoods[candidate] = -snr * distance * distance;
            }
            const double logPosteriorDenominator = logSumExp(logLikelihoods);
            const double information = (logCardinality - logPosteriorDenominator + logLikelihoods[input]) / LOG_TWO;
            // Information density may be negative for an individual noise
            // sample.  Clamp only the expected mutual information below;
            // clamping here would bias the quadrature result.
            mutualInformation += QUADRATURE_WEIGHTS[node] * information;
        }
    }
    return std::clamp(mutualInformation / double(axis.levels.size()), 0.0, logCardinality / LOG_TWO);
}

static double axisBitMutualInformation(const AxisGeometry& axis, double snr)
{
    const int bits = axis.labels.empty() ? 0 : axis.labels.front().size();
    if (bits == 0 || snr == 0)
        return 0;
    const double noiseScale = std::sqrt(2 * snr);
    std::vector<double> logLikelihoods(axis.levels.size());
    double mutualInformation = 0;
    for (int bit = 0; bit < bits; bit++) {
        double bitInformation = 0;
        for (size_t input = 0; input < axis.levels.size(); input++) {
            for (int node = 0; node < NUMBER_OF_QUADRATURE_POINTS; node++) {
                const double received = axis.levels[input] + QUADRATURE_NODES[node] / noiseScale;
                for (size_t candidate = 0; candidate < axis.levels.size(); candidate++) {
                    const double distance = received - axis.levels[candidate];
                    logLikelihoods[candidate] = -snr * distance * distance;
                }
                std::vector<double> sameBit, otherBit;
                for (size_t candidate = 0; candidate < axis.levels.size(); candidate++) {
                    if (axis.labels[candidate][bit] == axis.labels[input][bit])
                        sameBit.push_back(logLikelihoods[candidate]);
                    else
                        otherBit.push_back(logLikelihoods[candidate]);
                }
                const double logSame = logSumExp(sameBit);
                const double logOther = logSumExp(otherBit);
                // The same/other likelihood ratio already has the proper
                // transmitted-bit sign: "same" is conditioned on the input
                // bit, while "other" is conditioned on its complement.
                const double logRatio = logOther - logSame;
                const double loss = logRatio > 700 ? std::numeric_limits<double>::infinity() : std::log1p(std::exp(logRatio)) / LOG_TWO;
                // As with symbol information density, a negative sample
                // contribution is valid and must reach the expectation.
                bitInformation += QUADRATURE_WEIGHTS[node] * (1.0 - loss);
            }
        }
        mutualInformation += bitInformation / double(axis.levels.size());
    }
    return std::clamp(mutualInformation / bits, 0.0, 1.0);
}

template<bool BIT_METRIC>
static double computeMutualInformation(const ApskModulationBase *modulation, double snr)
{
    if (!std::isfinite(snr) || snr < 0)
        throw cRuntimeError("Ieee80211MutualInformationMapping: SNR must be finite and nonnegative");
    const auto geometry = buildGeometry(modulation);
    if (snr == 0)
        return 0;
    const int realBits = geometry.real.labels.empty() ? 0 : geometry.real.labels.front().size();
    const int imagBits = geometry.imag.labels.empty() ? 0 : geometry.imag.labels.front().size();
    if constexpr (BIT_METRIC) {
        const double value = (realBits * axisBitMutualInformation(geometry.real, snr) + imagBits * axisBitMutualInformation(geometry.imag, snr)) / geometry.codeWordSize;
        return std::clamp(value, 0.0, 1.0);
    }
    else {
        const double value = (axisSymbolMutualInformation(geometry.real, snr) + axisSymbolMutualInformation(geometry.imag, snr)) / geometry.codeWordSize;
        return std::clamp(value, 0.0, 1.0);
    }
}

template<bool BIT_METRIC>
static double computeEffectiveSnr(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta)
{
    if (carrierSnr.empty() || !std::isfinite(beta) || beta <= 0)
        throw cRuntimeError("Ieee80211MutualInformationMapping: effective SNR requires a nonempty vector and finite beta > 0");
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = 0;
    for (double snr : carrierSnr) {
        if (!std::isfinite(snr) || snr < 0)
            throw cRuntimeError("Ieee80211MutualInformationMapping: carrier SNIR must be finite and nonnegative");
        minimum = std::min(minimum, snr);
        maximum = std::max(maximum, snr);
    }
    if (maximum == 0)
        return 0;
    const double minimumScaled = minimum / beta;
    const double maximumScaled = maximum / beta;
    const double target = [&] {
        double sum = 0;
        for (double snr : carrierSnr)
            sum += computeMutualInformation<BIT_METRIC>(modulation, snr / beta);
        return sum / carrierSnr.size();
    }();
    if (target <= 0)
        return 0;
    double lower = minimumScaled;
    double upper = maximumScaled;
    const double upperMetric = computeMutualInformation<BIT_METRIC>(modulation, upper);
    if (target >= upperMetric)
        return maximum;
    for (int iteration = 0; iteration < 64; iteration++) {
        const double middle = lower + (upper - lower) / 2;
        if (computeMutualInformation<BIT_METRIC>(modulation, middle) < target)
            lower = middle;
        else
            upper = middle;
    }
    return beta * (lower + (upper - lower) / 2);
}

template<bool BIT_METRIC>
static double computeEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta)
{
    const double snr = computeEffectiveSnr<BIT_METRIC>(carrierSnr, modulation, beta);
    return snr == 0 ? -std::numeric_limits<double>::infinity() : 10 * std::log10(snr);
}

} // namespace

double Ieee80211MutualInformationMapping::computeSymbolMutualInformation(const ApskModulationBase *modulation, double snr)
{
    return computeMutualInformation<false>(modulation, snr);
}

double Ieee80211MutualInformationMapping::computeBitMutualInformation(const ApskModulationBase *modulation, double snr)
{
    return computeMutualInformation<true>(modulation, snr);
}

double Ieee80211MutualInformationMapping::computeSymbolEffectiveSnr(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta)
{
    return computeEffectiveSnr<false>(carrierSnr, modulation, beta);
}

double Ieee80211MutualInformationMapping::computeBitEffectiveSnr(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta)
{
    return computeEffectiveSnr<true>(carrierSnr, modulation, beta);
}

double Ieee80211MutualInformationMapping::computeSymbolEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta)
{
    return computeEffectiveSnrDb<false>(carrierSnr, modulation, beta);
}

double Ieee80211MutualInformationMapping::computeBitEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta)
{
    return computeEffectiveSnrDb<true>(carrierSnr, modulation, beta);
}

} // namespace physicallayer
} // namespace inet
