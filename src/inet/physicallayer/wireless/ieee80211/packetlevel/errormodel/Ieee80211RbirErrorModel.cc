//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211RbirErrorModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalSnir.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

namespace inet {
namespace physicallayer {

using Calibration = Ieee80211RbirCalibration;

Define_Module(Ieee80211RbirErrorModel);

void Calibration::clear()
{
    rbirCurves.clear();
    perCurves.clear();
}

Calibration::Modulation Calibration::parseModulation(const std::string& text)
{
    if (text == "BPSK") return Modulation::BPSK;
    if (text == "QPSK") return Modulation::QPSK;
    if (text == "QAM16") return Modulation::QAM16;
    if (text == "QAM64") return Modulation::QAM64;
    if (text == "QAM256") return Modulation::QAM256;
    throw cRuntimeError("Unknown RBIR modulation '%s'", text.c_str());
}

Calibration::Coding Calibration::parseCoding(const std::string& text)
{
    if (text == "BCC") return Coding::BCC;
    if (text == "LDPC") return Coding::LDPC;
    throw cRuntimeError("Unknown RBIR coding '%s'", text.c_str());
}

void Calibration::addRbirPoint(Modulation modulation, double snrDb, double rbir)
{
    double maximumRbir;
    switch (modulation) {
        case Modulation::BPSK: maximumRbir = 1; break;
        case Modulation::QPSK: maximumRbir = 2; break;
        case Modulation::QAM16: maximumRbir = 4; break;
        case Modulation::QAM64: maximumRbir = 6; break;
        case Modulation::QAM256: maximumRbir = 8; break;
        default: throw cRuntimeError("Unknown RBIR modulation");
    }
    if (!std::isfinite(snrDb) || !std::isfinite(rbir) || rbir < 0 || rbir > maximumRbir)
        throw cRuntimeError("Invalid RBIR point (SNR=%g dB, RBIR=%g)", snrDb, rbir);
    auto& curve = rbirCurves[modulation];
    curve.arguments.push_back(snrDb);
    curve.values.push_back(rbir);
    curve.monotonicValues.clear();
}

void Calibration::addPerPoint(Coding coding, int referencePacketLength, int mcs,
        double snrDb, double per)
{
    if ((coding == Coding::BCC && referencePacketLength != 32 && referencePacketLength != 1458) ||
            (coding == Coding::LDPC && referencePacketLength != 1458))
        throw cRuntimeError("Invalid reference packet length %d for RBIR calibration", referencePacketLength);
    if (mcs < 0 || mcs > 9)
        throw cRuntimeError("RBIR calibration only supports HE MCS 0-9, got %d", mcs);
    if (!std::isfinite(snrDb) || !std::isfinite(per) || per < 0 || per > 1)
        throw cRuntimeError("Invalid PER point (SNR=%g dB, PER=%g)", snrDb, per);
    auto& curve = perCurves[std::make_tuple(coding, referencePacketLength, mcs)];
    curve.arguments.push_back(snrDb);
    curve.values.push_back(per);
}

void Calibration::validateCurve(const Curve& curve, const char *name, double expectedStep)
{
    if (curve.arguments.size() < 2 || curve.arguments.size() != curve.values.size())
        throw cRuntimeError("RBIR calibration curve '%s' must contain at least two points", name);
    for (size_t i = 1; i < curve.arguments.size(); ++i) {
        double step = curve.arguments[i] - curve.arguments[i - 1];
        if (!(step > 0))
            throw cRuntimeError("RBIR calibration curve '%s' contains duplicate or unordered SNR values", name);
        if (!std::isnan(expectedStep) && std::abs(step - expectedStep) > 1e-8)
            throw cRuntimeError("RBIR calibration curve '%s' has SNR step %g dB instead of %g dB",
                    name, step, expectedStep);
    }
}

void Calibration::sortAndValidateCurves()
{
    auto sortCurve = [] (Curve& curve) {
        std::vector<std::pair<double, double>> points;
        points.reserve(curve.arguments.size());
        for (size_t i = 0; i < curve.arguments.size(); ++i)
            points.emplace_back(curve.arguments[i], curve.values[i]);
        std::sort(points.begin(), points.end());
        for (size_t i = 0; i < points.size(); ++i) {
            curve.arguments[i] = points[i].first;
            curve.values[i] = points[i].second;
        }
        curve.monotonicValues.clear();
    };
    for (auto& entry : rbirCurves) {
        sortCurve(entry.second);
        validateCurve(entry.second, "RBIR");
    }
    for (auto& entry : perCurves) {
        sortCurve(entry.second);
        validateCurve(entry.second, "PER");
    }
}

void Calibration::load(const char *fileName, bool requireTgaxCompleteness)
{
    clear();
    std::ifstream input(fileName);
    if (!input)
        throw cRuntimeError("Cannot open RBIR calibration file '%s'", fileName);
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        lineNumber++;
        auto comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);
        std::istringstream stream(line);
        std::string recordType;
        if (!(stream >> recordType))
            continue;
        try {
            if (recordType == "RBIR") {
                std::string modulation;
                double snrDb, rbir;
                if (!(stream >> modulation >> snrDb >> rbir))
                    throw cRuntimeError("Malformed RBIR record");
                addRbirPoint(parseModulation(modulation), snrDb, rbir);
            }
            else if (recordType == "PER") {
                std::string coding;
                int referencePacketLength, mcs;
                double snrDb, per;
                if (!(stream >> coding >> referencePacketLength >> mcs >> snrDb >> per))
                    throw cRuntimeError("Malformed PER record");
                addPerPoint(parseCoding(coding), referencePacketLength, mcs, snrDb, per);
            }
            else
                throw cRuntimeError("Unknown record type '%s'", recordType.c_str());
            std::string trailing;
            if (stream >> trailing)
                throw cRuntimeError("Unexpected trailing field '%s'", trailing.c_str());
        }
        catch (const cRuntimeError& error) {
            throw cRuntimeError("Invalid RBIR calibration at %s:%d: %s", fileName, lineNumber, error.what());
        }
    }
    sortAndValidateCurves();
    if (requireTgaxCompleteness)
        validateTgaxCompleteness();
}

