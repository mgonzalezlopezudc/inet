//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211Eesm.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211PerTable.h"

namespace inet {
namespace physicallayer {

namespace {

static std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

static std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ','))
        fields.push_back(trim(field));
    return fields;
}

static int parseInt(const std::string& text, const char *field)
{
    size_t position = 0;
    int result;
    try { result = std::stoi(text, &position); }
    catch (...) { throw cRuntimeError("Ieee80211Eesm: invalid %s '%s'", field, text.c_str()); }
    if (position != text.size())
        throw cRuntimeError("Ieee80211Eesm: invalid %s '%s'", field, text.c_str());
    return result;
}

static double parseDouble(const std::string& text, const char *field)
{
    size_t position = 0;
    double result;
    try { result = std::stod(text, &position); }
    catch (...) { throw cRuntimeError("Ieee80211Eesm: invalid %s '%s'", field, text.c_str()); }
    if (position != text.size() || !std::isfinite(result))
        throw cRuntimeError("Ieee80211Eesm: invalid finite %s '%s'", field, text.c_str());
    return result;
}

} // namespace

double Ieee80211Eesm::computeEffectiveSnr(const std::vector<double>& carrierSnr, double beta)
{
    if (carrierSnr.empty() || !std::isfinite(beta) || beta <= 0)
        throw cRuntimeError("Ieee80211Eesm: EESM requires a nonempty vector and finite beta > 0");
    double minimum = std::numeric_limits<double>::infinity();
    for (double snr : carrierSnr) {
        if (!std::isfinite(snr) || snr < 0)
            throw cRuntimeError("Ieee80211Eesm: carrier SNIR must be finite and nonnegative");
        minimum = std::min(minimum, snr);
    }
    double sum = 0;
    for (double snr : carrierSnr)
        sum += std::exp(-(snr - minimum) / beta);
    return minimum - beta * (std::log(sum) - std::log(double(carrierSnr.size())));
}

double Ieee80211Eesm::computeEffectiveSnrDb(const std::vector<double>& carrierSnr, double beta)
{
    const double snr = computeEffectiveSnr(carrierSnr, beta);
    if (snr == 0)
        return -std::numeric_limits<double>::infinity();
    return 10 * std::log10(snr);
}

bool Ieee80211Eesm::hasSignificantInteriorVariation(double minimum, double maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum))
        return true;
    const double tolerance = std::max(1e-15, 1e-12 * std::max(std::abs(minimum), std::abs(maximum)));
    return std::abs(maximum - minimum) > tolerance;
}

void Ieee80211Eesm::loadBetaCsv(const std::string& filename, const std::string& calibrationSet)
{
    if (calibrationSet.empty())
        throw cRuntimeError("Ieee80211Eesm: calibration set must not be empty");
    static const char *const supportedSets[] = {
        "patidar2017-bcc-d20", "patidar2017-bcc-d40", "patidar2017-bcc-e20", "patidar2017-bcc-e40"};
    int setIndex = -1;
    for (int i = 0; i < 4; i++)
        if (calibrationSet == supportedSets[i])
            setIndex = i;
    if (setIndex < 0)
        throw cRuntimeError("Ieee80211Eesm: unsupported published calibration set '%s'", calibrationSet.c_str());
    const int expectedBandwidth = (setIndex == 0 || setIndex == 2) ? 20 : 40;
    const int expectedDataCarriers = expectedBandwidth == 20 ? 52 : 108;
    std::ifstream input(filename);
    if (!input)
        throw cRuntimeError("Ieee80211Eesm: cannot open beta table '%s'", filename.c_str());
    clear();
    std::string line;
    bool sawHeader = false;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        auto fields = split(line);
        if (!sawHeader && !fields.empty() && fields[0] == "calibration_set") {
            if (fields.size() != 6 || fields[1] != "bandwidth_mhz" || fields[2] != "mcs" || fields[3] != "data_scope" || fields[4] != "data_carriers" || fields[5] != "beta")
                throw cRuntimeError("Ieee80211Eesm: invalid beta CSV header in '%s'", filename.c_str());
            sawHeader = true;
            continue;
        }
        if (fields.size() != 6)
            throw cRuntimeError("Ieee80211Eesm: expected six beta CSV fields in '%s'", filename.c_str());
        if (fields[0] != calibrationSet)
            continue;
        int bandwidthMHz = parseInt(fields[1], "bandwidth_mhz");
        int mcs = parseInt(fields[2], "mcs");
        int dataCarrierCount = parseInt(fields[4], "data_carriers");
        double beta = parseDouble(fields[5], "beta");
        if ((bandwidthMHz != 20 && bandwidthMHz != 40) || mcs < 0 || mcs > 7 || fields[3] != "dataOnly" || (dataCarrierCount != 52 && dataCarrierCount != 108) || beta <= 0)
            throw cRuntimeError("Ieee80211Eesm: unsupported beta row in '%s'", filename.c_str());
        Key key{calibrationSet, bandwidthMHz, mcs};
        if (!betas.emplace(key, Beta{dataCarrierCount, beta}).second)
            throw cRuntimeError("Ieee80211Eesm: duplicate beta key for %s %d MHz MCS %d", calibrationSet.c_str(), bandwidthMHz, mcs);
    }
    if (!sawHeader)
        throw cRuntimeError("Ieee80211Eesm: beta CSV '%s' is missing its schema header", filename.c_str());
    if (betas.size() != 8)
        throw cRuntimeError("Ieee80211Eesm: calibration set '%s' must contain exactly eight MCS rows, got %zu", calibrationSet.c_str(), betas.size());
    for (int mcs = 0; mcs <= 7; mcs++) {
        auto it = betas.find({calibrationSet, expectedBandwidth, mcs});
        if (it == betas.end())
            throw cRuntimeError("Ieee80211Eesm: calibration set '%s' is missing MCS %d", calibrationSet.c_str(), mcs);
        if (it->second.dataCarrierCount != expectedDataCarriers)
            throw cRuntimeError("Ieee80211Eesm: calibration set '%s' MCS %d has the wrong data-carrier pairing", calibrationSet.c_str(), mcs);
    }
    if (betas.empty())
        throw cRuntimeError("Ieee80211Eesm: beta CSV '%s' has no rows for calibration set '%s'", filename.c_str(), calibrationSet.c_str());
}

void Ieee80211Eesm::requireSha256(const std::string& filename, const std::string& expectedSha256) const
{
    const std::string actual = Ieee80211PerTable::sha256File(filename);
    if (actual != expectedSha256)
        throw cRuntimeError("Ieee80211Eesm: SHA-256 mismatch for '%s': expected %s, got %s", filename.c_str(), expectedSha256.c_str(), actual.c_str());
}

double Ieee80211Eesm::getBeta(const std::string& calibrationSet, int bandwidthMHz, int mcs, int dataCarrierCount) const
{
    auto it = betas.find({calibrationSet, bandwidthMHz, mcs});
    if (it == betas.end())
        throw cRuntimeError("Ieee80211Eesm: missing beta key for calibration set '%s', %d MHz MCS %d", calibrationSet.c_str(), bandwidthMHz, mcs);
    if (it->second.dataCarrierCount != dataCarrierCount)
        throw cRuntimeError("Ieee80211Eesm: beta carrier-count mismatch: table has %d, runtime has %d", it->second.dataCarrierCount, dataCarrierCount);
    return it->second.value;
}

} // namespace physicallayer
} // namespace inet
