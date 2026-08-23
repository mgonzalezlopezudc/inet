//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211PerTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace inet {
namespace physicallayer {

namespace {

class Sha256 {
  private:
    uint32_t state[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint64_t bitLength = 0;
    std::array<uint8_t, 64> block{};
    size_t blockLength = 0;

    static uint32_t rotateRight(uint32_t value, unsigned int bits) { return (value >> bits) | (value << (32 - bits)); }
    static uint32_t choose(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t majority(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t bigSigma0(uint32_t x) { return rotateRight(x, 2) ^ rotateRight(x, 13) ^ rotateRight(x, 22); }
    static uint32_t bigSigma1(uint32_t x) { return rotateRight(x, 6) ^ rotateRight(x, 11) ^ rotateRight(x, 25); }
    static uint32_t smallSigma0(uint32_t x) { return rotateRight(x, 7) ^ rotateRight(x, 18) ^ (x >> 3); }
    static uint32_t smallSigma1(uint32_t x) { return rotateRight(x, 17) ^ rotateRight(x, 19) ^ (x >> 10); }

    void transform(const uint8_t *input) {
        static const uint32_t constants[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32_t words[64];
        for (int i = 0; i < 16; i++)
            words[i] = (uint32_t(input[4 * i]) << 24) | (uint32_t(input[4 * i + 1]) << 16) | (uint32_t(input[4 * i + 2]) << 8) | uint32_t(input[4 * i + 3]);
        for (int i = 16; i < 64; i++)
            words[i] = smallSigma1(words[i - 2]) + words[i - 7] + smallSigma0(words[i - 15]) + words[i - 16];
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + bigSigma1(e) + choose(e, f, g) + constants[i] + words[i];
            uint32_t t2 = bigSigma0(a) + majority(a, b, c);
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

  public:
    void update(const uint8_t *data, size_t length) {
        bitLength += uint64_t(length) * 8;
        while (length != 0) {
            size_t amount = std::min(length, block.size() - blockLength);
            std::copy(data, data + amount, block.begin() + blockLength);
            blockLength += amount;
            data += amount;
            length -= amount;
            if (blockLength == block.size()) {
                transform(block.data());
                blockLength = 0;
            }
        }
    }

    std::string finish() {
        block[blockLength++] = 0x80;
        if (blockLength > 56) {
            std::fill(block.begin() + blockLength, block.end(), 0);
            transform(block.data());
            blockLength = 0;
        }
        std::fill(block.begin() + blockLength, block.begin() + 56, 0);
        for (int i = 0; i < 8; i++)
            block[56 + i] = uint8_t(bitLength >> (56 - 8 * i));
        transform(block.data());
        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (uint32_t word : state)
            output << std::setw(8) << word;
        return output.str();
    }
};

static std::string invalid(const std::string& message) {
    throw cRuntimeError("Ieee80211PerTable: %s", message.c_str());
}

static int parseInteger(const std::string& text, const char *field) {
    size_t position = 0;
    int value;
    try { value = std::stoi(text, &position); }
    catch (...) { return invalid(std::string("invalid ") + field + " '" + text + "'"), 0; }
    if (position != text.size())
        return invalid(std::string("invalid ") + field + " '" + text + "'"), 0;
    return value;
}

static uint64_t parseUnsigned(const std::string& text, const char *field) {
    size_t position = 0;
    unsigned long long value;
    try { value = std::stoull(text, &position); }
    catch (...) { return invalid(std::string("invalid ") + field + " '" + text + "'"), 0; }
    if (position != text.size())
        return invalid(std::string("invalid ") + field + " '" + text + "'"), 0;
    return uint64_t(value);
}

static double parseDouble(const std::string& text, const char *field) {
    size_t position = 0;
    double value;
    try { value = std::stod(text, &position); }
    catch (...) { return invalid(std::string("invalid ") + field + " '" + text + "'"), 0; }
    if (position != text.size() || !std::isfinite(value))
        return invalid(std::string("invalid finite ") + field + " '" + text + "'"), 0;
    return value;
}

} // namespace

std::string Ieee80211PerTable::trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::vector<std::string> Ieee80211PerTable::split(const std::string& line)
{
    std::vector<std::string> result;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ','))
        result.push_back(trim(field));
    return result;
}

int Ieee80211PerTable::bandwidthToMHz(double bandwidthMHz)
{
    int result = int(std::llround(bandwidthMHz));
    if (std::abs(bandwidthMHz - result) > 1e-9 || (result != 20 && result != 40))
        throw cRuntimeError("Ieee80211PerTable: unsupported bandwidth %.17g MHz", bandwidthMHz);
    return result;
}

void Ieee80211PerTable::validatePointAxis(const std::vector<Point>& points, const Key& key)
{
    if (points.size() < 2)
        throw cRuntimeError("Ieee80211PerTable: table for %d MHz MCS %d PSDU length %llu must have explicit low/high endpoints", key.bandwidthMHz, key.mcs, (unsigned long long)key.referenceLength);
    double previousSnr = -std::numeric_limits<double>::infinity();
    double previousPer = 1.0;
    for (const auto& point : points) {
        if (!(point.snrDb > previousSnr) || point.per < 0 || point.per > 1 || point.per > previousPer + 1e-12)
            throw cRuntimeError("Ieee80211PerTable: non-monotone or invalid curve for %d MHz MCS %d reference length %llu", key.bandwidthMHz, key.mcs, (unsigned long long)key.referenceLength);
        previousSnr = point.snrDb;
        previousPer = point.per;
    }
}

void Ieee80211PerTable::loadCsv(const std::string& filename)
{
    std::ifstream input(filename);
    if (!input)
        throw cRuntimeError("Ieee80211PerTable: cannot open PER table '%s'", filename.c_str());
    clear();
    std::string line;
    bool sawHeader = false;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        auto fields = split(line);
        if (!sawHeader && !fields.empty() && fields[0] == "coding") {
            if (fields.size() != 6 || fields[1] != "bandwidth_mhz" || fields[2] != "mcs" || fields[3] != "psdu_bytes" || fields[4] != "snr_db" || fields[5] != "per")
                throw cRuntimeError("Ieee80211PerTable: invalid CSV header in '%s'", filename.c_str());
            sawHeader = true;
            continue;
        }
        if (fields.size() != 6)
            throw cRuntimeError("Ieee80211PerTable: expected six CSV fields in '%s'", filename.c_str());
        if (fields[0] != "BCC")
            throw cRuntimeError("Ieee80211PerTable: only BCC rows are supported, got '%s'", fields[0].c_str());
        int bandwidthMHz = bandwidthToMHz(parseDouble(fields[1], "bandwidth_mhz"));
        int mcs = parseInteger(fields[2], "mcs");
        if (mcs < 0 || mcs > 7)
            throw cRuntimeError("Ieee80211PerTable: unsupported MCS %d", mcs);
        uint64_t referenceLength = parseUnsigned(fields[3], "psdu_bytes");
        if (referenceLength != 32 && referenceLength != 1458)
            throw cRuntimeError("Ieee80211PerTable: unsupported psdu_bytes %llu; required values are 32 and 1458", (unsigned long long)referenceLength);
        Point point{parseDouble(fields[4], "snr_db"), parseDouble(fields[5], "per")};
        if (point.per < 0 || point.per > 1)
            throw cRuntimeError("Ieee80211PerTable: PER must be in [0,1]");
        tables[{bandwidthMHz, mcs, referenceLength}].push_back(point);
    }
    if (!sawHeader)
        throw cRuntimeError("Ieee80211PerTable: CSV '%s' is missing its schema header", filename.c_str());
    for (const auto& entry : tables)
        validatePointAxis(entry.second, entry.first);
    if (tables.empty())
        throw cRuntimeError("Ieee80211PerTable: CSV '%s' contains no rows", filename.c_str());
    for (int bandwidthMHz : {20, 40}) {
        for (int mcs = 0; mcs <= 7; mcs++) {
            if (!tables.count({bandwidthMHz, mcs, 32}) || !tables.count({bandwidthMHz, mcs, 1458}))
                throw cRuntimeError("Ieee80211PerTable: missing required 32-byte or 1458-byte reference curve for %d MHz MCS %d", bandwidthMHz, mcs);
        }
    }
}

void Ieee80211PerTable::requireSha256(const std::string& filename, const std::string& expectedSha256) const
{
    if (expectedSha256.size() != 64)
        throw cRuntimeError("Ieee80211PerTable: SHA-256 manifest value must contain 64 hex characters");
    const std::string actual = sha256File(filename);
    if (actual != expectedSha256)
        throw cRuntimeError("Ieee80211PerTable: SHA-256 mismatch for '%s': expected %s, got %s", filename.c_str(), expectedSha256.c_str(), actual.c_str());
}

double Ieee80211PerTable::getPacketErrorRate(int bandwidthMHz, int mcs, uint64_t psduLengthBytes, double snrDb) const
{
    if (mcs < 0 || mcs > 7 || (bandwidthMHz != 20 && bandwidthMHz != 40) || psduLengthBytes == 0 || std::isnan(snrDb) || snrDb == std::numeric_limits<double>::infinity())
        throw cRuntimeError("Ieee80211PerTable: unsupported PER lookup key");
    const uint64_t referenceLength = psduLengthBytes <= 400 ? 32 : 1458;
    auto selected = tables.find({bandwidthMHz, mcs, referenceLength});
    if (selected == tables.end())
        throw cRuntimeError("Ieee80211PerTable: missing table key %d MHz MCS %d reference length %llu", bandwidthMHz, mcs, (unsigned long long)referenceLength);
    const auto& points = selected->second;
    double referencePer;
    if (snrDb <= points.front().snrDb)
        referencePer = points.front().per;
    else if (snrDb >= points.back().snrDb)
        referencePer = points.back().per;
    else {
        auto upper = std::upper_bound(points.begin(), points.end(), snrDb, [](double value, const Point& point) { return value < point.snrDb; });
        auto lower = upper - 1;
        double fraction = (snrDb - lower->snrDb) / (upper->snrDb - lower->snrDb);
        if (lower->per <= 0 || lower->per >= 1 || upper->per <= 0 || upper->per >= 1)
            referencePer = lower->per + fraction * (upper->per - lower->per);
        else {
            // Interpolate complementary-log-log probability in SNR dB.  It
            // remains well behaved in the waterfall and composes with Eq. 1.
            const double lowerZ = std::log(-std::log1p(-lower->per));
            const double upperZ = std::log(-std::log1p(-upper->per));
            const double z = lowerZ + fraction * (upperZ - lowerZ);
            referencePer = -std::expm1(-std::exp(z));
        }
    }
    if (referencePer <= 0)
        return 0;
    if (referencePer >= 1)
        return 1;
    double scaled = -std::expm1((double(psduLengthBytes) / double(referenceLength)) * std::log1p(-referencePer));
    return std::clamp(scaled, 0.0, 1.0);
}

std::string Ieee80211PerTable::sha256File(const std::string& filename)
{
    std::ifstream input(filename, std::ios::binary);
    if (!input)
        throw cRuntimeError("Ieee80211PerTable: cannot open '%s' for SHA-256", filename.c_str());
    Sha256 sha;
    std::array<uint8_t, 8192> buffer;
    while (input) {
        input.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        const std::streamsize count = input.gcount();
        if (count > 0)
            sha.update(buffer.data(), size_t(count));
    }
    return sha.finish();
}

} // namespace physicallayer
} // namespace inet
