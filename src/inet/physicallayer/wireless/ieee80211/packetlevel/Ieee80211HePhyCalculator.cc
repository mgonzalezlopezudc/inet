//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>

namespace inet {
namespace physicallayer {

int getHeRuDataSubcarrierCount(int toneSize)
{
    switch (toneSize) {
        case 26: return 24;
        case 52: return 48;
        case 106: return 102;
        case 242: return 234;
        case 484: return 468;
        case 996: return 980;
        case 1992: return 1960;
        default: throw std::invalid_argument("Unsupported IEEE 802.11ax RU tone size");
    }
}

int getHeRuPilotSubcarrierCount(int toneSize)
{
    switch (toneSize) {
        case 26: return 2;
        case 52: return 4;
        case 106: return 4;
        case 242: return 8;
        case 484: return 16;
        case 996: return 16;
        case 1992: return 32;
        default: throw std::invalid_argument("Unsupported IEEE 802.11ax RU tone size");
    }
}

int getHeSigBSymbolCount(Hz channelBandwidth, int numberOfUsers)
{
    // IEEE 802.11-2024 Clause 27.3.11.13.2 ("HE-SIG-B field"):
    // HE-SIG-B is encoded with BPSK and code rate 1/2. Each content channel
    // uses 52 OFDM subcarriers for data: 52 coded bits × 1/2 = 26 information
    // bits per symbol.
    constexpr int HE_SIG_B_DATA_BITS_PER_SYMBOL = 26;
    // IEEE 802.11-2024 Table 27-26 ("Contents of the User Specific field"):
    // each User Specific subfield is 21 bits wide.
    constexpr int HE_SIG_B_USER_FIELD_BITS_PER_USER = 21;
    // Each content channel also carries a 10-bit tail/pad in both the Common
    // field and the User field (6 tail bits + 4 pad bits, Clause 27.3.11.13.2).
    constexpr int HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL = 10;

    int widthMhz = std::lround(channelBandwidth.get() / 1e6);
    int contentChannels = getHeSigBContentChannelCount(channelBandwidth);
    int twentyMhzChannels = widthMhz / 20;
    int commonBitsPerContentChannel = 8 * ((twentyMhzChannels + contentChannels - 1) / contentChannels)
            + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL;
    int usersPerContentChannel = (numberOfUsers + contentChannels - 1) / contentChannels;
    int userBitsPerContentChannel = usersPerContentChannel * HE_SIG_B_USER_FIELD_BITS_PER_USER
            + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL;
    return std::max(1, (commonBitsPerContentChannel + userBitsPerContentChannel
            + HE_SIG_B_DATA_BITS_PER_SYMBOL - 1) / HE_SIG_B_DATA_BITS_PER_SYMBOL);
}

Ieee80211HePhyValidationResult computeHePpduParameters(
        const std::vector<Ieee80211HeUserPhyParameters>& requestedUsers,
        Hz channelBandwidth,
        Ieee80211HePpduFormat ppduFormat,
        Ieee80211HeGuardInterval guardInterval,
        Ieee80211HeLtfType ltfType,
        int packetExtensionDurationUs,
        bool enforceDurationLimit)
{
    Ieee80211HePhyValidationResult result;
    if (requestedUsers.empty()) {
        result.error = "HE PPDU has no users";
        return result;
    }

    // Group users by RU index to detect and validate MU-MIMO.
    // IEEE 802.11-2024, Clause 27.3.11.13 ("HE MU PPDU").
    // Validates standard spatial stream limits:
    // - Maximum MU-MIMO group size is 8 users.
    // - Maximum spatial streams per user is 4.
    // - Total spatial streams (N_STS) in a group cannot exceed 8.
    // - User spatial streams must be contiguous (no gaps or overlapping indices).
    std::map<int, std::vector<Ieee80211HeUserPhyParameters>> ruGroups;
    for (const auto& requested : requestedUsers) {
        ruGroups[requested.ru.index].push_back(requested);
    }
    for (const auto& pair : ruGroups) {
        const auto& group = pair.second;
        if (group.size() > 1) {
            if (group.size() > 8) {
                result.error = "HE MU-MIMO group has too many users (max 8)";
                return result;
            }
            uint16_t toneSize = group[0].ru.toneSize;
            uint16_t toneOffset = group[0].ru.toneOffset;
            std::set<uint16_t> staIds;
            std::vector<std::pair<int, int>> streams; // {startIndex, nss}
            int groupTotalNsts = 0;
            for (const auto& user : group) {
                if (user.ru.toneSize != toneSize || user.ru.toneOffset != toneOffset) {
                    result.error = "HE MU-MIMO users on the same RU must have matching tone size and offset";
                    return result;
                }
                if (staIds.count(user.staId) > 0) {
                    result.error = "HE MU-MIMO group contains duplicate STA IDs";
                    return result;
                }
                staIds.insert(user.staId);
                if (user.numberOfSpatialStreams > 4) {
                    result.error = "HE MU-MIMO user cannot have more than 4 spatial streams";
                    return result;
                }
                streams.push_back({user.streamStartIndex, user.numberOfSpatialStreams});
                groupTotalNsts += user.numberOfSpatialStreams;
            }
            if (groupTotalNsts > 8) {
                result.error = "HE MU-MIMO group total spatial streams exceeds 8";
                return result;
            }
            std::sort(streams.begin(), streams.end());
            int expectedStart = 0;
            for (const auto& stream : streams) {
                if (stream.first != expectedStart) {
                    result.error = "HE MU-MIMO spatial streams are not contiguous or have gaps/overlaps";
                    return result;
                }
                expectedStart += stream.second;
            }
        }
    }

    if (packetExtensionDurationUs != 0 && packetExtensionDurationUs != 4 &&
            packetExtensionDurationUs != 8 && packetExtensionDurationUs != 12 && packetExtensionDurationUs != 16) {
        result.error = "invalid HE packet extension duration";
        return result;
    }
    try {
        getHeSigBContentChannelCount(channelBandwidth);
        getHeGuardIntervalDuration(guardInterval);
        getHeLtfSymbolDuration(ltfType);
    }
    catch (const omnetpp::cRuntimeError& error) {
        result.error = error.what();
        return result;
    }

    auto& parameters = result.parameters;
    parameters.common.ppduFormat = ppduFormat;
    parameters.common.channelBandwidth = channelBandwidth;
    parameters.common.guardInterval = guardInterval;
    parameters.common.ltfType = ltfType;
    parameters.common.packetExtensionDurationUs = packetExtensionDurationUs;
    parameters.common.sigA.ppduFormat = ppduFormat;
    parameters.common.sigA.uplink = ppduFormat == HE_TRIGGER_BASED_UPLINK;
    parameters.common.sigB.numberOfSymbols = ppduFormat == HE_MU_DOWNLINK ?
            getHeSigBSymbolCount(channelBandwidth, requestedUsers.size()) : 0;
    // See getHeSigBSymbolCount() for the origin of these constants.
    constexpr int HE_SIG_B_USER_FIELD_BITS_PER_USER = 21;
    constexpr int HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL = 10;
    parameters.common.sigB.commonFieldBits = ppduFormat == HE_MU_DOWNLINK ?
            8 * std::lround(channelBandwidth.get() / 20e6) + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL : 0;
    parameters.common.sigB.userFieldBits = ppduFormat == HE_MU_DOWNLINK ?
            HE_SIG_B_USER_FIELD_BITS_PER_USER * (int)requestedUsers.size() + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL : 0;
    parameters.common.heSigBDuration =
            parameters.common.sigB.numberOfSymbols * SimTime(4, SIMTIME_US);

    int totalSpaceTimeStreams = 0;
    for (const auto& requested : requestedUsers)
        totalSpaceTimeStreams += requested.numberOfSpatialStreams;
    if (totalSpaceTimeStreams < 1 || totalSpaceTimeStreams > 8) {
        result.error = "HE PPDU has an unsupported total number of space-time streams";
        return result;
    }
    parameters.common.numberOfHeLtfSymbols = getHeNumberOfLtfSymbols(totalSpaceTimeStreams);
    parameters.common.heLtfDuration =
            parameters.common.numberOfHeLtfSymbols * getHeLtfSymbolDuration(ltfType);
    parameters.common.commonPreambleDuration =
            parameters.common.legacyPreambleDuration +
            parameters.common.rlSigDuration +
            parameters.common.heSigADuration +
            parameters.common.heSigBDuration +
            parameters.common.heStfDuration +
            parameters.common.heLtfDuration;

    auto symbolDuration = SimTime(12800, SIMTIME_NS) + getHeGuardIntervalDuration(guardInterval);
    for (const auto& requested : requestedUsers) {
        auto user = requested;
        if (user.ru.toneSize <= 0) {
            result.error = "invalid HE RU tone size";
            return result;
        }
        if (user.numberOfSpatialStreams < 1 || user.numberOfSpatialStreams > 8) {
            result.error = "invalid HE number of spatial streams";
            return result;
        }
        if (user.coding != HE_CODING_BCC && user.coding != HE_CODING_LDPC) {
            result.error = "invalid HE coding type";
            return result;
        }
        // IEEE Std 802.11-2024 Clause 27.3.12.5 ("Coding"):
        // "LDPC is the only FEC coding scheme in the HE PPDU Data field for a 484-, 996-, and 2x996-tone RU."
        // "LDPC is the only FEC coding scheme in the HE PPDU Data field for HE-MCSs 10 and 11."
        // "Support for BCC coding is limited to less than or equal to four spatial streams..."
        if (user.coding == HE_CODING_BCC) {
            if (user.mcs == 10 || user.mcs == 11) {
                result.error = "HE BCC coding is not supported for MCS 10 or 11";
                return result;
            }
            if (user.ru.toneSize >= 484) {
                result.error = "HE BCC coding is not supported for 484-tone RUs or larger";
                return result;
            }
            if (user.numberOfSpatialStreams > 4) {
                result.error = "HE BCC coding is limited to less than or equal to 4 spatial streams";
                return result;
            }
        }
        if (user.dcm && !isHeDcmCombinationSupported(user.mcs, user.numberOfSpatialStreams)) {
            result.error = "unsupported HE DCM combination";
            return result;
        }
        // IEEE 802.11-2024 Tables 27-62..27-117: reject N/A (MCS, Nss, RU) triples.
        if (!isHeValidMcsNssCombination(user.mcs, user.numberOfSpatialStreams, user.ru.toneSize)) {
            result.error = std::string("HE MCS ") + std::to_string(user.mcs)
                    + ", Nss=" + std::to_string(user.numberOfSpatialStreams)
                    + ", RU=" + std::to_string(user.ru.toneSize)
                    + "-tone is N/A per IEEE 802.11-2024 Tables 27-62..27-117";
            return result;
        }
        int dataSubcarriers = user.ru.dataSubcarriers > 0 ? user.ru.dataSubcarriers :
                getHeRuDataSubcarrierCount(user.ru.toneSize);
        auto codeRate = getHeMcsCodeRate(user.mcs);
        user.guardInterval = guardInterval;
        user.codedBitsPerSymbol = dataSubcarriers * getHeMcsBitsPerSubcarrier(user.mcs) *
                user.numberOfSpatialStreams;
        user.dataBitsPerSymbol = user.codedBitsPerSymbol * codeRate.first / codeRate.second;
        if (user.dcm)
            user.dataBitsPerSymbol /= 2;
        if (user.dataBitsPerSymbol <= 0) {
            result.error = "HE user has no data bits per symbol";
            return result;
        }
        // IEEE Std 802.11-2024 Clause 27.3.12.5.1 ("BCC coding and puncturing"):
        // "When conducting BCC FEC encoding for an HE PPDU, the number of encoders is always 1."
        if (user.coding == HE_CODING_BCC) {
            user.numberOfEncoders = 1;
        } else {
            user.numberOfEncoders = std::max(1, (user.dataBitsPerSymbol + 647) / 648);
        }
        user.tailBits = user.coding == HE_CODING_LDPC ? 0 : 6 * user.numberOfEncoders;
        int64_t uncodedBits = user.serviceBits + user.psduLength.get<B>() * 8 + user.tailBits;
        if (user.coding == HE_CODING_LDPC) {
            // 802.11 LDPC uses 648/1296/1944-bit codewords. At packet level
            // we model codeword selection, shortening and repetition while
            // retaining the standard NDBPS symbol rounding used by the PHY.
            // The largest legal codeword which can carry a single shortened
            // payload is chosen first; additional payload is split over equal
            // codewords. This makes the boundary behaviour deterministic and
            // keeps the accounting shared by DL and HE-TB calculations.
            const int candidates[] = {648, 1296, 1944};
            int codeRateNumerator = codeRate.first;
            int codeRateDenominator = codeRate.second;
            user.ldpcCodewordLength = 1944;
            for (int candidate : candidates) {
                if (uncodedBits <= candidate * codeRateNumerator / codeRateDenominator) {
                    user.ldpcCodewordLength = candidate;
                    break;
                }
            }
            int informationBitsPerCodeword = user.ldpcCodewordLength * codeRateNumerator / codeRateDenominator;
            user.ldpcCodewordCount = std::max<int64_t>(1,
                    (uncodedBits + informationBitsPerCodeword - 1) / informationBitsPerCodeword);
            int64_t ldpcInformationCapacity = (int64_t)user.ldpcCodewordCount * informationBitsPerCodeword;
            user.ldpcShorteningBits = std::max<int64_t>(0, ldpcInformationCapacity - uncodedBits);
            int64_t codedBits = (int64_t)user.ldpcCodewordCount * user.ldpcCodewordLength;
            user.numberOfDataSymbols = std::max<int64_t>(1,
                    (codedBits + user.codedBitsPerSymbol - 1) / user.codedBitsPerSymbol);
            int64_t symbolCapacity = (int64_t)user.numberOfDataSymbols * user.codedBitsPerSymbol;
            user.ldpcRepetitionBits = std::max<int64_t>(0, symbolCapacity - codedBits);
        }
        else {
            user.numberOfDataSymbols = std::max<int64_t>(1,
                    (uncodedBits + user.dataBitsPerSymbol - 1) / user.dataBitsPerSymbol);
        }
        int64_t bitsInLastSymbol = uncodedBits -
                (int64_t)(user.numberOfDataSymbols - 1) * user.dataBitsPerSymbol;
        user.preFecPaddingFactor = std::clamp<int>(
                (4 * bitsInLastSymbol + user.dataBitsPerSymbol - 1) / user.dataBitsPerSymbol, 1, 4);
        int effectiveLastSymbolBits =
                (user.preFecPaddingFactor * user.dataBitsPerSymbol + 3) / 4;
        user.postFecPaddingBits = std::max<int64_t>(0, effectiveLastSymbolBits - bitsInLastSymbol);
        user.numberOfSymbols = user.numberOfDataSymbols;
        parameters.commonNumberOfDataSymbols =
                std::max(parameters.commonNumberOfDataSymbols, user.numberOfDataSymbols);
        parameters.users.push_back(user);
    }

    parameters.duration = parameters.common.commonPreambleDuration +
            parameters.commonNumberOfDataSymbols * symbolDuration +
            SimTime(packetExtensionDurationUs, SIMTIME_US);
    
    // IEEE 802.11ax imposes a strict physical limit of 5.484 ms (5484 µs) on total PPDU transmission duration
    // to guarantee fair medium access and compatibility (Clause 27.3.11.8).
    if (enforceDurationLimit && parameters.duration > SimTime(5.484, SIMTIME_MS)) {
        result.error = "HE PPDU exceeds the 5.484 ms duration limit";
        return result;
    }
    for (auto& user : parameters.users) {
        user.dataDuration = parameters.commonNumberOfDataSymbols * symbolDuration;
        user.preambleDuration = parameters.common.commonPreambleDuration;
        user.headerDuration = SIMTIME_ZERO;
        user.duration = parameters.duration;
    }
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet
