//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCPERTABLE_H
#define __INET_IEEE80211LDPCPERTABLE_H

#include <istream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"

namespace inet {
namespace physicallayer {

class IIeee80211DataMode;

struct INET_API Ieee80211LdpcPerCurveKey {
    Ieee80211PhyFormat phyFormat;
    int bandwidthMhz;
    int mcs;
    int numberOfSpatialStreams;
    int numberOfCodewords;
    int ldpcCodewordLength;
    int shortenedBits;
    int puncturedBits;
    int repeatedBits;

    auto asTuple() const
    {
        return std::tie(phyFormat, bandwidthMhz, mcs, numberOfSpatialStreams,
                        numberOfCodewords, ldpcCodewordLength, shortenedBits,
                        puncturedBits, repeatedBits);
    }

    bool operator<(const Ieee80211LdpcPerCurveKey& other) const { return asTuple() < other.asTuple(); }
    bool operator==(const Ieee80211LdpcPerCurveKey& other) const { return asTuple() == other.asTuple(); }
};

struct INET_API Ieee80211LdpcPerPoint {
    double snrDb;
    double packetErrorRate;
};

/**
 * Strict, deterministic LDPC packet-error-rate curve table.
 *
 * Curves are calibrated and validated by a complete structural key, then
 * matched by format, bandwidth, and per-stream MCS. SNR is expressed in dB.
 * Within a curve, interpolation is linear in log10(PER), and values outside
 * the sampled interval are clamped to the nearest endpoint.
 *
 * Modeling assumption: for each calibrated format/bandwidth/per-stream-MCS
 * tuple, the PER-vs-SNR curve is invariant across all LDPC payload/codeword
 * sizes contemplated by the standard and across NSS when spatial streams are
 * ideally separated. HT MCS 8..31 therefore reuse the MCS modulo 8 curve;
 * VHT uses its received MCS unchanged. The structural fields are retained for
 * strict CSV validation and calibration provenance.
 */
class INET_API Ieee80211LdpcPerTable
{
  public:
    using Curve = std::vector<Ieee80211LdpcPerPoint>;

  protected:
    std::map<Ieee80211LdpcPerCurveKey, Curve> curves;
    using ModeKey = std::tuple<Ieee80211PhyFormat, int, int>;
    std::map<ModeKey, Ieee80211LdpcPerCurveKey> modeCurveKeys;

  public:
    static void validateKey(const Ieee80211LdpcPerCurveKey& key, const std::string& source, int lineNumber);

    void load(const char *tableFile);
    void load(std::istream& input, const char *sourceName = "<stream>");

    size_t getNumberOfCurves() const { return modeCurveKeys.size(); }
    const Curve& getCurve(const Ieee80211LdpcPerCurveKey& key) const;
    double getPacketErrorRate(const Ieee80211LdpcPerCurveKey& key, double snrDb) const;
};

INET_API Ieee80211LdpcPerCurveKey makeIeee80211LdpcPerCurveKey(
        const IIeee80211DataMode& mode, const Ieee80211DataEncodingPlan& plan);

} // namespace physicallayer
} // namespace inet

#endif
