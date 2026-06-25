//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HESIGCODEC_H
#define __INET_IEEE80211HESIGCODEC_H

#include <algorithm>
#include <string>
#include <vector>
#include <map>

// HE-SIG-B Common field codec.
//
// Encodes and decodes the RU allocation subfields of the HE-SIG-B Common field
// as defined in IEEE 802.11-2024 Clause 27.3.11.13.2 and Table 27-27.  The
// codec maps between a concrete Ieee80211HeRu layout and the compact 8-bit
// allocation codes carried in HE-SIG-B content channels.
//
// Approximations / limitations:
//   - Punctured subchannels are accepted as a parameter but are not currently
//     incorporated into the encoded common field.
//   - Code points 116-127 of Table 27-27 are treated as invalid.  If the
//     standard assigns meaning to any of these reserved codes, the codec should
//     be updated.
//   - Encoding is performed by brute-force search over all codes 0-215 rather
//     than a direct inverse table lookup.  This works for standard HE RU layouts
//     but is fragile if the table is extended.
//   - Bandwidth support is limited to 20/40/80/160 MHz (no 320 MHz / EHT).
//   - HE-SIG-B modulation/coding constraints (BPSK, DCM, etc.) are not modeled
//     here; only the RU allocation semantics are handled.

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace physicallayer {

/** One content channel's allocation subfields. */
struct Ieee80211HeSigBContentChannel {
    std::vector<uint8_t> ruAllocationSubfields; // Table 27-27 B7..B0 values
    bool hasCenterRu = false;                   // Center 26-tone RU present
};

/** Wire-level HE-SIG-B Common field layout. */
struct Ieee80211HeSigBCommonField {
    std::vector<Ieee80211HeSigBContentChannel> contentChannels; // 1 or 2
    std::vector<Ieee80211HeRu> rus;             // resolved RU objects
};

struct Ieee80211HeSigBCommonFieldResult {
    bool valid = false;
    std::string error;
    Ieee80211HeSigBCommonField commonField;

    explicit operator bool() const { return valid; }
};

/** HE-SIG-B representation of an RU layout and its compact allocation codes. */
struct Ieee80211HeSigBRuAllocation
{
    std::vector<uint8_t> allocationCodes;
    std::vector<Ieee80211HeRu> rus;
};

/** Non-throwing result of HE-SIG-B RU allocation encoding or decoding. */
struct Ieee80211HeSigCodecResult
{
    bool valid = false;
    std::string error;
    Ieee80211HeSigBRuAllocation allocation;

    explicit operator bool() const { return valid; }
};

inline bool decodeTable27_27(uint8_t code, std::vector<std::pair<int, int>>& RUs, std::vector<int>& userCounts)
{
    RUs.clear();
    userCounts.clear();
    if (code <= 15) {
        // slots: 1&2 (52 @ 0), 3&4 (52 @ 54), 5 (26 @ 108), 6&7 (52 @ 136), 8&9 (52 @ 190)
        if (code & 8) {
            RUs.push_back({52, 0}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 0}); userCounts.push_back(1);
            RUs.push_back({26, 26}); userCounts.push_back(1);
        }
        if (code & 4) {
            RUs.push_back({52, 54}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 54}); userCounts.push_back(1);
            RUs.push_back({26, 80}); userCounts.push_back(1);
        }
        RUs.push_back({26, 108}); userCounts.push_back(1);
        if (code & 2) {
            RUs.push_back({52, 136}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 136}); userCounts.push_back(1);
            RUs.push_back({26, 162}); userCounts.push_back(1);
        }
        if (code & 1) {
            RUs.push_back({52, 190}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 190}); userCounts.push_back(1);
            RUs.push_back({26, 216}); userCounts.push_back(1);
        }
        return true;
    }
    if (code >= 16 && code <= 23) {
        int y = code - 16;
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 24 && code <= 31) {
        int y = code - 24;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code >= 32 && code <= 39) {
        int y = code - 32;
        RUs.push_back({26, 0}); userCounts.push_back(1);
        RUs.push_back({26, 26}); userCounts.push_back(1);
        RUs.push_back({26, 54}); userCounts.push_back(1);
        RUs.push_back({26, 80}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 40 && code <= 47) {
        int y = code - 40;
        RUs.push_back({26, 0}); userCounts.push_back(1);
        RUs.push_back({26, 26}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 48 && code <= 55) {
        int y = code - 48;
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({26, 54}); userCounts.push_back(1);
        RUs.push_back({26, 80}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 56 && code <= 63) {
        int y = code - 56;
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 64 && code <= 71) {
        int y = code - 64;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({26, 136}); userCounts.push_back(1);
        RUs.push_back({26, 162}); userCounts.push_back(1);
        RUs.push_back({26, 190}); userCounts.push_back(1);
        RUs.push_back({26, 216}); userCounts.push_back(1);
        return true;
    }
    if (code >= 72 && code <= 79) {
        int y = code - 72;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({26, 136}); userCounts.push_back(1);
        RUs.push_back({26, 162}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code >= 80 && code <= 87) {
        int y = code - 80;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({26, 190}); userCounts.push_back(1);
        RUs.push_back({26, 216}); userCounts.push_back(1);
        return true;
    }
    if (code >= 88 && code <= 95) {
        int y = code - 88;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code >= 96 && code <= 111) {
        int val = code - 96;
        int n1 = ((val >> 2) & 3) + 1;
        int n2 = (val & 3) + 1;
        RUs.push_back({106, 0}); userCounts.push_back(n1);
        RUs.push_back({106, 136}); userCounts.push_back(n2);
        return true;
    }
    if (code == 112) {
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code == 113) {
        RUs.push_back({242, 0}); userCounts.push_back(0);
        return true;
    }
    if (code == 114) {
        RUs.push_back({484, 0}); userCounts.push_back(0);
        return true;
    }
    if (code == 115) {
        RUs.push_back({996, 0}); userCounts.push_back(0);
        return true;
    }
    if (code >= 128 && code <= 191) {
        int val = code - 128;
        int n1 = ((val >> 3) & 7) + 1;
        int n2 = (val & 7) + 1;
        RUs.push_back({106, 0}); userCounts.push_back(n1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(n2);
        return true;
    }
    if (code >= 192 && code <= 199) {
        int n = code - 192 + 1;
        RUs.push_back({242, 0}); userCounts.push_back(n);
        return true;
    }
    if (code >= 200 && code <= 207) {
        int n = code - 200 + 1;
        RUs.push_back({484, 0}); userCounts.push_back(n);
        return true;
    }
    if (code >= 208 && code <= 215) {
        int n = code - 208 + 1;
        RUs.push_back({996, 0}); userCounts.push_back(n);
        return true;
    }
    return false;
}

inline Ieee80211HeSigBCommonFieldResult encodeHeSigBCommonField(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth,
        const std::vector<bool>& puncturedSubchannels = {})
{
    Ieee80211HeSigBCommonFieldResult result;
    if (!validateHeRuLayout(rus, channelBandwidth)) {
        result.error = "overlapping, out-of-band, or reserved HE RU allocation";
        return result;
    }

    auto subchannelRUs = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    subchannelRUs.erase(std::remove_if(subchannelRUs.begin(), subchannelRUs.end(),
        [](const Ieee80211HeRu& ru) { return ru.toneSize != 242; }), subchannelRUs.end());
    std::sort(subchannelRUs.begin(), subchannelRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
        return a.toneOffset < b.toneOffset;
    });

    int K = subchannelRUs.size();
    int numContentChannels = (channelBandwidth > Hz(20e6)) ? 2 : 1;
    result.commonField.contentChannels.resize(numContentChannels);

    // Group user count per RU
    std::map<int, int> userCountsByRuIndex;
    for (const auto& ru : rus) {
        if (ru.index >= 0) {
            userCountsByRuIndex[ru.index]++;
        }
    }

    // Set center 26-tone RU flags
    for (const auto& ru : rus) {
        if (ru.toneSize == 26) {
            if (ru.toneOffset == 485) {
                result.commonField.contentChannels[0].hasCenterRu = true;
                if (channelBandwidth == Hz(80e6)) {
                    result.commonField.contentChannels[1].hasCenterRu = true;
                }
            } else if (ru.toneOffset == 1481 && numContentChannels > 1) {
                result.commonField.contentChannels[1].hasCenterRu = true;
            }
        }
    }

    for (int s = 0; s < K; ++s) {
        int c = s % 2;
        auto& cc = result.commonField.contentChannels[c];

        // Find partitions in this 20-MHz subchannel
        std::vector<std::pair<int, int>> partitionKeys;
        std::vector<int> partitionUsers;
        bool isWide = false;
        Ieee80211HeRu wideRU;

        for (const auto& ru : rus) {
            if (ru.toneSize > 242) {
                if (ru.toneOffset <= subchannelRUs[s].toneOffset &&
                    ru.toneOffset + ru.toneSize >= subchannelRUs[s].toneOffset + 242) {
                    isWide = true;
                    wideRU = ru;
                    break;
                }
            } else if (ru.toneOffset >= subchannelRUs[s].toneOffset &&
                       ru.toneOffset + ru.toneSize <= subchannelRUs[s].toneOffset + 242) {
                partitionKeys.push_back({ru.toneSize, ru.toneOffset - subchannelRUs[s].toneOffset});
            }
        }

        if (isWide) {
            int totalUsers = userCountsByRuIndex[wideRU.index];
            int n_c = 0;
            if (totalUsers > 0) {
                int n_cc1 = (totalUsers + 1) / 2;
                int n_cc2 = totalUsers / 2;
                n_c = (c == 0) ? n_cc1 : n_cc2;
            }
            uint8_t code = 0;
            if (wideRU.toneSize == 484) {
                if (n_c == 0) {
                    code = 114;
                } else {
                    code = 200 + (n_c - 1);
                }
            } else { // >= 996
                // Determine if this is the first subchannel of the wide RU in this CC
                bool isFirst = true;
                for (int prev_s = c; prev_s < s; prev_s += 2) {
                    if (wideRU.toneOffset <= subchannelRUs[prev_s].toneOffset &&
                        wideRU.toneOffset + wideRU.toneSize >= subchannelRUs[prev_s].toneOffset + 242) {
                        isFirst = false;
                        break;
                    }
                }
                if (isFirst) {
                    if (n_c == 0) {
                        code = 115;
                    } else {
                        code = 208 + (n_c - 1);
                    }
                } else {
                    code = 115;
                }
            }
            cc.ruAllocationSubfields.push_back(code);
        } else {
            // Sort by relative offset
            std::sort(partitionKeys.begin(), partitionKeys.end(),
                [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                    return a.second < b.second;
                });
            for (const auto& key : partitionKeys) {
                // Find matching RU in input rus to get its user count
                int userCount = 0;
                for (const auto& ru : rus) {
                    if (ru.toneSize == key.first &&
                        (ru.toneOffset - subchannelRUs[s].toneOffset) == key.second) {
                        userCount = userCountsByRuIndex[ru.index];
                        break;
                    }
                }
                partitionUsers.push_back(userCount);
            }

            // Search Table 27-27 for a matching code
            uint8_t selectedCode = 113; // default empty/punctured
            bool found = false;
            for (int code = 0; code <= 215; ++code) {
                std::vector<std::pair<int, int>> candidateRUs;
                std::vector<int> candidateUsers;
                if (decodeTable27_27(code, candidateRUs, candidateUsers)) {
                    if (candidateRUs.size() == partitionKeys.size()) {
                        bool match = true;
                        for (size_t i = 0; i < candidateRUs.size(); ++i) {
                            if (candidateRUs[i].first != partitionKeys[i].first ||
                                candidateRUs[i].second != partitionKeys[i].second ||
                                candidateUsers[i] != partitionUsers[i]) {
                                match = false;
                                break;
                            }
                        }
                        if (match) {
                            selectedCode = code;
                            found = true;
                            break;
                        }
                    }
                }
            }
            // If the subchannel has no active RUs, it is punctured/empty -> code 113
            if (!found && partitionKeys.empty()) {
                selectedCode = 113;
                found = true;
            }
            if (!found) {
                result.error = "No valid standard HE-SIG-B RU allocation code found for the partition";
                return result;
            }
            cc.ruAllocationSubfields.push_back(selectedCode);
        }
    }

    result.commonField.rus = rus;
    result.valid = true;
    return result;
}

inline Ieee80211HeSigBCommonFieldResult decodeHeSigBCommonField(
        const Ieee80211HeSigBCommonField& commonField, Hz channelCenterFrequency,
        Hz channelBandwidth)
{
    Ieee80211HeSigBCommonFieldResult result;
    auto centeredCatalog = getHeRuAllocationCatalog(channelCenterFrequency, channelBandwidth);
    auto subchannelRUs = centeredCatalog;
    subchannelRUs.erase(std::remove_if(subchannelRUs.begin(), subchannelRUs.end(),
        [](const Ieee80211HeRu& ru) { return ru.toneSize != 242; }), subchannelRUs.end());
    std::sort(subchannelRUs.begin(), subchannelRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
        return a.toneOffset < b.toneOffset;
    });

    int K = subchannelRUs.size();
    std::vector<Ieee80211HeRu> resolvedRUs;

    // We keep track of wide RUs to avoid duplicate instantiation and sum their user counts
    struct WideRuState {
        Ieee80211HeRu ru;
        int n1 = 0;
        int n2 = 0;
    };
    std::vector<WideRuState> wideRus;

    for (int s = 0; s < K; ++s) {
        int c = s % 2;
        int f = s / 2;
        if (c >= (int)commonField.contentChannels.size() || f >= (int)commonField.contentChannels[c].ruAllocationSubfields.size()) {
            result.error = "HE-SIG-B common field content channels index overflow";
            return result;
        }
        uint8_t code = commonField.contentChannels[c].ruAllocationSubfields[f];
        if (code == 115) {
            // Continuation of 996/1992-tone RU, already resolved.
            continue;
        }

        std::vector<std::pair<int, int>> decodedRUs;
        std::vector<int> decodedUserCounts;
        if (!decodeTable27_27(code, decodedRUs, decodedUserCounts)) {
            result.error = "Reserved or invalid RU allocation code in HE-SIG-B Common field";
            return result;
        }

        for (size_t i = 0; i < decodedRUs.size(); ++i) {
            int toneSize = decodedRUs[i].first;
            int relOffset = decodedRUs[i].second;
            int uCount = decodedUserCounts[i];

            if (toneSize > 242) {
                // Wide RU. Check if we already registered it.
                auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&](const Ieee80211HeRu& candidate) {
                    return candidate.toneSize == toneSize && candidate.toneOffset <= subchannelRUs[s].toneOffset &&
                           candidate.toneOffset + candidate.toneSize >= subchannelRUs[s].toneOffset + 242;
                });
                if (it == centeredCatalog.end()) {
                    result.error = "Wide RU cannot be resolved in centered catalog";
                    return result;
                }
                auto wideIt = std::find_if(wideRus.begin(), wideRus.end(), [&](const WideRuState& state) {
                    return state.ru.toneOffset == it->toneOffset && state.ru.toneSize == it->toneSize;
                });
                if (wideIt == wideRus.end()) {
                    WideRuState state;
                    state.ru = *it;
                    if (c == 0) state.n1 = uCount;
                    else state.n2 = uCount;
                    wideRus.push_back(state);
                } else {
                    if (c == 0) wideIt->n1 = uCount;
                    else wideIt->n2 = uCount;
                }
            } else {
                int absOffset = subchannelRUs[s].toneOffset + relOffset;
                auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&](const Ieee80211HeRu& candidate) {
                    return candidate.toneSize == toneSize && candidate.toneOffset == absOffset;
                });
                if (it == centeredCatalog.end()) {
                    result.error = "RU cannot be resolved in centered catalog";
                    return result;
                }
                // Add one instance of the RU per multiplexed user
                for (int u = 0; u < uCount; ++u) {
                    resolvedRUs.push_back(*it);
                }
            }
        }
    }

    // Add resolved wide RUs
    for (const auto& state : wideRus) {
        int totalUsers = state.n1 + state.n2;
        for (int u = 0; u < totalUsers; ++u) {
            resolvedRUs.push_back(state.ru);
        }
    }

    // Add center 26-tone RUs if present
    for (size_t c = 0; c < commonField.contentChannels.size(); ++c) {
        if (commonField.contentChannels[c].hasCenterRu) {
            int targetOffset = (c == 0) ? 485 : 1481;
            auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&](const Ieee80211HeRu& candidate) {
                return candidate.toneSize == 26 && candidate.toneOffset == targetOffset;
            });
            if (it != centeredCatalog.end()) {
                resolvedRUs.push_back(*it);
            }
        }
    }

    std::sort(resolvedRUs.begin(), resolvedRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
        return a.toneOffset < b.toneOffset;
    });

    result.commonField.rus = resolvedRUs;
    result.commonField.contentChannels = commonField.contentChannels;
    result.valid = true;
    return result;
}

/** Encodes a validated HE RU layout using its canonical allocation-catalog indices. */
inline Ieee80211HeSigCodecResult encodeHeSigBRuAllocation(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth)
{
    Ieee80211HeSigCodecResult result;
    auto commonFieldResult = encodeHeSigBCommonField(rus, channelBandwidth);
    if (!commonFieldResult) {
        result.error = commonFieldResult.error;
        return result;
    }
    result.allocation.rus = commonFieldResult.commonField.rus;
    for (const auto& cc : commonFieldResult.commonField.contentChannels) {
        for (uint8_t code : cc.ruAllocationSubfields) {
            result.allocation.allocationCodes.push_back(code);
        }
    }
    result.valid = true;
    return result;
}

/** Decodes HE-SIG-B allocation codes into channel-centered RU descriptions. */
inline Ieee80211HeSigCodecResult decodeHeSigBRuAllocation(
        const std::vector<uint8_t>& allocationCodes, Hz channelCenterFrequency,
        Hz channelBandwidth)
{
    Ieee80211HeSigCodecResult result;
    int numContentChannels = (channelBandwidth > Hz(20e6)) ? 2 : 1;
    int N = (channelBandwidth >= Hz(160e6)) ? 4 : (channelBandwidth >= Hz(80e6)) ? 2 : 1;
    
    // Check if the size of allocationCodes corresponds to a structured Common Field
    if ((int)allocationCodes.size() == numContentChannels * N) {
        Ieee80211HeSigBCommonField commonField;
        commonField.contentChannels.resize(numContentChannels);
        int idx = 0;
        for (int c = 0; c < numContentChannels; ++c) {
            for (int f = 0; f < N; ++f) {
                commonField.contentChannels[c].ruAllocationSubfields.push_back(allocationCodes[idx++]);
            }
        }
        auto decodedCF = decodeHeSigBCommonField(commonField, channelCenterFrequency, channelBandwidth);
        if (decodedCF) {
            result.allocation.rus = decodedCF.commonField.rus;
            result.allocation.allocationCodes = allocationCodes;
            result.valid = true;
            return result;
        }
    }

    // Fallback: Legacy flat-catalog decoding logic for compatibility
    auto zeroCenteredCatalog = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    auto centeredCatalog = getHeRuAllocationCatalog(channelCenterFrequency, channelBandwidth);
    for (uint8_t code : allocationCodes) {
        if (code >= zeroCenteredCatalog.size()) {
            result.error = "reserved HE-SIG-B RU allocation code";
            return result;
        }
        const auto& encoded = zeroCenteredCatalog[code];
        auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&] (const auto& candidate) {
            return candidate.toneSize == encoded.toneSize &&
                    candidate.toneOffset == encoded.toneOffset;
        });
        if (it == centeredCatalog.end()) {
            result.error = "HE-SIG-B RU allocation cannot be resolved";
            return result;
        }
        result.allocation.rus.push_back(*it);
        result.allocation.allocationCodes.push_back(code);
    }
    if (!validateHeRuLayout(result.allocation.rus, channelBandwidth)) {
        result.error = "decoded HE-SIG-B RU allocation overlaps";
        result.allocation = {};
        return result;
    }
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet

#endif
