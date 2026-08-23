//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211LdpcPerTable.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace physicallayer {

namespace {

constexpr const char *CSV_HEADER =
        "phy_format,bandwidth_mhz,mcs,nss,number_of_codewords,ldpc_codeword_length,shortened_bits,punctured_bits,repeated_bits,snr_db,per";

std::string trim(const std::string& value)
{
    auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> splitCsv(const std::string& line, const std::string& source, int lineNumber)
{
    if (line.find('"') != std::string::npos)
        throw cRuntimeError("%s:%d: quoted CSV fields are not supported", source.c_str(), lineNumber);
    std::vector<std::string> fields;
    size_t begin = 0;
    while (true) {
        size_t comma = line.find(',', begin);
        fields.push_back(trim(line.substr(begin, comma == std::string::npos ? comma : comma - begin)));
        if (comma == std::string::npos)
            break;
        begin = comma + 1;
    }
    if (fields.size() != 11)
        throw cRuntimeError("%s:%d: expected 11 CSV fields, found %zu", source.c_str(), lineNumber, fields.size());
    for (size_t i = 0; i < fields.size(); i++)
        if (fields[i].empty())
            throw cRuntimeError("%s:%d:%zu: empty CSV field", source.c_str(), lineNumber, i + 1);
    return fields;
}

int parseInt(const std::string& text, const std::string& source, int lineNumber, int column)
{
    int value;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size())
        throw cRuntimeError("%s:%d:%d: invalid integer '%s'", source.c_str(), lineNumber, column, text.c_str());
    return value;
}

double parseFiniteDouble(const std::string& text, const std::string& source, int lineNumber, int column)
{
    errno = 0;
    char *end = nullptr;
    double value = std::strtod(text.c_str(), &end);
    if (errno == ERANGE || end != text.c_str() + text.size() || !std::isfinite(value))
        throw cRuntimeError("%s:%d:%d: invalid finite number '%s'", source.c_str(), lineNumber, column, text.c_str());
    return value;
}

Ieee80211PhyFormat parseFormat(const std::string& text, const std::string& source, int lineNumber)
{
    if (text == "HT")
        return Ieee80211PhyFormat::HT;
    if (text == "VHT_SU")
        return Ieee80211PhyFormat::VHT_SU;
    throw cRuntimeError("%s:%d:1: unknown PHY format '%s'", source.c_str(), lineNumber, text.c_str());
}

bool sameAggregatePlan(const Ieee80211DataEncodingPlan& plan, const Ieee80211LdpcPerCurveKey& key)
{
    if (plan.getPhyFormat() != key.phyFormat || plan.getNumberOfCodewords() != key.numberOfCodewords ||
        plan.getShortenedBits() != key.shortenedBits || plan.getPuncturedBits() != key.puncturedBits ||
        plan.getRepeatedBits() != key.repeatedBits)
        return false;
    for (const auto& codeword : plan.getCodewords())
        if (codeword.getCodewordLength() != key.ldpcCodewordLength)
            return false;
    return true;
}

std::string keyString(const Ieee80211LdpcPerCurveKey& key)
{
    std::ostringstream stream;
    stream << (key.phyFormat == Ieee80211PhyFormat::HT ? "HT" : "VHT_SU")
           << ',' << key.bandwidthMhz << ',' << key.mcs << ',' << key.numberOfSpatialStreams
           << ',' << key.numberOfCodewords << ',' << key.ldpcCodewordLength
           << ',' << key.shortenedBits << ',' << key.puncturedBits << ',' << key.repeatedBits;
    return stream.str();
}

using ModeKey = std::tuple<Ieee80211PhyFormat, int, int>;

ModeKey makeModeKey(const Ieee80211LdpcPerCurveKey& key)
{
    int perStreamMcs = key.phyFormat == Ieee80211PhyFormat::HT ? key.mcs % 8 : key.mcs;
    return std::make_tuple(key.phyFormat, key.bandwidthMhz, perStreamMcs);
}

std::string modeKeyString(const ModeKey& key)
{
    std::ostringstream stream;
    stream << (std::get<0>(key) == Ieee80211PhyFormat::HT ? "HT" : "VHT_SU")
           << ',' << std::get<1>(key) << ',' << std::get<2>(key);
    return stream.str();
}

} // namespace

