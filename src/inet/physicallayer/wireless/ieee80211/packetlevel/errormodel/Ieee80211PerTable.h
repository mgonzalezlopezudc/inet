//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211PERTABLE_H
#define __INET_IEEE80211PERTABLE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/**
 * A small, deterministic lookup for a future supplied HT AWGN PER table.
 *
 * The production implementation deliberately does not contain a PER table:
 * the table and its manifest are deployment inputs.  CSV rows use the schema
 * coding,bandwidth_mhz,mcs,psdu_bytes,snr_db,per and are restricted to
 * BCC HT MCS 0--7 at 20 or 40 MHz.
 */
class INET_API Ieee80211PerTable
{
  public:
    struct Point {
        double snrDb;
        double per;
    };

  protected:
    struct Key {
        int bandwidthMHz;
        int mcs;
        uint64_t referenceLength;

        bool operator<(const Key& other) const {
            if (bandwidthMHz != other.bandwidthMHz)
                return bandwidthMHz < other.bandwidthMHz;
            if (mcs != other.mcs)
                return mcs < other.mcs;
            return referenceLength < other.referenceLength;
        }
    };

    std::map<Key, std::vector<Point>> tables;

    static int bandwidthToMHz(double bandwidthMHz);
    static std::vector<std::string> split(const std::string& line);
    static std::string trim(const std::string& text);
    static void validatePointAxis(const std::vector<Point>& points, const Key& key);

  public:
    void clear() { tables.clear(); }
    void loadCsv(const std::string& filename);
    void requireSha256(const std::string& filename, const std::string& expectedSha256) const;

    /**
     * Returns the packet error probability after Eq. 1 length scaling.  The
     * table is interpolated in SNR dB and then converted with the
     * complementary-log-log packet-length relation.
     */
    double getPacketErrorRate(int bandwidthMHz, int mcs, uint64_t psduLengthBytes, double snrDb) const;

    bool empty() const { return tables.empty(); }
    static std::string sha256File(const std::string& filename);
};

} // namespace physicallayer
} // namespace inet

#endif