void Calibration::validateTgaxCompleteness() const
{
    for (auto modulation : {Modulation::BPSK, Modulation::QPSK, Modulation::QAM16,
            Modulation::QAM64, Modulation::QAM256}) {
        auto it = rbirCurves.find(modulation);
        if (it == rbirCurves.end())
            throw cRuntimeError("RBIR calibration is missing a required modulation curve");
        const auto& curve = it->second;
        validateCurve(curve, "RBIR", 0.02);
        if (std::abs(curve.arguments.front() + 20) > 1e-8 ||
                std::abs(curve.arguments.back() - 32) > 1e-8)
            throw cRuntimeError("RBIR calibration curves must cover -20 through 32 dB");
    }
    struct PerMetadata {
        Coding coding;
        int packetLength;
        int mcs;
        size_t count;
        double start;
        double end;
    };
    static const PerMetadata metadata[] = {
        {Coding::BCC, 32, 0, 71, -5, 2}, {Coding::BCC, 32, 1, 71, -2, 5},
        {Coding::BCC, 32, 2, 66, 1, 7.5}, {Coding::BCC, 32, 3, 79, 3, 10.8},
        {Coding::BCC, 32, 4, 81, 6, 14}, {Coding::BCC, 32, 5, 81, 10, 18},
        {Coding::BCC, 32, 6, 81, 11, 19}, {Coding::BCC, 32, 7, 71, 13, 20},
        {Coding::BCC, 32, 8, 86, 16, 24.5}, {Coding::BCC, 32, 9, 82, 18, 26.1},
        {Coding::BCC, 1458, 0, 46, -2, 2.5}, {Coding::BCC, 1458, 1, 46, 1, 5.5},
        {Coding::BCC, 1458, 2, 45, 4, 8.4}, {Coding::BCC, 1458, 3, 52, 6, 11.1},
        {Coding::BCC, 1458, 4, 48, 10, 14.7}, {Coding::BCC, 1458, 5, 63, 13, 19.2},
        {Coding::BCC, 1458, 6, 57, 15, 20.6}, {Coding::BCC, 1458, 7, 58, 16, 21.7},
        {Coding::BCC, 1458, 8, 74, 19, 26.3}, {Coding::BCC, 1458, 9, 65, 21, 27.4},
        {Coding::LDPC, 1458, 0, 21, -2, 0}, {Coding::LDPC, 1458, 1, 21, 1, 3},
        {Coding::LDPC, 1458, 2, 21, 3.6, 5.6}, {Coding::LDPC, 1458, 3, 26, 6, 8.5},
        {Coding::LDPC, 1458, 4, 28, 9, 11.7}, {Coding::LDPC, 1458, 5, 29, 13, 15.8},
        {Coding::LDPC, 1458, 6, 31, 14, 17}, {Coding::LDPC, 1458, 7, 31, 16, 19},
        {Coding::LDPC, 1458, 8, 24, 20.3, 22.6}, {Coding::LDPC, 1458, 9, 25, 22, 24.4},
    };
    for (const auto& expected : metadata) {
        auto it = perCurves.find(std::make_tuple(expected.coding, expected.packetLength, expected.mcs));
        if (it == perCurves.end())
            throw cRuntimeError("RBIR calibration is missing a required PER curve for MCS %d", expected.mcs);
        const auto& curve = it->second;
        validateCurve(curve, "PER", 0.1);
        if (curve.arguments.size() != expected.count ||
                std::abs(curve.arguments.front() - expected.start) > 1e-8 ||
                std::abs(curve.arguments.back() - expected.end) > 1e-8)
            throw cRuntimeError("RBIR PER calibration has wrong extent for MCS %d, packet length %d",
                    expected.mcs, expected.packetLength);
    }
}