void Ieee80211LdpcPerTable::validateKey(const Ieee80211LdpcPerCurveKey& key, const std::string& source, int lineNumber)
{
    bool isHt = key.phyFormat == Ieee80211PhyFormat::HT;
    if (key.mcs < 0 || key.bandwidthMhz <= 0 || key.numberOfSpatialStreams < 1)
        throw cRuntimeError("%s:%d: invalid PHY mode dimensions", source.c_str(), lineNumber);
    const auto *mode = Ieee80211ModeSet::findMode(key.phyFormat, key.mcs,
            MHz(key.bandwidthMhz), key.numberOfSpatialStreams, Ieee80211FecType::LDPC);
    if (mode == nullptr)
        throw cRuntimeError("%s:%d: no canonical IEEE 802.11 mode matches the PHY format/MCS/bandwidth/NSS tuple",
                source.c_str(), lineNumber);
    const auto *dataMode = mode->getDataMode();
    if (key.numberOfCodewords < 1 || (key.ldpcCodewordLength != 648 && key.ldpcCodewordLength != 1296 && key.ldpcCodewordLength != 1944))
        throw cRuntimeError("%s:%d: invalid LDPC codeword dimensions", source.c_str(), lineNumber);

    auto rate = dataMode->getCodeRate();
    int informationLength = rate.multiplyExact(key.ldpcCodewordLength);
    int64_t informationCapacity = int64_t(key.numberOfCodewords) * informationLength;
    int64_t parityCapacity = int64_t(key.numberOfCodewords) * (key.ldpcCodewordLength - informationLength);
    if (key.shortenedBits < 0 || key.shortenedBits >= informationCapacity ||
        key.puncturedBits < 0 || key.puncturedBits > parityCapacity || key.repeatedBits < 0 ||
        (key.puncturedBits != 0 && key.repeatedBits != 0))
        throw cRuntimeError("%s:%d: invalid shortening/puncturing/repetition signature", source.c_str(), lineNumber);

    int ncbps = dataMode->getNumberOfCodedBitsPerSymbol();
    int ndbps = dataMode->getNumberOfDataBitsPerSymbol();
    int64_t npld64 = informationCapacity - key.shortenedBits;
    int64_t navbits64 = int64_t(key.numberOfCodewords) * key.ldpcCodewordLength -
                        key.shortenedBits - key.puncturedBits + key.repeatedBits;
    if (npld64 > std::numeric_limits<int>::max() || navbits64 <= 0 || navbits64 > std::numeric_limits<int>::max() || navbits64 % ncbps != 0)
        throw cRuntimeError("%s:%d: impossible LDPC planner signature", source.c_str(), lineNumber);

    Ieee80211DataEncodingPlan plan = [&]() {
        int npld = static_cast<int>(npld64);
        if (isHt) {
            if (npld < 16 || (npld - 16) % 8 != 0)
                throw cRuntimeError("%s:%d: HT Npld is not a whole PSDU", source.c_str(), lineNumber);
            return Ieee80211LdpcPlanner::computeHt((npld - 16) / 8, ncbps, rate);
        }
        if (npld % ndbps != 0)
            throw cRuntimeError("%s:%d: VHT Npld is not a whole number of symbols", source.c_str(), lineNumber);
        int nsym = npld / ndbps;
        int64_t lowerExclusive = int64_t(nsym - 1) * ndbps;
        int64_t minimumPayloadBits = std::max<int64_t>(0, lowerExclusive - 16 + 1);
        int apepOctets = static_cast<int>((minimumPayloadBits + 7) / 8);
        if (int64_t(apepOctets) * 8 + 16 > int64_t(nsym) * ndbps)
            throw cRuntimeError("%s:%d: VHT Npld cannot be produced by an APEP length", source.c_str(), lineNumber);
        return Ieee80211LdpcPlanner::computeVhtSu(apepOctets, ncbps, ndbps, rate);
    }();
    if (!sameAggregatePlan(plan, key))
        throw cRuntimeError("%s:%d: curve key does not match the canonical IEEE 802.11 LDPC planner", source.c_str(), lineNumber);
}

void Ieee80211LdpcPerTable::load(const char *tableFile)
{
    std::ifstream input(tableFile);
    if (!input)
        throw cRuntimeError("Cannot open IEEE 802.11 LDPC PER table '%s'", tableFile);
    load(input, tableFile);
}

void Ieee80211LdpcPerTable::load(std::istream& input, const char *sourceName)
{
    std::string source = sourceName == nullptr ? "<stream>" : sourceName;
    std::map<Ieee80211LdpcPerCurveKey, Curve> loaded;
    std::map<ModeKey, Ieee80211LdpcPerCurveKey> loadedModeKeys;
    std::string line;
    int lineNumber = 0;
    bool sawHeader = false;
    bool havePreviousKey = false;
    Ieee80211LdpcPerCurveKey previousKey{};
    while (std::getline(input, line)) {
        lineNumber++;
        std::string value = trim(line);
        if (value.empty() || value[0] == '#')
            continue;
        if (!sawHeader) {
            if (value != CSV_HEADER)
                throw cRuntimeError("%s:%d: expected exact CSV header '%s'", source.c_str(), lineNumber, CSV_HEADER);
            sawHeader = true;
            continue;
        }

        auto fields = splitCsv(value, source, lineNumber);
        Ieee80211LdpcPerCurveKey key{
            parseFormat(fields[0], source, lineNumber),
            parseInt(fields[1], source, lineNumber, 2),
            parseInt(fields[2], source, lineNumber, 3),
            parseInt(fields[3], source, lineNumber, 4),
            parseInt(fields[4], source, lineNumber, 5),
            parseInt(fields[5], source, lineNumber, 6),
            parseInt(fields[6], source, lineNumber, 7),
            parseInt(fields[7], source, lineNumber, 8),
            parseInt(fields[8], source, lineNumber, 9)};
        double snrDb = parseFiniteDouble(fields[9], source, lineNumber, 10);
        double packetErrorRate = parseFiniteDouble(fields[10], source, lineNumber, 11);
        validateKey(key, source, lineNumber);
        if (!(packetErrorRate > 0 && packetErrorRate <= 1))
            throw cRuntimeError("%s:%d:11: PER must satisfy 0 < PER <= 1", source.c_str(), lineNumber);
        if (havePreviousKey && key < previousKey)
            throw cRuntimeError("%s:%d: curve keys must be in canonical order and contiguous", source.c_str(), lineNumber);
        auto modeKey = makeModeKey(key);
        auto modeKeyIterator = loadedModeKeys.find(modeKey);
        if (modeKeyIterator != loadedModeKeys.end() && !(modeKeyIterator->second == key))
            throw cRuntimeError("%s:%d: multiple LDPC calibration signatures for key %s; one curve is required per format/bandwidth/per-stream-MCS",
                    source.c_str(), lineNumber, modeKeyString(modeKey).c_str());
        loadedModeKeys.emplace(modeKey, key);
        auto& curve = loaded[key];
        if (!curve.empty()) {
            if (!(snrDb > curve.back().snrDb))
                throw cRuntimeError("%s:%d: SNR values must be strictly increasing within a curve", source.c_str(), lineNumber);
            if (packetErrorRate > curve.back().packetErrorRate)
                throw cRuntimeError("%s:%d: PER values must be non-increasing within a curve", source.c_str(), lineNumber);
        }
        curve.push_back({snrDb, packetErrorRate});
        previousKey = key;
        havePreviousKey = true;
    }
    if (!sawHeader)
        throw cRuntimeError("%s: missing CSV header", source.c_str());
    if (loaded.empty())
        throw cRuntimeError("%s: LDPC PER table contains no curves", source.c_str());
    for (const auto& entry : loaded)
        if (entry.second.size() < 2)
            throw cRuntimeError("%s: curve %s contains fewer than two points", source.c_str(), keyString(entry.first).c_str());
    curves = std::move(loaded);
    modeCurveKeys = std::move(loadedModeKeys);
}

const Ieee80211LdpcPerTable::Curve& Ieee80211LdpcPerTable::getCurve(const Ieee80211LdpcPerCurveKey& key) const
{
    // Keep lookup-side structural validation strict even though the
    // calibrated curve is selected by format/bandwidth/per-stream-MCS.
    validateKey(key, "<lookup>", 0);
    auto modeKey = makeModeKey(key);
    auto modeIterator = modeCurveKeys.find(modeKey);
    if (modeIterator == modeCurveKeys.end())
        throw cRuntimeError("Missing IEEE 802.11 LDPC PER curve for calibrated mode key %s", modeKeyString(modeKey).c_str());
    auto iterator = curves.find(modeIterator->second);
    if (iterator == curves.end())
        throw cRuntimeError("IEEE 802.11 LDPC PER table lost calibration curve for key %s", keyString(modeIterator->second).c_str());
    // Modeling assumption: for a calibrated format/bandwidth/per-stream-MCS
    // tuple, PER versus SNR is reused unchanged for every LDPC payload/codeword
    // size contemplated by the standard and for NSS>1 ideal separated streams.
    // HT MCS 8..31 map to their MCS modulo 8 calibration. Structural CSV
    // columns remain strict provenance validation, not lookup keys.
    return iterator->second;
}

