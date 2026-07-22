//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HETXVECTOR_H
#define __INET_IEEE80211HETXVECTOR_H

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace physicallayer {

inline bool areIeee80211HeRuParametersEqual(const Ieee80211HeRu& left, const Ieee80211HeRu& right)
{
    const bool equalCenterFrequency = left.centerFrequency == right.centerFrequency ||
            (std::isnan(left.centerFrequency.get()) && std::isnan(right.centerFrequency.get()));
    const bool equalBandwidth = left.bandwidth == right.bandwidth ||
            (std::isnan(left.bandwidth.get()) && std::isnan(right.bandwidth.get()));
    return left.index == right.index && left.toneSize == right.toneSize &&
            left.toneOffset == right.toneOffset && left.dataSubcarriers == right.dataSubcarriers &&
            left.pilotSubcarriers == right.pilotSubcarriers && equalCenterFrequency && equalBandwidth;
}

inline bool areIeee80211HeCommonParametersEqual(const Ieee80211HeCommonPhyParameters& left,
        const Ieee80211HeCommonPhyParameters& right)
{
    return left.ppduFormat == right.ppduFormat && left.ndp == right.ndp &&
            left.channelBandwidth == right.channelBandwidth && left.guardInterval == right.guardInterval &&
            left.ltfType == right.ltfType && left.numberOfHeLtfSymbols == right.numberOfHeLtfSymbols &&
            left.ldpcExtraSymbol == right.ldpcExtraSymbol &&
            left.packetExtensionDurationUs == right.packetExtensionDurationUs &&
            left.sigA.ppduFormat == right.sigA.ppduFormat && left.sigA.bssColor == right.sigA.bssColor &&
            left.sigA.uplink == right.sigA.uplink && left.sigA.txopDurationUs == right.sigA.txopDurationUs &&
            left.sigA.doppler == right.sigA.doppler && left.sigA.stbc == right.sigA.stbc &&
            left.sigB.compression == right.sigB.compression && left.sigB.mcs == right.sigB.mcs &&
            left.sigB.numberOfSymbols == right.sigB.numberOfSymbols &&
            left.sigB.commonFieldBits == right.sigB.commonFieldBits &&
            left.sigB.userFieldBits == right.sigB.userFieldBits &&
            left.legacyPreambleDuration == right.legacyPreambleDuration &&
            left.rlSigDuration == right.rlSigDuration && left.heSigADuration == right.heSigADuration &&
            left.heSigBDuration == right.heSigBDuration && left.heStfDuration == right.heStfDuration &&
            left.heLtfDuration == right.heLtfDuration &&
            left.commonPreambleDuration == right.commonPreambleDuration;
}

inline bool areIeee80211HeUserParametersEqual(const Ieee80211HeUserPhyParameters& left,
        const Ieee80211HeUserPhyParameters& right)
{
    return areIeee80211HeRuParametersEqual(left.ru, right.ru) && left.mcs == right.mcs &&
            left.numberOfSpatialStreams == right.numberOfSpatialStreams && left.dcm == right.dcm &&
            left.guardInterval == right.guardInterval && left.coding == right.coding &&
            left.psduLength == right.psduLength && left.streamStartIndex == right.streamStartIndex &&
            left.staId == right.staId && left.numberOfEncoders == right.numberOfEncoders &&
            left.codedBitsPerSymbol == right.codedBitsPerSymbol &&
            left.dataBitsPerSymbol == right.dataBitsPerSymbol && left.serviceBits == right.serviceBits &&
            left.tailBits == right.tailBits && left.ldpcCodewordLength == right.ldpcCodewordLength &&
            left.ldpcCodewordCount == right.ldpcCodewordCount &&
            left.ldpcShorteningBits == right.ldpcShorteningBits &&
            left.ldpcRepetitionBits == right.ldpcRepetitionBits &&
            left.preFecPaddingFactor == right.preFecPaddingFactor &&
            left.postFecPaddingBits == right.postFecPaddingBits &&
            left.numberOfDataSymbols == right.numberOfDataSymbols &&
            left.numberOfSymbols == right.numberOfSymbols &&
            left.preambleDuration == right.preambleDuration && left.headerDuration == right.headerDuration &&
            left.dataDuration == right.dataDuration && left.duration == right.duration;
}

class Ieee80211HeTxVectorFactory;

/** Caller-controlled inputs for one user of an HE TXVECTOR; no calculated fields are accepted. */
struct Ieee80211HeUserTxVectorRequest
{
    Ieee80211HeRu ru;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    int streamStartIndex = 0;
    bool dcm = false;
    Ieee80211HeCoding coding = HE_CODING_BCC;
    B psduLength = B(0);
    uint16_t staId = 0;
};

/** Caller-controlled common and per-user inputs for construction of an HE TXVECTOR. */
struct Ieee80211HeTxVectorRequest
{
    Hz centerFrequency = Hz(NaN);
    Hz channelBandwidth = Hz(NaN);
    Ieee80211HePpduFormat ppduFormat = HE_MU_DOWNLINK;
    Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US;
    Ieee80211HeLtfType ltfType = HE_LTF_4X;
    int packetExtensionDurationUs = 0;
    bool enforceDurationLimit = true;
    std::vector<Ieee80211HeUserTxVectorRequest> users;
};

/** Immutable validated common portion of an IEEE 802.11 HE TXVECTOR. */
class INET_API Ieee80211HeCommonTxVector final
{
  private:
    const Hz centerFrequency;
    const Ieee80211HeCommonPhyParameters parameters;

    Ieee80211HeCommonTxVector(Hz centerFrequency,
            const Ieee80211HeCommonPhyParameters& parameters) :
        centerFrequency(centerFrequency), parameters(parameters) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    Ieee80211HeCommonTxVector(const Ieee80211HeCommonTxVector&) = default;
    Ieee80211HeCommonTxVector(Ieee80211HeCommonTxVector&&) = default;
    Ieee80211HeCommonTxVector& operator=(const Ieee80211HeCommonTxVector&) = delete;

    Hz getCenterFrequency() const { return centerFrequency; }
    const Ieee80211HeCommonPhyParameters& getParameters() const { return parameters; }

    bool operator==(const Ieee80211HeCommonTxVector& other) const
    {
        return centerFrequency == other.centerFrequency &&
                areIeee80211HeCommonParametersEqual(parameters, other.parameters);
    }
};

/** Immutable validated per-user portion of an IEEE 802.11 HE TXVECTOR. */
class INET_API Ieee80211HeUserTxVector final
{
  private:
    const Ieee80211HeUserPhyParameters parameters;

    explicit Ieee80211HeUserTxVector(const Ieee80211HeUserPhyParameters& parameters) : parameters(parameters) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    Ieee80211HeUserTxVector(const Ieee80211HeUserTxVector&) = default;
    Ieee80211HeUserTxVector(Ieee80211HeUserTxVector&&) = default;
    Ieee80211HeUserTxVector& operator=(const Ieee80211HeUserTxVector&) = delete;

    const Ieee80211HeUserPhyParameters& getParameters() const { return parameters; }

    bool operator==(const Ieee80211HeUserTxVector& other) const
    {
        return areIeee80211HeUserParametersEqual(parameters, other.parameters);
    }
};

/** Complete immutable, validated IEEE 802.11 HE TXVECTOR. */
class INET_API Ieee80211HeTxVector final
{
  private:
    const Ieee80211HeCommonTxVector common;
    const std::vector<Ieee80211HeUserTxVector> users;

    Ieee80211HeTxVector(Ieee80211HeCommonTxVector common,
            std::vector<Ieee80211HeUserTxVector> users) :
        common(std::move(common)), users(std::move(users)) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    Ieee80211HeTxVector(const Ieee80211HeTxVector&) = default;
    Ieee80211HeTxVector(Ieee80211HeTxVector&&) = default;
    Ieee80211HeTxVector& operator=(const Ieee80211HeTxVector&) = delete;

    const Ieee80211HeCommonTxVector& getCommon() const { return common; }
    const std::vector<Ieee80211HeUserTxVector>& getUsers() const { return users; }

    bool operator==(const Ieee80211HeTxVector& other) const
    {
        return common == other.common && users == other.users;
    }
};

/** Immutable calculated timing and symbol summary; logical field offsets remain a Gate 2 concern. */
class INET_API Ieee80211HePpduTimingLayout final
{
  private:
    const Ieee80211HeCommonPhyParameters common;
    const std::vector<Ieee80211HeUserPhyParameters> users;
    const int commonNumberOfDataSymbols;
    const simtime_t duration;

    explicit Ieee80211HePpduTimingLayout(const Ieee80211HePpduParameters& parameters) :
        common(parameters.common), users(parameters.users),
        commonNumberOfDataSymbols(parameters.commonNumberOfDataSymbols), duration(parameters.duration) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    Ieee80211HePpduTimingLayout(const Ieee80211HePpduTimingLayout&) = default;
    Ieee80211HePpduTimingLayout(Ieee80211HePpduTimingLayout&&) = default;
    Ieee80211HePpduTimingLayout& operator=(const Ieee80211HePpduTimingLayout&) = delete;

    Ieee80211HePpduFormat getPpduFormat() const { return common.ppduFormat; }
    Hz getChannelBandwidth() const { return common.channelBandwidth; }
    Ieee80211HeGuardInterval getGuardInterval() const { return common.guardInterval; }
    Ieee80211HeLtfType getLtfType() const { return common.ltfType; }
    int getNumberOfHeLtfSymbols() const { return common.numberOfHeLtfSymbols; }
    simtime_t getLegacyPreambleDuration() const { return common.legacyPreambleDuration; }
    simtime_t getRlSigDuration() const { return common.rlSigDuration; }
    simtime_t getHeSigADuration() const { return common.heSigADuration; }
    simtime_t getHeSigBDuration() const { return common.heSigBDuration; }
    simtime_t getHeStfDuration() const { return common.heStfDuration; }
    simtime_t getHeLtfDuration() const { return common.heLtfDuration; }
    simtime_t getCommonPreambleDuration() const { return common.commonPreambleDuration; }
    int getCommonNumberOfDataSymbols() const { return commonNumberOfDataSymbols; }
    int getPacketExtensionDurationUs() const { return common.packetExtensionDurationUs; }
    simtime_t getDuration() const { return duration; }
    const Ieee80211HeCommonPhyParameters& getCommon() const { return common; }
    const std::vector<Ieee80211HeUserPhyParameters>& getUsers() const { return users; }

    bool matches(const Ieee80211HePpduParameters& parameters) const
    {
        if (!areIeee80211HeCommonParametersEqual(common, parameters.common) ||
                commonNumberOfDataSymbols != parameters.commonNumberOfDataSymbols ||
                duration != parameters.duration || users.size() != parameters.users.size())
            return false;
        for (size_t i = 0; i < users.size(); i++)
            if (!areIeee80211HeUserParametersEqual(users[i], parameters.users[i]))
                return false;
        return true;
    }

    bool operator==(const Ieee80211HePpduTimingLayout& other) const
    {
        Ieee80211HePpduParameters parameters;
        parameters.common = other.common;
        parameters.users = other.users;
        parameters.commonNumberOfDataSymbols = other.commonNumberOfDataSymbols;
        parameters.duration = other.duration;
        return matches(parameters);
    }
};

/** Malformed-input validation outcome. Invalid outcomes contain neither a vector nor a timing layout. */
class INET_API Ieee80211HeTxVectorValidationResult final
{
  private:
    Ieee80211HeValidationErrorCode errorCode = Ieee80211HeValidationErrorCode::NONE;
    Ieee80211HeValidationContext context;
    std::shared_ptr<const Ieee80211HeTxVector> txVector;
    std::shared_ptr<const Ieee80211HePpduTimingLayout> timingLayout;

    Ieee80211HeTxVectorValidationResult(Ieee80211HeValidationErrorCode errorCode,
            Ieee80211HeValidationContext context) : errorCode(errorCode), context(std::move(context)) {}

    Ieee80211HeTxVectorValidationResult(std::shared_ptr<const Ieee80211HeTxVector> txVector,
            std::shared_ptr<const Ieee80211HePpduTimingLayout> timingLayout) :
        txVector(std::move(txVector)), timingLayout(std::move(timingLayout)) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    explicit operator bool() const
    {
        return errorCode == Ieee80211HeValidationErrorCode::NONE && txVector && timingLayout;
    }

    Ieee80211HeValidationErrorCode getErrorCode() const { return errorCode; }
    const Ieee80211HeValidationContext& getContext() const { return context; }
    const std::shared_ptr<const Ieee80211HeTxVector>& getTxVector() const { return txVector; }
    const std::shared_ptr<const Ieee80211HePpduTimingLayout>& getTimingLayout() const { return timingLayout; }
};

/**
 * The only public construction path for canonical HE TXVECTOR/timing objects.
 * Ordinary malformed inputs return diagnostics; allocation failures are not a noexcept guarantee.
 */
class INET_API Ieee80211HeTxVectorFactory final
{
  private:
    static Ieee80211HeTxVectorValidationResult makeError(Ieee80211HeValidationErrorCode errorCode,
            const char *fieldName, const std::string& detail,
            std::optional<size_t> userIndex = {}, std::optional<size_t> physicalRuIndex = {})
    {
        Ieee80211HeValidationContext context;
        context.userIndex = userIndex;
        context.physicalRuIndex = physicalRuIndex;
        context.fieldName = fieldName;
        context.detail = detail;
        return Ieee80211HeTxVectorValidationResult(errorCode, std::move(context));
    }

    static bool samePhysicalRu(const Ieee80211HeRu& left, const Ieee80211HeRu& right)
    {
        const bool equalCenterFrequency = left.centerFrequency == right.centerFrequency ||
                (std::isnan(left.centerFrequency.get()) && std::isnan(right.centerFrequency.get()));
        return equalCenterFrequency && left.toneSize == right.toneSize && left.toneOffset == right.toneOffset;
    }

  public:
    static Ieee80211HeTxVectorValidationResult create(const Ieee80211HeTxVectorRequest& request)
    {
        try {
            if (!std::isfinite(request.centerFrequency.get()))
                return makeError(Ieee80211HeValidationErrorCode::INVALID_CENTER_FREQUENCY,
                        "centerFrequency", "HE channel center frequency must be finite");
            if (request.channelBandwidth != MHz(20) && request.channelBandwidth != MHz(40) &&
                    request.channelBandwidth != MHz(80) && request.channelBandwidth != MHz(160))
                return makeError(Ieee80211HeValidationErrorCode::INVALID_CHANNEL_BANDWIDTH,
                        "channelBandwidth", "unsupported HE channel bandwidth");

            auto catalog = getHeRuAllocationCatalog(request.centerFrequency, request.channelBandwidth);
            std::vector<Ieee80211HeUserPhyParameters> calculatorUsers;
            calculatorUsers.reserve(request.users.size());
            for (size_t userIndex = 0; userIndex < request.users.size(); userIndex++) {
                const auto& input = request.users[userIndex];
                auto catalogIt = std::find_if(catalog.begin(), catalog.end(),
                        [&] (const Ieee80211HeRu& candidate) {
                            return candidate.index == input.ru.index && candidate.toneSize == input.ru.toneSize &&
                                    candidate.toneOffset == input.ru.toneOffset;
                        });
                if (catalogIt == catalog.end())
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                            "ru", "HE RU is outside the canonical allocation catalog", userIndex);
                if (std::isfinite(input.ru.centerFrequency.get()) &&
                        input.ru.centerFrequency != catalogIt->centerFrequency)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                            "ru.centerFrequency", "HE RU center disagrees with the authoritative channel center",
                            userIndex);
                Ieee80211HeUserPhyParameters user;
                user.ru = *catalogIt;
                user.mcs = input.mcs;
                user.numberOfSpatialStreams = input.numberOfSpatialStreams;
                user.streamStartIndex = input.streamStartIndex;
                user.dcm = input.dcm;
                user.coding = input.coding;
                user.psduLength = input.psduLength;
                user.staId = input.staId;
                calculatorUsers.push_back(user);
            }

            std::vector<Ieee80211HeRu> uniquePhysicalRus;
            for (const auto& user : calculatorUsers) {
                if (std::none_of(uniquePhysicalRus.begin(), uniquePhysicalRus.end(),
                        [&] (const Ieee80211HeRu& ru) { return samePhysicalRu(ru, user.ru); }))
                    uniquePhysicalRus.push_back(user.ru);
            }
            if (!validateHeRuLayout(uniquePhysicalRus, request.channelBandwidth))
                return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                        "users[].ru", "HE physical RU layout is duplicate, overlapping, or out of band");

            auto calculation = computeHePpduParameters(calculatorUsers, request.channelBandwidth,
                    request.ppduFormat, request.guardInterval, request.ltfType,
                    request.packetExtensionDurationUs, request.enforceDurationLimit);
            if (!calculation)
                return Ieee80211HeTxVectorValidationResult(calculation.errorCode, calculation.context);

            Ieee80211HeCommonTxVector commonTx(request.centerFrequency, calculation.parameters.common);
            std::vector<Ieee80211HeUserTxVector> userTx;
            userTx.reserve(calculation.parameters.users.size());
            for (const auto& user : calculation.parameters.users)
                userTx.push_back(Ieee80211HeUserTxVector(user));
            auto txVector = std::shared_ptr<const Ieee80211HeTxVector>(
                    new Ieee80211HeTxVector(std::move(commonTx), std::move(userTx)));
            auto timingLayout = std::shared_ptr<const Ieee80211HePpduTimingLayout>(
                    new Ieee80211HePpduTimingLayout(calculation.parameters));
            return Ieee80211HeTxVectorValidationResult(std::move(txVector), std::move(timingLayout));
        }
        catch (const std::exception& error) {
            return makeError(Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                    "factory", error.what());
        }
        catch (...) {
            return makeError(Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                    "factory", "unknown HE TXVECTOR construction error");
        }
    }
};

} // namespace physicallayer
} // namespace inet

#endif