double Calibration::interpolate(const Curve& curve, double argument)
{
    return interpolate(curve.arguments, curve.values, argument);
}

double Calibration::interpolate(const std::vector<double>& arguments,
        const std::vector<double>& values, double argument)
{
    if (argument <= arguments.front())
        return values.front();
    if (argument >= arguments.back())
        return values.back();
    auto upper = std::upper_bound(arguments.begin(), arguments.end(), argument);
    size_t i = upper - arguments.begin();
    double ratio = (argument - arguments[i - 1]) /
            (arguments[i] - arguments[i - 1]);
    return values[i - 1] + ratio * (values[i] - values[i - 1]);
}

double Calibration::mapSnirToRbir(Modulation modulation, double snrDb) const
{
    auto it = rbirCurves.find(modulation);
    if (it == rbirCurves.end())
        throw cRuntimeError("Missing RBIR modulation calibration");
    const auto& curve = it->second;
    return interpolate(curve.arguments, getMonotonicValues(curve), snrDb);
}

double Calibration::mapRbirToSnir(Modulation modulation, double rbir) const
{
    auto it = rbirCurves.find(modulation);
    if (it == rbirCurves.end())
        throw cRuntimeError("Missing RBIR modulation calibration");
    const auto& curve = it->second;
    const auto& values = getMonotonicValues(curve);
    if (rbir <= values.front())
        return curve.arguments.front();
    if (rbir >= values.back())
        return curve.arguments.back();
    for (size_t i = 1; i < values.size(); ++i) {
        double lower = values[i - 1];
        double upper = values[i];
        if (lower <= rbir && rbir <= upper) {
            if (upper == lower)
                return curve.arguments[i - 1];
            double ratio = (rbir - lower) / (upper - lower);
            return curve.arguments[i - 1] + ratio *
                    (curve.arguments[i] - curve.arguments[i - 1]);
        }
    }
    throw cRuntimeError("RBIR value %g cannot be inverse-mapped", rbir);
}

const std::vector<double>& Calibration::getMonotonicValues(const Curve& curve)
{
    if (!curve.monotonicValues.empty())
        return curve.monotonicValues;
    struct Block {
        size_t begin;
        size_t end;
        double mean;
        size_t weight;
    };
    std::vector<Block> blocks;
    blocks.reserve(curve.values.size());
    for (size_t i = 0; i < curve.values.size(); ++i) {
        blocks.push_back({i, i + 1, curve.values[i], 1});
        while (blocks.size() >= 2 && blocks[blocks.size() - 2].mean > blocks.back().mean) {
            auto right = blocks.back();
            blocks.pop_back();
            auto& left = blocks.back();
            left.mean = (left.mean * left.weight + right.mean * right.weight) /
                    (left.weight + right.weight);
            left.end = right.end;
            left.weight += right.weight;
        }
    }
    curve.monotonicValues.resize(curve.values.size());
    for (const auto& block : blocks)
        std::fill(curve.monotonicValues.begin() + block.begin,
                curve.monotonicValues.begin() + block.end, block.mean);
    return curve.monotonicValues;
}