double Ieee80211LdpcPerTable::getPacketErrorRate(const Ieee80211LdpcPerCurveKey& key, double snrDb) const
{
    if (std::isnan(snrDb))
        throw cRuntimeError("IEEE 802.11 LDPC PER lookup SNR must not be NaN");
    const auto& curve = getCurve(key);
    if (snrDb <= curve.front().snrDb)
        return curve.front().packetErrorRate;
    if (snrDb >= curve.back().snrDb)
        return curve.back().packetErrorRate;
    auto upper = std::upper_bound(curve.begin(), curve.end(), snrDb,
        [](double value, const Ieee80211LdpcPerPoint& point) { return value < point.snrDb; });
    auto lower = upper - 1;
    if (snrDb == lower->snrDb)
        return lower->packetErrorRate;
    if (snrDb == upper->snrDb)
        return upper->packetErrorRate;
    double fraction = (snrDb - lower->snrDb) / (upper->snrDb - lower->snrDb);
    double logPer = std::log10(lower->packetErrorRate) +
                    fraction * (std::log10(upper->packetErrorRate) - std::log10(lower->packetErrorRate));
    return std::pow(10.0, logPer);
}

Ieee80211LdpcPerCurveKey makeIeee80211LdpcPerCurveKey(
        const IIeee80211DataMode& mode, const Ieee80211DataEncodingPlan& plan)
{
    if (mode.getFecType() != Ieee80211FecType::LDPC || plan.getFecType() != Ieee80211FecType::LDPC)
        throw cRuntimeError("An LDPC PER curve key requires an LDPC mode and plan");
    if (plan.getCodewords().empty())
        throw cRuntimeError("An LDPC PER curve key requires at least one codeword");
    int codewordLength = plan.getCodewords().front().getCodewordLength();
    for (const auto& codeword : plan.getCodewords())
        if (codeword.getCodewordLength() != codewordLength)
            throw cRuntimeError("An LDPC PER curve key requires a common codeword length");

    int mcs = mode.getMcsIndex();
    Ieee80211PhyFormat format = mode.getPhyFormat();
    if (format != Ieee80211PhyFormat::HT && format != Ieee80211PhyFormat::VHT_SU)
        throw cRuntimeError("LDPC PER curves support only HT and VHT-SU data modes");
    if (plan.getPhyFormat() != format)
        throw cRuntimeError("IEEE 802.11 LDPC mode and encoding plan have different PHY formats");
    int numberOfCodedBitsPerSymbol = mode.getNumberOfCodedBitsPerSymbol();
    if (plan.getNumberOfCodedBitsPerSymbol() != numberOfCodedBitsPerSymbol ||
        int64_t(plan.getNumberOfSymbols()) * numberOfCodedBitsPerSymbol != plan.getAvailableEncodedBits())
        throw cRuntimeError("IEEE 802.11 LDPC encoding plan is inconsistent with the resolved mode's coded-bit geometry");
    auto codeRate = mode.getCodeRate();
    int64_t dataBits = 0;
    int64_t transmittedBits = 0;
    for (const auto& codeword : plan.getCodewords()) {
        if (codeword.getInformationLength() != codeRate.multiplyExact(codeword.getCodewordLength()))
            throw cRuntimeError("IEEE 802.11 LDPC encoding plan is inconsistent with the resolved mode's code rate");
        dataBits += codeword.getDataBits();
        transmittedBits += codeword.getTransmittedBits();
    }
    if (dataBits != plan.getUncodedDataBits() || transmittedBits != plan.getAvailableEncodedBits())
        throw cRuntimeError("IEEE 802.11 LDPC encoding plan has inconsistent aggregate bit counts");
    int bandwidthMhz = static_cast<int>(std::llround(mode.getBandwidth().get() / 1e6));
    if (std::fabs(mode.getBandwidth().get() - bandwidthMhz * 1e6) > 0.5)
        throw cRuntimeError("IEEE 802.11 LDPC bandwidth is not an integer number of MHz");
    Ieee80211LdpcPerCurveKey key{format, bandwidthMhz, mcs, mode.getNumberOfSpatialStreams(),
        plan.getNumberOfCodewords(), codewordLength, plan.getShortenedBits(),
        plan.getPuncturedBits(), plan.getRepeatedBits()};
    return key;
}

} // namespace physicallayer
} // namespace inet
