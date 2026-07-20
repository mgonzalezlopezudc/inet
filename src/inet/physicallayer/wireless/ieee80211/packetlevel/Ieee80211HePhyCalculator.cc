//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

#include <algorithm>
#include <set>
#include <stdexcept>

// HE PHY parameter and duration calculator.
//
// Computes per-user and common PHY parameters for HE SU, HE ER SU, HE MU and
// HE TB PPDUs based on IEEE 802.11-2024:
//   - Clause 27.3.2: RU tone sizes, data/pilot subcarriers.
//   - Clause 27.3.4: HE PPDU formats and preamble field ordering.
//   - Clause 27.3.11.8: HE-SIG-B field sizing and content channels.
//   - Clause 27.3.12: modulation, coding, number of symbols, padding.
//   - Clause 27.3.12.5: BCC/LDPC coding rules and constraints.
//
// Approximations:
//   - LDPC shortening and repetition are modeled at packet level using the
//     standard codeword lengths (648/1296/1944 bits), but the exact bit-level
//     shortening/repetition procedure of Clause 27.3.12.5.2 is approximated.
//   - HE-SIG-B symbol count uses a closed-form estimate of Clause 27.3.11.8
//     rather than bit-level encoder emulation.

namespace inet {
namespace physicallayer {

namespace {

constexpr int HE_SIG_B_DATA_BITS_PER_SYMBOL = 26;
constexpr int HE_SIG_B_USER_FIELD_BITS_PER_USER = 21;
constexpr int HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK = 10;

int getHeSigBUserBlockCount(int numberOfUsers)
{
    return (numberOfUsers + 1) / 2;
}

bool samePhysicalHeRu(const Ieee80211HeRu& left, const Ieee80211HeRu& right)
{
    bool sameCenterFrequency = left.centerFrequency == right.centerFrequency ||
            (std::isnan(left.centerFrequency.get()) && std::isnan(right.centerFrequency.get()));
    return sameCenterFrequency && left.toneSize == right.toneSize &&
            left.toneOffset == right.toneOffset;
}

bool isHeLtfGiCombinationAllowed(Ieee80211HePpduFormat ppduFormat,
        Ieee80211HeLtfType ltfType, Ieee80211HeGuardInterval guardInterval,
        bool isFeedbackNdp, bool isFullBandwidthUlMuMimo)
{
    if (ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU)
        return (ltfType == HE_LTF_1X && guardInterval == HE_GI_0_8_US) ||
                (ltfType == HE_LTF_2X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_1_6_US)) ||
                (ltfType == HE_LTF_4X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_3_2_US));
    if (ppduFormat == HE_MU_DOWNLINK)
        return (ltfType == HE_LTF_2X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_1_6_US)) ||
                (ltfType == HE_LTF_4X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_3_2_US));
    if (isFeedbackNdp)
        return ltfType == HE_LTF_4X && guardInterval == HE_GI_3_2_US;
    return (ltfType == HE_LTF_2X && guardInterval == HE_GI_1_6_US) ||
            (ltfType == HE_LTF_4X && guardInterval == HE_GI_3_2_US) ||
            (isFullBandwidthUlMuMimo && ltfType == HE_LTF_1X && guardInterval == HE_GI_1_6_US);
}

} // namespace

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
    // This constrained estimate assumes uncompressed HE-SIG-B MCS 0 without
    // DCM. Each content channel therefore carries 26 information bits per
    // symbol. Tables 27-28 through 27-30 define 21-bit User fields and a 4-bit
    // CRC plus 6-bit tail for every block of one or two User fields.
    int widthMhz = std::lround(channelBandwidth.get() / 1e6);
    int contentChannels = getHeSigBContentChannelCount(channelBandwidth);
    int twentyMhzChannels = widthMhz / 20;
    int commonBitsPerContentChannel = 8 * ((twentyMhzChannels + contentChannels - 1) / contentChannels)
            + HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
    int usersPerContentChannel = (numberOfUsers + contentChannels - 1) / contentChannels;
    int userBitsPerContentChannel = usersPerContentChannel * HE_SIG_B_USER_FIELD_BITS_PER_USER
            + getHeSigBUserBlockCount(usersPerContentChannel) * HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
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
    if (ppduFormat != HE_MU_DOWNLINK && ppduFormat != HE_TRIGGER_BASED_UPLINK &&
            ppduFormat != HE_SINGLE_USER && ppduFormat != HE_EXTENDED_RANGE_SU) {
        result.error = "invalid HE PPDU format";
        return result;
    }
    if (channelBandwidth != MHz(20) && channelBandwidth != MHz(40) &&
            channelBandwidth != MHz(80) && channelBandwidth != MHz(160)) {
        result.error = "unsupported HE channel bandwidth";
        return result;
    }
    if (guardInterval != HE_GI_0_8_US && guardInterval != HE_GI_1_6_US &&
            guardInterval != HE_GI_3_2_US) {
        result.error = "invalid HE guard interval";
        return result;
    }
    if (ltfType != HE_LTF_1X && ltfType != HE_LTF_2X && ltfType != HE_LTF_4X) {
        result.error = "invalid HE-LTF type";
        return result;
    }
    try {
        getHeLtfSymbolDuration(ltfType, guardInterval);
    }
    catch (const omnetpp::cRuntimeError& error) {
        result.error = error.what();
        return result;
    }
    if (packetExtensionDurationUs != 0 && packetExtensionDurationUs != 4 &&
            packetExtensionDurationUs != 8 && packetExtensionDurationUs != 12 && packetExtensionDurationUs != 16) {
        result.error = "invalid HE packet extension duration";
        return result;
    }
    if (requestedUsers.empty()) {
        result.error = "HE PPDU has no users";
        return result;
    }

    // Validate all externally supplied scalar values before calling the
    // throwing calculation helpers below. This API is also used as a public
    // feasibility check by schedulers, so malformed input must be reported in
    // the result in both release and debug builds.
    for (const auto& requested : requestedUsers) {
        if (requested.mcs < 0 || requested.mcs > 11) {
            result.error = "invalid HE MCS";
            return result;
        }
        if (requested.ru.toneSize != 26 && requested.ru.toneSize != 52 &&
                requested.ru.toneSize != 106 && requested.ru.toneSize != 242 &&
                requested.ru.toneSize != 484 && requested.ru.toneSize != 996 &&
                requested.ru.toneSize != 1992) {
            result.error = "unsupported HE RU tone size";
            return result;
        }
        if (requested.numberOfSpatialStreams < 1 || requested.numberOfSpatialStreams > 8) {
            result.error = "invalid HE number of spatial streams";
            return result;
        }
        if (requested.streamStartIndex < 0 ||
                requested.streamStartIndex > 8 - requested.numberOfSpatialStreams) {
            result.error = "HE RU spatial stream range exceeds 8 streams";
            return result;
        }
        if (requested.coding != HE_CODING_BCC && requested.coding != HE_CODING_LDPC) {
            result.error = "invalid HE coding type";
            return result;
        }
    }

    // Group users by physical RU identity to detect and validate MU-MIMO.
    // The model-local RU index is deliberately excluded: equivalent physical
    // RUs from different allocation catalogs may have different indices, and
    // the same numeric index may identify RUs in different channel segments.
    // IEEE 802.11-2024 Clause 27.3.11.8 links HE-SIG-B User fields to an RU
    // allocation; Clause 27.3.12.5 constrains per-user and per-RU FEC choices.
    // Validates standard spatial stream limits:
    // - Maximum MU-MIMO group size is 8 users.
    // - Maximum spatial streams per user is 4.
    // - Total spatial streams (N_STS) in a group cannot exceed 8.
    // - User spatial streams must be contiguous (no gaps or overlapping indices).
    std::vector<std::pair<Ieee80211HeRu, std::vector<Ieee80211HeUserPhyParameters>>> ruGroups;
    for (const auto& requested : requestedUsers) {
        auto group = std::find_if(ruGroups.begin(), ruGroups.end(),
                [&] (const auto& candidate) { return samePhysicalHeRu(candidate.first, requested.ru); });
        if (group == ruGroups.end())
            ruGroups.push_back({requested.ru, {requested}});
        else
            group->second.push_back(requested);
    }
    int maximumSpaceTimeStreamsPerRu = 0;
    for (const auto& pair : ruGroups) {
        const auto& group = pair.second;
        int groupTotalNsts = 0;
        for (const auto& user : group)
            groupTotalNsts += user.numberOfSpatialStreams;
        if (groupTotalNsts > 8) {
            result.error = "HE MU-MIMO group total spatial streams exceeds 8";
            return result;
        }
        maximumSpaceTimeStreamsPerRu = std::max(maximumSpaceTimeStreamsPerRu, groupTotalNsts);
        if (group.size() > 1) {
            if (group.size() > 8) {
                result.error = "HE MU-MIMO group has too many users (max 8)";
                return result;
            }
            std::set<uint16_t> staIds;
            std::vector<std::pair<int, int>> streams; // {startIndex, nss}
            for (const auto& user : group) {
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


    bool isFeedbackNdp = ppduFormat == HE_TRIGGER_BASED_UPLINK &&
            std::all_of(requestedUsers.begin(), requestedUsers.end(),
                    [] (const auto& requested) { return requested.psduLength == B(0); });
    bool isFullBandwidthUlMuMimo = ppduFormat == HE_TRIGGER_BASED_UPLINK &&
            ruGroups.size() == 1 && ruGroups.front().second.size() >= 2 &&
            ruGroups.front().first.toneSize == getHeChannelToneCount(channelBandwidth);
    // IEEE 802.11-2024 Table 27-32 marks several otherwise meaningful
    // HE-LTF/GI duration pairs N/A for particular PPDU formats. CM3 permits
    // 1x HE-LTF with 1.6 us GI only for full-bandwidth UL MU-MIMO.
    if (!isHeLtfGiCombinationAllowed(ppduFormat, ltfType, guardInterval,
            isFeedbackNdp, isFullBandwidthUlMuMimo)) {
        result.error = "HE-LTF/GI combination is not supported for the HE PPDU format";
        return result;
    }

    auto& parameters = result.parameters;
    parameters.common.ppduFormat = ppduFormat;
    parameters.common.channelBandwidth = channelBandwidth;
    parameters.common.guardInterval = guardInterval;
    parameters.common.ltfType = ltfType;
    parameters.common.packetExtensionDurationUs = packetExtensionDurationUs;
    parameters.common.heSigADuration = ppduFormat == HE_EXTENDED_RANGE_SU ?
            SimTime(16, SIMTIME_US) : SimTime(8, SIMTIME_US);
    parameters.common.sigA.ppduFormat = ppduFormat;
    parameters.common.sigA.uplink = ppduFormat == HE_TRIGGER_BASED_UPLINK;
    parameters.common.sigB.numberOfSymbols = ppduFormat == HE_MU_DOWNLINK ?
            getHeSigBSymbolCount(channelBandwidth, requestedUsers.size()) : 0;
    if (ppduFormat == HE_MU_DOWNLINK) {
        int contentChannels = getHeSigBContentChannelCount(channelBandwidth);
        int numberOfUsers = requestedUsers.size();
        parameters.common.sigB.commonFieldBits = 8 * std::lround(channelBandwidth.get() / 20e6) +
                contentChannels * HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
        parameters.common.sigB.userFieldBits =
                HE_SIG_B_USER_FIELD_BITS_PER_USER * numberOfUsers;
        int usersPerContentChannel = numberOfUsers / contentChannels;
        int channelsWithExtraUser = numberOfUsers % contentChannels;
        for (int channel = 0; channel < contentChannels; channel++) {
            int channelUsers = usersPerContentChannel + (channel < channelsWithExtraUser ? 1 : 0);
            parameters.common.sigB.userFieldBits +=
                    getHeSigBUserBlockCount(channelUsers) * HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
        }
    }
    parameters.common.heSigBDuration =
            parameters.common.sigB.numberOfSymbols * SimTime(4, SIMTIME_US);

    if (isFeedbackNdp) {
        parameters.common.numberOfHeLtfSymbols = 2;
        parameters.common.heStfDuration = SimTime(8, SIMTIME_US);
    } else {
        // Deterministic calculator policy: select the minimum legal HE-LTF
        // count for the maximum initial N_STS on any physical RU. This avoids
        // summing concurrent OFDMA RUs; it is not a standard requirement that
        // a transmitter always choose exactly this HE-LTF count.
        parameters.common.numberOfHeLtfSymbols = getHeNumberOfLtfSymbols(maximumSpaceTimeStreamsPerRu);
        parameters.common.heStfDuration = ppduFormat == HE_TRIGGER_BASED_UPLINK ?
                SimTime(8, SIMTIME_US) : SimTime(4, SIMTIME_US);
    }
    parameters.common.heLtfDuration =
            parameters.common.numberOfHeLtfSymbols * getHeLtfSymbolDuration(ltfType, guardInterval);
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
        if (user.psduLength == B(0)) {
            user.numberOfDataSymbols = 0;
            user.numberOfSymbols = 0;
            user.preFecPaddingFactor = 0;
            user.postFecPaddingBits = 0;
            user.ldpcCodewordLength = 0;
            user.ldpcCodewordCount = 0;
            user.ldpcShorteningBits = 0;
            user.ldpcRepetitionBits = 0;
            user.numberOfEncoders = 0;
            user.tailBits = 0;
            parameters.users.push_back(user);
            continue;
        }
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
    
    // IEEE 802.11-2024 Table 27-61 defines aPPDUMaxTime = 5.484 ms; Clause 10.12
    // forbids transmitting an HE PPDU whose PLME-TXTIME exceeds that limit.
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