double Calibration::computeEffectiveSnirDb(Modulation modulation,
        const std::vector<double>& snirDb) const
{
    if (snirDb.empty())
        throw cRuntimeError("Cannot compute effective SNR from an empty tone set");
    if (std::all_of(snirDb.begin() + 1, snirDb.end(),
            [&] (double value) { return value == snirDb.front(); })) {
        auto it = rbirCurves.find(modulation);
        if (it == rbirCurves.end())
            throw cRuntimeError("Missing RBIR modulation calibration");
        return std::clamp(snirDb.front(), it->second.arguments.front(), it->second.arguments.back());
    }
    double sum = 0;
    for (double value : snirDb)
        sum += mapSnirToRbir(modulation, value);
    return mapRbirToSnir(modulation, sum / snirDb.size());
}

double Calibration::lookupPer(Coding coding, int mcs, int packetLength,
        double effectiveSnirDb) const
{
    if (packetLength <= 0)
        throw cRuntimeError("Packet length must be positive for RBIR evaluation");
    int referencePacketLength = coding == Coding::BCC && packetLength < 400 ? 32 : 1458;
    auto it = perCurves.find(std::make_tuple(coding, referencePacketLength, mcs));
    if (it == perCurves.end())
        throw cRuntimeError("Missing RBIR PER calibration for MCS %d and packet length %d",
                mcs, referencePacketLength);
    double referencePer = interpolate(it->second, effectiveSnirDb);
    return 1 - std::pow(1 - referencePer,
            static_cast<double>(packetLength) / referencePacketLength);
}

void Ieee80211RbirErrorModel::initialize(int stage)
{
    Ieee80211NistErrorModel::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        std::string fileName = getEnvir()->getConfig()->substituteVariables(par("calibrationFile"));
        if (fileName.empty())
            throw cRuntimeError("The calibrationFile parameter is mandatory for Ieee80211RbirErrorModel");
        calibration.load(fileName.c_str(), true);
    }
}

std::vector<double> Ieee80211RbirErrorModel::sampleHeSnirDb(const ISnir *snir,
        int toneSize, int toneOffset, double scalarSnir) const
{
    auto dimensionalSnir = dynamic_cast<const DimensionalSnir *>(snir);
    if (dimensionalSnir == nullptr)
        throw cRuntimeError("Ieee80211RbirErrorModel requires dimensional SNIR");
    auto reception = snir->getReception();
    check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
    auto transmission = check_and_cast<const Ieee80211Transmission *>(reception->getTransmission());
    auto channel = transmission->getChannel();
    if (channel == nullptr || channel->getBand() == nullptr)
        throw cRuntimeError("Ieee80211RbirErrorModel requires an IEEE 802.11 transmission channel");
    int channelTones;
    try {
        channelTones = getHeChannelToneCount(channel->getBand()->getSpacing());
    }
    catch (const std::invalid_argument& error) {
        throw cRuntimeError("Invalid HE channel bandwidth for RBIR evaluation: %s", error.what());
    }
    std::vector<int> dataTones;
    try {
        dataTones = getHeRuDataToneIndices(channelTones, toneSize, toneOffset);
    }
    catch (const std::exception& error) {
        throw cRuntimeError("Invalid HE RU for RBIR evaluation: %s", error.what());
    }
    constexpr double toneSpacing = 78125;
    auto snirFunction = dimensionalSnir->getSnir();
    double unadjustedScalarSnir = getScalarSnir(snir);
    double adjustment = snirOffset *
            (unadjustedScalarSnir == 0 ? 1 : scalarSnir / unadjustedScalarSnir);
    auto startTime = simsec(reception->getDataStartTime());
    auto endTime = simsec(reception->getDataEndTime());
    auto centerFrequency = channel->getCenterFrequency();
    std::vector<double> result;
    result.reserve(dataTones.size());
    for (int tone : dataTones) {
        auto toneCenter = centerFrequency + Hz(tone * toneSpacing);
        Point<simsec, Hz> lower(startTime, toneCenter);
        Point<simsec, Hz> upper(endTime, toneCenter);
        // Frequency is a fixed dimension: stationarity is checked only over
        // time at the coded-data tone center. A nonzero frequency interval
        // would straddle adjacent channel-model cells when the tone lies on a
        // frequency-grid boundary.
        Interval<simsec, Hz> interval(lower, upper, 0b11, 0b01, 0b01);
        double minimum = snirFunction->getMin(interval);
        double maximum = snirFunction->getMax(interval);
        validateStationaryToneSnir(minimum, maximum);
        auto middleTime = simsec((reception->getDataStartTime() + reception->getDataEndTime()) / 2);
        double toneSnir = snirFunction->getValue(Point<simsec, Hz>(middleTime, toneCenter)) * adjustment;
        result.push_back(toneSnir > 0 ? 10 * std::log10(toneSnir) : -INFINITY);
    }
    return result;
}

void Ieee80211RbirErrorModel::validateStationaryToneSnir(double minimum, double maximum) const
{
    if (std::abs(maximum - minimum) > 1e-9 * std::max({1.0, std::abs(minimum), std::abs(maximum)}))
        throw cRuntimeError("Ieee80211RbirErrorModel does not support time-varying per-tone SNIR");
}

double Ieee80211RbirErrorModel::computeHeSuccessRate(int mcs,
        Calibration::Coding coding, int numberOfSpatialStreams, bool dcm,
        int packetLength, int toneSize, int toneOffset,
        const ISnir *snir, double scalarSnir) const
{
    if (mcs < 0 || mcs > 9)
        throw cRuntimeError("RBIR calibration supports HE MCS 0-9 only, got MCS %d", mcs);
    if (numberOfSpatialStreams != 1)
        throw cRuntimeError("Ieee80211RbirErrorModel supports SISO HE receptions only");
    if (dcm)
        throw cRuntimeError("Ieee80211RbirErrorModel has no DCM calibration");
    Calibration::Modulation modulation;
    if (mcs == 0) modulation = Calibration::Modulation::BPSK;
    else if (mcs <= 2) modulation = Calibration::Modulation::QPSK;
    else if (mcs <= 4) modulation = Calibration::Modulation::QAM16;
    else if (mcs <= 7) modulation = Calibration::Modulation::QAM64;
    else modulation = Calibration::Modulation::QAM256;
    auto toneSnirDb = sampleHeSnirDb(snir, toneSize, toneOffset, scalarSnir);
    double effectiveSnirDb = calibration.computeEffectiveSnirDb(modulation, toneSnirDb);
    return 1 - calibration.lookupPer(coding, mcs, packetLength, effectiveSnirDb);
}

double Ieee80211RbirErrorModel::getDataSuccessRate(const IIeee80211Mode *mode,
        unsigned int bitLength, const ISnir *snir, double scalarSnir) const
{
    auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode);
    if (heMode == nullptr)
        return Ieee80211NistErrorModel::getDataSuccessRate(mode, bitLength, scalarSnir);
    auto dataMode = heMode->getDataMode();
    int mcs = dataMode->getModulationAndCodingScheme()->getMcsIndex();
    auto coding = dataMode->isLdpc() ? Calibration::Coding::LDPC : Calibration::Coding::BCC;
    auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(
            snir->getReception()->getTransmission()->getPacket());
    int packetLength = phyHeader->getLengthField().get<B>();
    return computeHeSuccessRate(mcs, coding, dataMode->getNumberOfSpatialStreams(), false,
            packetLength, dataMode->getNumberOfTotalSubcarriers(), 0, snir, scalarSnir);
}

double Ieee80211RbirErrorModel::getHeDataSuccessRate(
        const Ieee80211HeUserPhyParameters& parameters, unsigned int bitLength,
        const ISnir *snir, double scalarSnir) const
{
    Calibration::Coding coding;
    if (parameters.coding == HE_CODING_BCC)
        coding = Calibration::Coding::BCC;
    else if (parameters.coding == HE_CODING_LDPC)
        coding = Calibration::Coding::LDPC;
    else
        throw cRuntimeError("Unsupported HE coding for RBIR evaluation");
    return computeHeSuccessRate(parameters.mcs, coding, parameters.numberOfSpatialStreams,
            parameters.dcm, parameters.psduLength.get<B>(), parameters.ru.toneSize,
            parameters.ru.toneOffset, snir, scalarSnir);
}

} // namespace physicallayer
} // namespace inet
