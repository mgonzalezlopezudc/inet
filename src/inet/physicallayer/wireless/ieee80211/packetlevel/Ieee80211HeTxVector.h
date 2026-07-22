//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HETXVECTOR_H
#define __INET_IEEE80211HETXVECTOR_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "inet/common/TagBase.h"
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
            left.puncturedSubchannelMask == right.puncturedSubchannelMask &&
            left.lSigLength == right.lSigLength &&
            left.noSignalExtension == right.noSignalExtension &&
            left.signalExtensionNs == right.signalExtensionNs &&
            left.ltfType == right.ltfType && left.numberOfHeLtfSymbols == right.numberOfHeLtfSymbols &&
            left.ldpcExtraSymbol == right.ldpcExtraSymbol &&
            left.packetExtensionDurationUs == right.packetExtensionDurationUs &&
            left.sigA.ppduFormat == right.sigA.ppduFormat && left.sigA.bssColor == right.sigA.bssColor &&
            left.sigA.uplink == right.sigA.uplink &&
            left.sigA.txopUnspecified == right.sigA.txopUnspecified &&
            left.sigA.txopDurationUs == right.sigA.txopDurationUs &&
            left.sigA.doppler == right.sigA.doppler &&
            left.sigA.midamblePeriodicity == right.sigA.midamblePeriodicity &&
            left.sigA.stbc == right.sigA.stbc &&
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
            left.ndpFeedbackReport == right.ndpFeedbackReport &&
            left.ndpFeedbackStatus == right.ndpFeedbackStatus &&
            left.ndpRuToneSetIndex == right.ndpRuToneSetIndex &&
            left.ndpStartingStsNumber == right.ndpStartingStsNumber &&
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
class Ieee80211HeRxVectorFactory;

/** Caller-controlled inputs for one user of an HE TXVECTOR; no calculated fields are accepted. */
struct Ieee80211HeUserTxVectorRequest
{
    Ieee80211HeRu ru;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    int streamStartIndex = 0;
    bool dcm = false;
    bool ndpFeedbackReport = false;
    uint8_t ndpFeedbackStatus = 0;
    uint8_t ndpRuToneSetIndex = 0;
    uint8_t ndpStartingStsNumber = 0;
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
    uint8_t puncturedSubchannelMask = 0;
    uint16_t lSigLength = 0; // Trigger Common Info UL Length for HE TB; calculated for HE MU
    bool noSignalExtension = false;
    simtime_t requestedTxTime = SIMTIME_ZERO;
    bool requestedTxTimeExact = false;
    bool ldpcExtraSymbolSegment = false;
    bool ndp = false;
    uint8_t bssColor = 0;
    bool uplink = false;
    Ieee80211HeTxopDuration txopDuration;
    bool doppler = false;
    uint8_t midamblePeriodicity = 0;
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
    const Ieee80211HeTxopDuration requestedTxopDuration;

    Ieee80211HeCommonTxVector(Hz centerFrequency,
            const Ieee80211HeCommonPhyParameters& parameters,
            Ieee80211HeTxopDuration requestedTxopDuration) :
        centerFrequency(centerFrequency), parameters(parameters),
        requestedTxopDuration(requestedTxopDuration) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    Ieee80211HeCommonTxVector(const Ieee80211HeCommonTxVector&) = default;
    Ieee80211HeCommonTxVector(Ieee80211HeCommonTxVector&&) = default;
    Ieee80211HeCommonTxVector& operator=(const Ieee80211HeCommonTxVector&) = delete;

    Hz getCenterFrequency() const { return centerFrequency; }
    const Ieee80211HeCommonPhyParameters& getParameters() const { return parameters; }
    const Ieee80211HeTxopDuration& getRequestedTxopDuration() const { return requestedTxopDuration; }

    bool operator==(const Ieee80211HeCommonTxVector& other) const
    {
        return centerFrequency == other.centerFrequency &&
                areIeee80211HeCommonParametersEqual(parameters, other.parameters) &&
                requestedTxopDuration == other.requestedTxopDuration;
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

/** Ordered packet-level HE PPDU fields from Figures 27-8 through 27-11. */
enum class Ieee80211HePpduField {
    L_STF,
    L_LTF,
    L_SIG,
    RL_SIG,
    HE_SIG_A,
    HE_SIG_B,
    HE_STF,
    HE_LTF,
    DATA,
    PE,
    SIGNAL_EXTENSION,
};

/**
 * Immutable half-open temporal span [startOffset, endOffset) relative to PPDU start.
 * These are time-domain model offsets, not coded over-the-air bit offsets.
 */
class INET_API Ieee80211HePpduFieldSpan final
{
  private:
    const Ieee80211HePpduField field;
    const simtime_t startOffset;
    const simtime_t endOffset;

    Ieee80211HePpduFieldSpan(Ieee80211HePpduField field, simtime_t startOffset,
            simtime_t endOffset) : field(field), startOffset(startOffset), endOffset(endOffset) {}

    friend class Ieee80211HePpduLayout;

  public:
    Ieee80211HePpduFieldSpan(const Ieee80211HePpduFieldSpan&) = default;
    Ieee80211HePpduFieldSpan(Ieee80211HePpduFieldSpan&&) = default;
    Ieee80211HePpduFieldSpan& operator=(const Ieee80211HePpduFieldSpan&) = delete;

    Ieee80211HePpduField getField() const { return field; }
    simtime_t getStartOffset() const { return startOffset; }
    simtime_t getEndOffset() const { return endOffset; }
    simtime_t getDuration() const { return endOffset - startOffset; }

    bool operator==(const Ieee80211HePpduFieldSpan& other) const
    {
        return field == other.field && startOffset == other.startOffset && endOffset == other.endOffset;
    }
};

/**
 * Immutable half-open PSDU bit range [startBitOffset, endBitOffset) in the packet-model
 * DATA container. This metadata describes stable user concatenation only; it is not a
 * coded or interleaved over-the-air PHY bit position.
 */
class INET_API Ieee80211HeModelPsduBitRange final
{
  private:
    const size_t userIndex;
    const uint16_t staId;
    const b startBitOffset;
    const b endBitOffset;

    Ieee80211HeModelPsduBitRange(size_t userIndex, uint16_t staId, b startBitOffset,
            b endBitOffset) : userIndex(userIndex), staId(staId),
        startBitOffset(startBitOffset), endBitOffset(endBitOffset) {}

    friend class Ieee80211HePpduLayout;

  public:
    Ieee80211HeModelPsduBitRange(const Ieee80211HeModelPsduBitRange&) = default;
    Ieee80211HeModelPsduBitRange(Ieee80211HeModelPsduBitRange&&) = default;
    Ieee80211HeModelPsduBitRange& operator=(const Ieee80211HeModelPsduBitRange&) = delete;

    size_t getUserIndex() const { return userIndex; }
    uint16_t getStaId() const { return staId; }
    b getStartBitOffset() const { return startBitOffset; }
    b getEndBitOffset() const { return endBitOffset; }
    b getBitLength() const { return endBitOffset - startBitOffset; }

    bool operator==(const Ieee80211HeModelPsduBitRange& other) const
    {
        return userIndex == other.userIndex && staId == other.staId &&
                startBitOffset == other.startBitOffset && endBitOffset == other.endBitOffset;
    }
};

/**
 * Canonical immutable calculated HE PPDU temporal and packet-container layout.
 * DATA is omitted for an NDP and present otherwise. PE is always the terminal
 * temporal field, including when its duration is zero.
 */
class INET_API Ieee80211HePpduLayout final
{
  private:
    const Hz centerFrequency;
    const Ieee80211HeCommonPhyParameters common;
    const std::vector<Ieee80211HeUserPhyParameters> users;
    const int commonNumberOfDataSymbols;
    const simtime_t duration;
    const std::vector<Ieee80211HePpduFieldSpan> fieldSpans;
    const std::vector<Ieee80211HeModelPsduBitRange> psduBitRanges;

    static std::vector<Ieee80211HePpduFieldSpan> makeFieldSpans(
            const Ieee80211HePpduParameters& parameters)
    {
        std::vector<Ieee80211HePpduFieldSpan> spans;
        spans.reserve(11);
        simtime_t offset = SIMTIME_ZERO;
        auto append = [&] (Ieee80211HePpduField field, simtime_t fieldDuration) {
            spans.emplace_back(Ieee80211HePpduFieldSpan(field, offset, offset + fieldDuration));
            offset += fieldDuration;
        };
        append(Ieee80211HePpduField::L_STF, SimTime(8, SIMTIME_US));
        append(Ieee80211HePpduField::L_LTF, SimTime(8, SIMTIME_US));
        append(Ieee80211HePpduField::L_SIG, SimTime(4, SIMTIME_US));
        append(Ieee80211HePpduField::RL_SIG, parameters.common.rlSigDuration);
        append(Ieee80211HePpduField::HE_SIG_A, parameters.common.heSigADuration);
        if (parameters.common.ppduFormat == HE_MU_DOWNLINK)
            append(Ieee80211HePpduField::HE_SIG_B, parameters.common.heSigBDuration);
        append(Ieee80211HePpduField::HE_STF, parameters.common.heStfDuration);
        append(Ieee80211HePpduField::HE_LTF, parameters.common.heLtfDuration);
        const auto packetExtensionDuration = SimTime(parameters.common.packetExtensionDurationUs, SIMTIME_US);
        const auto signalExtensionDuration = SimTime(parameters.common.signalExtensionNs, SIMTIME_NS);
        if (!parameters.common.ndp)
            append(Ieee80211HePpduField::DATA,
                    parameters.duration - parameters.common.commonPreambleDuration -
                    packetExtensionDuration - signalExtensionDuration);
        append(Ieee80211HePpduField::PE, packetExtensionDuration);
        if (signalExtensionDuration > SIMTIME_ZERO)
            append(Ieee80211HePpduField::SIGNAL_EXTENSION, signalExtensionDuration);
        return spans;
    }

    static std::vector<Ieee80211HeModelPsduBitRange> makePsduBitRanges(
            const Ieee80211HePpduParameters& parameters)
    {
        std::vector<Ieee80211HeModelPsduBitRange> ranges;
        if (parameters.common.ndp)
            return ranges;
        ranges.reserve(parameters.users.size());
        int64_t bitOffset = 0;
        for (size_t userIndex = 0; userIndex < parameters.users.size(); userIndex++) {
            const auto& user = parameters.users[userIndex];
            const int64_t bitLength = user.psduLength.get<B>() * 8;
            ranges.emplace_back(Ieee80211HeModelPsduBitRange(userIndex, user.staId,
                    b(bitOffset), b(bitOffset + bitLength)));
            bitOffset += bitLength;
        }
        return ranges;
    }

    Ieee80211HePpduLayout(Hz centerFrequency, const Ieee80211HePpduParameters& parameters) :
        centerFrequency(centerFrequency), common(parameters.common), users(parameters.users),
        commonNumberOfDataSymbols(parameters.commonNumberOfDataSymbols), duration(parameters.duration),
        fieldSpans(makeFieldSpans(parameters)), psduBitRanges(makePsduBitRanges(parameters)) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    Ieee80211HePpduLayout(const Ieee80211HePpduLayout&) = default;
    Ieee80211HePpduLayout(Ieee80211HePpduLayout&&) = default;
    Ieee80211HePpduLayout& operator=(const Ieee80211HePpduLayout&) = delete;

    Hz getCenterFrequency() const { return centerFrequency; }
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
    bool isNdp() const { return common.ndp; }
    simtime_t getDuration() const { return duration; }
    const Ieee80211HeCommonPhyParameters& getCommon() const { return common; }
    const std::vector<Ieee80211HeUserPhyParameters>& getUsers() const { return users; }
    const std::vector<Ieee80211HePpduFieldSpan>& getFieldSpans() const { return fieldSpans; }
    const std::vector<Ieee80211HeModelPsduBitRange>& getPsduBitRanges() const { return psduBitRanges; }

    bool matches(const Ieee80211HeTxVector& txVector) const
    {
        if (centerFrequency != txVector.getCommon().getCenterFrequency() ||
                !areIeee80211HeCommonParametersEqual(common, txVector.getCommon().getParameters()) ||
                users.size() != txVector.getUsers().size())
            return false;
        for (size_t i = 0; i < users.size(); i++)
            if (!areIeee80211HeUserParametersEqual(users[i], txVector.getUsers()[i].getParameters()))
                return false;
        return true;
    }

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

    bool operator==(const Ieee80211HePpduLayout& other) const
    {
        return centerFrequency == other.centerFrequency &&
                areIeee80211HeCommonParametersEqual(common, other.common) && users.size() == other.users.size() &&
                commonNumberOfDataSymbols == other.commonNumberOfDataSymbols && duration == other.duration &&
                fieldSpans == other.fieldSpans && psduBitRanges == other.psduBitRanges &&
                std::equal(users.begin(), users.end(), other.users.begin(),
                        [] (const auto& left, const auto& right) {
                            return areIeee80211HeUserParametersEqual(left, right);
                        });
    }
};

/** Malformed-input validation outcome. Invalid outcomes contain neither a vector nor a PPDU layout. */
class INET_API Ieee80211HeTxVectorValidationResult final
{
  private:
    Ieee80211HeValidationErrorCode errorCode = Ieee80211HeValidationErrorCode::NONE;
    Ieee80211HeValidationContext context;
    std::shared_ptr<const Ieee80211HeTxVector> txVector;
    std::shared_ptr<const Ieee80211HePpduLayout> ppduLayout;

    Ieee80211HeTxVectorValidationResult(Ieee80211HeValidationErrorCode errorCode,
            Ieee80211HeValidationContext context) : errorCode(errorCode), context(std::move(context)) {}

    Ieee80211HeTxVectorValidationResult(std::shared_ptr<const Ieee80211HeTxVector> txVector,
            std::shared_ptr<const Ieee80211HePpduLayout> ppduLayout) :
        txVector(std::move(txVector)), ppduLayout(std::move(ppduLayout)) {}

    friend class Ieee80211HeTxVectorFactory;

  public:
    explicit operator bool() const
    {
        return errorCode == Ieee80211HeValidationErrorCode::NONE && txVector && ppduLayout;
    }

    Ieee80211HeValidationErrorCode getErrorCode() const { return errorCode; }
    const Ieee80211HeValidationContext& getContext() const { return context; }
    const std::shared_ptr<const Ieee80211HeTxVector>& getTxVector() const { return txVector; }
    const std::shared_ptr<const Ieee80211HePpduLayout>& getPpduLayout() const { return ppduLayout; }
};

/**
 * The only public construction path for canonical HE TXVECTOR/PPDU-layout objects.
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
            if (request.bssColor > 63)
                return makeError(Ieee80211HeValidationErrorCode::INVALID_BSS_COLOR,
                        "bssColor", "HE BSS color exceeds the 6-bit field width");
            std::vector<bool> puncturedSubchannels(
                    std::lround(request.channelBandwidth.get() / 20e6), false);
            for (size_t i = 0; i < puncturedSubchannels.size(); ++i)
                puncturedSubchannels[i] = request.puncturedSubchannelMask & (uint8_t(1) << i);
            if (request.puncturedSubchannelMask >> puncturedSubchannels.size())
                return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                        "puncturedSubchannelMask", "HE puncturing mask exceeds the channel bandwidth");
            if ((request.txopDuration.unspecified && request.txopDuration.durationUs != 0) ||
                    (!request.txopDuration.unspecified && request.txopDuration.durationUs > 8448))
                return makeError(Ieee80211HeValidationErrorCode::INVALID_TXOP_DURATION,
                        "txopDuration", "HE TXOP duration must be unspecified or between 0 and 8448 us");
            if (request.doppler || request.midamblePeriodicity != 0)
                return makeError(Ieee80211HeValidationErrorCode::UNSUPPORTED_DOPPLER_TIMING,
                        "doppler", "HE Doppler is deferred until N_MA midamble timing is modeled");
            if (request.ndp) {
                if (request.ppduFormat != HE_SINGLE_USER &&
                        request.ppduFormat != HE_TRIGGER_BASED_UPLINK)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PPDU_FORMAT,
                            "ndp", "the modeled HE NDP formats are HE SU sounding and HE TB feedback");
                if (request.ppduFormat == HE_SINGLE_USER &&
                        !((request.ltfType == HE_LTF_2X &&
                           (request.guardInterval == HE_GI_0_8_US || request.guardInterval == HE_GI_1_6_US)) ||
                          (request.ltfType == HE_LTF_4X && request.guardInterval == HE_GI_3_2_US)))
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_GI_LTF_COMBINATION,
                            "guardInterval/ltfType", "HE sounding NDP requires 2x HE-LTF with 0.8/1.6 us GI or 4x HE-LTF with 3.2 us GI");
                if (!std::all_of(request.users.begin(), request.users.end(),
                        [] (const auto& user) { return user.psduLength == B(0); }))
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                            "users[].psduLength", "an HE NDP must not contain a PSDU");
                if ((request.ppduFormat == HE_SINGLE_USER && request.packetExtensionDurationUs != 4) ||
                        (request.ppduFormat == HE_TRIGGER_BASED_UPLINK &&
                                request.packetExtensionDurationUs != 0))
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PACKET_EXTENSION,
                            "packetExtensionDurationUs", request.ppduFormat == HE_SINGLE_USER ?
                                    "HE SU sounding NDP requires a 4 us packet extension" :
                                    "HE TB feedback NDP requires a zero-duration packet extension");
            }

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
                user.ndpFeedbackReport = input.ndpFeedbackReport;
                user.ndpFeedbackStatus = input.ndpFeedbackStatus;
                user.ndpRuToneSetIndex = input.ndpRuToneSetIndex;
                user.ndpStartingStsNumber = input.ndpStartingStsNumber;
                user.coding = input.coding;
                user.psduLength = input.psduLength;
                user.staId = input.staId;
                calculatorUsers.push_back(user);
            }

            if (request.ndp && request.ppduFormat == HE_TRIGGER_BASED_UPLINK) {
                if (request.guardInterval != HE_GI_3_2_US || request.ltfType != HE_LTF_4X)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_GI_LTF_COMBINATION,
                            "guardInterval/ltfType", "HE TB feedback NDP requires 4x HE-LTF with 3.2 us GI");
                if (calculatorUsers.size() != 1)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_USER_COUNT,
                            "users", "HE TB feedback NDP requires exactly one modeled recipient");
                const auto& user = calculatorUsers.front();
                const int toneSetsPerSpatialStream = 18 *
                        static_cast<int>(request.channelBandwidth.get() / 20e6);
                if (!user.ndpFeedbackReport)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PPDU_FORMAT,
                            "users[].ndpFeedbackReport", "HE TB feedback NDP requires explicit NDP report metadata", 0);
                if (user.ndpFeedbackStatus > 1)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PPDU_FORMAT,
                            "users[].ndpFeedbackStatus", "HE TB feedback status is a one-bit value", 0);
                if (user.ndpRuToneSetIndex < 1 || user.ndpRuToneSetIndex > toneSetsPerSpatialStream)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PPDU_FORMAT,
                            "users[].ndpRuToneSetIndex", "HE TB feedback tone-set index is outside the channel range", 0);
                if (user.ndpStartingStsNumber > 1)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PPDU_FORMAT,
                            "users[].ndpStartingStsNumber", "HE TB feedback starting STS number exceeds the NFRP range", 0);
                if (user.mcs != 0)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_MCS,
                            "mcs", "HE TB feedback NDP requires MCS 0", 0);
                if (user.dcm)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_DCM_COMBINATION,
                            "dcm", "HE TB feedback NDP does not use DCM", 0);
                if (user.coding != HE_CODING_BCC)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_CODING,
                            "coding", "HE TB feedback NDP requires BCC", 0);
                if (user.numberOfSpatialStreams != 1)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_SPATIAL_STREAMS,
                            "numberOfSpatialStreams", "HE TB feedback NDP requires one space-time stream", 0);
                const auto maximumRu = getHeEqualRuLayout(request.centerFrequency,
                        request.channelBandwidth, 1).front();
                if (!samePhysicalRu(user.ru, maximumRu))
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                            "ru", "HE TB feedback NDP requires the maximum RU for the channel", 0);
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

            auto& parameters = calculation.parameters;
            parameters.common.puncturedSubchannelMask = request.puncturedSubchannelMask;
            parameters.common.ldpcExtraSymbol = request.ldpcExtraSymbolSegment;
            parameters.common.noSignalExtension = request.noSignalExtension;
            parameters.common.signalExtensionNs = getIeee80211HeSignalExtensionNs(
                    request.centerFrequency, request.noSignalExtension);
            const auto signalExtensionDuration =
                    SimTime(parameters.common.signalExtensionNs, SIMTIME_NS);
            if (signalExtensionDuration > SIMTIME_ZERO) {
                parameters.duration += signalExtensionDuration;
                for (auto& user : parameters.users)
                    user.duration += signalExtensionDuration;
            }
            if (request.enforceDurationLimit && parameters.duration > SimTime(5.484, SIMTIME_MS))
                return makeError(Ieee80211HeValidationErrorCode::PPDU_DURATION_EXCEEDED,
                        "duration", "HE PPDU including signal extension exceeds the 5.484 ms duration limit");
            if (request.ppduFormat == HE_MU_DOWNLINK) {
                auto lSig = buildHeLSig(Ieee80211HeSigFormat::MU,
                        parameters.duration.inUnit(SIMTIME_NS),
                        parameters.common.signalExtensionNs);
                if (!lSig)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "duration", lSig.error);
                parameters.common.lSigLength = lSig.value.length;
                bool compression = calculatorUsers.size() > 0 &&
                        std::all_of(calculatorUsers.begin(), calculatorUsers.end(), [&] (const auto& user) {
                            return user.ru.toneSize == 1992 &&
                                    user.ru.toneOffset == calculatorUsers.front().ru.toneOffset;
                        });
                if (compression && calculatorUsers.size() > 8)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_USER_COUNT,
                            "users", "compressed HE-SIG-B can signal at most eight MU-MIMO users");
                if (parameters.common.sigB.numberOfSymbols > 16)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_USER_COUNT,
                            "users", "packet-level HE-SIG-B raw decoding cannot resolve a symbol count above the saturated 16-symbol field");
                auto bandwidth = encodeHeMuBandwidth(request.channelBandwidth,
                        puncturedSubchannels, compression);
                if (!bandwidth)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                            "puncturedSubchannelMask", bandwidth.error);
                if (!compression) {
                    std::vector<Ieee80211HeRu> rus;
                    for (const auto& user : calculatorUsers)
                        rus.push_back(user.ru);
                    auto sigBCommon = encodeHeSigBCommonField(rus, request.channelBandwidth,
                            puncturedSubchannels);
                    if (!sigBCommon)
                        return makeError(Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                                "users[].ru", sigBCommon.error);
                }
            }
            else if (request.ppduFormat == HE_TRIGGER_BASED_UPLINK) {
                auto lSig = buildHeTbLSig(request.lSigLength);
                if (!lSig)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "lSigLength", lSig.error);
                parameters.common.lSigLength = lSig.value.length;

                auto upperBound = getIeee80211HeTriggerTxTimeUpperBound(
                        request.lSigLength, parameters.common.signalExtensionNs);
                if (!upperBound)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "lSigLength", upperBound.error);
                const auto symbolDuration = SimTime(12800, SIMTIME_NS) +
                        getHeGuardIntervalDuration(request.guardInterval);
                const auto packetExtensionDuration =
                        SimTime(parameters.common.packetExtensionDurationUs, SIMTIME_US);
                const auto fixedDuration = parameters.common.commonPreambleDuration +
                        packetExtensionDuration + signalExtensionDuration;
                simtime_t targetDuration;
                if (request.requestedTxTimeExact) {
                    if (request.requestedTxTime <= SIMTIME_ZERO)
                        return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                                "requestedTxTime", "an exact HE TB TXTIME must be positive");
                    targetDuration = request.requestedTxTime;
                }
                else if (request.ndp)
                    // An NDP has no Data field that could absorb padding. Its
                    // resolved preamble/PE duration must itself select the
                    // Trigger L_LENGTH bucket.
                    targetDuration = parameters.duration;
                else {
                    // Raw Trigger Common Info recovers only the Equation 27-11
                    // upper boundary. Select the longest HE-symbol-aligned PPDU
                    // inside that bucket so a short PSDU is padded instead of
                    // silently shortening the solicited response.
                    if (upperBound.txTime < fixedDuration)
                        return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                                "lSigLength", "HE Trigger UL Length cannot contain the selected preamble");
                    const auto available = upperBound.txTime - fixedDuration;
                    const int64_t numberOfSymbols = available.inUnit(SIMTIME_NS) /
                            symbolDuration.inUnit(SIMTIME_NS);
                    targetDuration = fixedDuration + numberOfSymbols * symbolDuration;
                }
                if (targetDuration < parameters.duration)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "requestedTxTime", "HE TB payload does not fit the solicited TXTIME");
                if (targetDuration < fixedDuration ||
                        (targetDuration - fixedDuration).inUnit(SIMTIME_NS) %
                                symbolDuration.inUnit(SIMTIME_NS) != 0)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "requestedTxTime", "HE TB TXTIME is not aligned to a complete HE data symbol");
                auto projected = buildHeLSig(Ieee80211HeSigFormat::SU,
                        targetDuration.inUnit(SIMTIME_NS), parameters.common.signalExtensionNs);
                if (!projected || projected.value.length != request.lSigLength)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "lSigLength", "HE TB TXTIME and Trigger UL Length select different L-SIG buckets");
                if (request.enforceDurationLimit && targetDuration > SimTime(5.484, SIMTIME_MS))
                    return makeError(Ieee80211HeValidationErrorCode::PPDU_DURATION_EXCEEDED,
                            "requestedTxTime", "HE TB solicited TXTIME exceeds the 5.484 ms duration limit");
                const int64_t targetNumberOfSymbols =
                        (targetDuration - fixedDuration).inUnit(SIMTIME_NS) /
                        symbolDuration.inUnit(SIMTIME_NS);
                if (targetNumberOfSymbols > std::numeric_limits<int>::max())
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH,
                            "requestedTxTime", "HE TB solicited data-symbol count exceeds the model range");
                parameters.commonNumberOfDataSymbols = targetNumberOfSymbols;
                parameters.duration = targetDuration;
                for (auto& user : parameters.users) {
                    const int extraSymbols = parameters.commonNumberOfDataSymbols - user.numberOfDataSymbols;
                    if (extraSymbols > 0) {
                        const int64_t extraPaddingBits = (int64_t)extraSymbols * user.dataBitsPerSymbol;
                        if (extraPaddingBits > std::numeric_limits<int>::max() - user.postFecPaddingBits)
                            return makeError(Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                                    "requestedTxTime", "HE TB padding exceeds the model bit-count range");
                        user.postFecPaddingBits += extraPaddingBits;
                    }
                    user.numberOfDataSymbols = parameters.commonNumberOfDataSymbols;
                    user.numberOfSymbols = parameters.commonNumberOfDataSymbols;
                    user.dataDuration = parameters.commonNumberOfDataSymbols * symbolDuration;
                    user.duration = targetDuration;
                }
            }
            if (request.ndp && request.ppduFormat == HE_TRIGGER_BASED_UPLINK &&
                    parameters.common.heLtfDuration != SimTime(32, SIMTIME_US))
                return makeError(Ieee80211HeValidationErrorCode::INVALID_STREAM_MAPPING,
                        "streamStartIndex", "HE TB feedback NDP requires exactly two 4x HE-LTF symbols", 0);
            parameters.common.sigA.bssColor = request.bssColor;
            parameters.common.sigA.uplink = request.uplink;
            parameters.common.sigA.txopUnspecified = request.txopDuration.unspecified;
            parameters.common.sigA.txopDurationUs = request.txopDuration.unspecified ? 0 :
                    request.txopDuration.durationUs < 512 ?
                    request.txopDuration.durationUs / 8 * 8 :
                    512 + (request.txopDuration.durationUs - 512) / 128 * 128;
            parameters.common.sigA.doppler = request.doppler;
            parameters.common.sigA.midamblePeriodicity = request.midamblePeriodicity;
            // STBC data processing is outside the canonical factory contract;
            // all accepted model paths are explicitly non-STBC.
            parameters.common.sigA.stbc = false;
            if (request.ndp) {
                if (parameters.common.ppduFormat == HE_SINGLE_USER) {
                    if (parameters.common.packetExtensionDurationUs != 4)
                        return makeError(Ieee80211HeValidationErrorCode::INVALID_PACKET_EXTENSION,
                                "packetExtensionDurationUs", "HE SU sounding NDP requires a 4 us packet extension");
                }
                else {
                    if (parameters.common.packetExtensionDurationUs != 0)
                        return makeError(Ieee80211HeValidationErrorCode::INVALID_PACKET_EXTENSION,
                                "packetExtensionDurationUs", "HE TB feedback NDP requires a zero-duration packet extension");
                }
                parameters.common.ndp = true;
            }
            else
                parameters.common.ndp = false;
            const auto calculatedPreambleDuration = parameters.common.legacyPreambleDuration +
                    parameters.common.rlSigDuration + parameters.common.heSigADuration +
                    parameters.common.heSigBDuration + parameters.common.heStfDuration +
                    parameters.common.heLtfDuration;
            const auto packetExtensionDuration =
                    SimTime(parameters.common.packetExtensionDurationUs, SIMTIME_US);
            if (parameters.common.legacyPreambleDuration != SimTime(20, SIMTIME_US) ||
                    parameters.common.commonPreambleDuration != calculatedPreambleDuration ||
                    parameters.duration < parameters.common.commonPreambleDuration +
                            packetExtensionDuration + signalExtensionDuration ||
                    (parameters.common.ndp && parameters.duration !=
                            parameters.common.commonPreambleDuration + packetExtensionDuration +
                            signalExtensionDuration) ||
                    (parameters.common.ppduFormat == HE_MU_DOWNLINK) !=
                            (parameters.common.heSigBDuration != SIMTIME_ZERO))
                return makeError(Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                        "ppduLayout", "calculated HE PPDU field durations are inconsistent");
            int64_t modelBitOffset = 0;
            for (size_t userIndex = 0; userIndex < parameters.users.size(); userIndex++) {
                const int64_t psduBytes = parameters.users[userIndex].psduLength.get<B>();
                if (psduBytes < 0 || psduBytes > std::numeric_limits<int64_t>::max() / 8)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                            "psduLength", "HE PSDU bit range exceeds the model offset representation", userIndex);
                const int64_t psduBits = psduBytes * 8;
                if (modelBitOffset > std::numeric_limits<int64_t>::max() - psduBits)
                    return makeError(Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                            "psduLength", "concatenated HE PSDU bit ranges overflow", userIndex);
                modelBitOffset += psduBits;
            }

            Ieee80211HeCommonTxVector commonTx(request.centerFrequency, parameters.common,
                    request.txopDuration);
            std::vector<Ieee80211HeUserTxVector> userTx;
            userTx.reserve(parameters.users.size());
            for (const auto& user : parameters.users)
                userTx.push_back(Ieee80211HeUserTxVector(user));
            auto txVector = std::shared_ptr<const Ieee80211HeTxVector>(
                    new Ieee80211HeTxVector(std::move(commonTx), std::move(userTx)));
            auto ppduLayout = std::shared_ptr<const Ieee80211HePpduLayout>(
                    new Ieee80211HePpduLayout(request.centerFrequency, parameters));
            return Ieee80211HeTxVectorValidationResult(std::move(txVector), std::move(ppduLayout));
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

/**
 * Immutable common RXVECTOR facts available after format-aware HE reception.
 * HE ER SU represents CH_BANDWIDTH with its RU mode rather than a physical Hz
 * value, and the HE TB UPLINK_FLAG is explicitly absent.
 */
class INET_API Ieee80211HeCommonRxVector final
{
  private:
    const Ieee80211HePpduFormat ppduFormat;
    const std::optional<Hz> channelBandwidth;
    const std::optional<Ieee80211HeErSuRuMode> erSuRuMode;
    const Ieee80211HeGuardInterval guardInterval;
    const Ieee80211HeLtfType ltfType;
    const uint8_t bssColor;
    const std::optional<bool> uplink;
    const Ieee80211HeTxopDuration txopDuration;
    const bool doppler;
    const bool stbc;

    Ieee80211HeCommonRxVector(const Ieee80211HeCommonPhyParameters& common,
            const Ieee80211HeUserPhyParameters& user) :
        ppduFormat(common.ppduFormat),
        channelBandwidth(common.ppduFormat == HE_EXTENDED_RANGE_SU ?
                std::optional<Hz>() : std::optional<Hz>(common.channelBandwidth)),
        erSuRuMode(common.ppduFormat != HE_EXTENDED_RANGE_SU ?
                std::optional<Ieee80211HeErSuRuMode>() :
                std::optional<Ieee80211HeErSuRuMode>(user.ru.toneSize == 242 ?
                        Ieee80211HeErSuRuMode::PRIMARY_242_TONE :
                        Ieee80211HeErSuRuMode::PRIMARY_UPPER_106_TONE)),
        guardInterval(common.guardInterval), ltfType(common.ltfType), bssColor(common.sigA.bssColor),
        uplink(common.ppduFormat == HE_TRIGGER_BASED_UPLINK ?
                std::optional<bool>() : std::optional<bool>(common.sigA.uplink)),
        txopDuration{common.sigA.txopUnspecified,
                static_cast<uint16_t>(common.sigA.txopDurationUs)},
        doppler(common.sigA.doppler), stbc(common.sigA.stbc) {}

    friend class Ieee80211HeRxVectorFactory;

  public:
    Ieee80211HeCommonRxVector(const Ieee80211HeCommonRxVector&) = default;
    Ieee80211HeCommonRxVector(Ieee80211HeCommonRxVector&&) = default;
    Ieee80211HeCommonRxVector& operator=(const Ieee80211HeCommonRxVector&) = delete;

    Ieee80211HePpduFormat getPpduFormat() const { return ppduFormat; }
    const std::optional<Hz>& getChannelBandwidth() const { return channelBandwidth; }
    const std::optional<Ieee80211HeErSuRuMode>& getErSuRuMode() const { return erSuRuMode; }
    Ieee80211HeGuardInterval getGuardInterval() const { return guardInterval; }
    Ieee80211HeLtfType getLtfType() const { return ltfType; }
    uint8_t getBssColor() const { return bssColor; }
    const std::optional<bool>& getUplink() const { return uplink; }
    const Ieee80211HeTxopDuration& getTxopDuration() const { return txopDuration; }
    bool getDoppler() const { return doppler; }
    bool getStbc() const { return stbc; }

    bool operator==(const Ieee80211HeCommonRxVector& other) const
    {
        return ppduFormat == other.ppduFormat && channelBandwidth == other.channelBandwidth &&
                erSuRuMode == other.erSuRuMode &&
                guardInterval == other.guardInterval && ltfType == other.ltfType &&
                bssColor == other.bssColor && uplink == other.uplink &&
                txopDuration == other.txopDuration && doppler == other.doppler && stbc == other.stbc;
    }
};

/**
 * Immutable per-user RXVECTOR facts. Absence is format-significant: HE TB user
 * RU/MCS/FEC/DCM/NUM_STS values are trigger-derived MU parameters and are therefore
 * not projected here as received wire facts. PSDU length is supplied by the
 * receiver and checked against the packet-model bit range, which remains
 * exclusively in Ieee80211HePpduLayout.
 */
class INET_API Ieee80211HeUserRxVector final
{
  private:
    const B psduLength;
    const std::optional<uint16_t> staId;
    const std::optional<Ieee80211HeRu> ruAllocation;
    const std::optional<int> mcs;
    const std::optional<int> numberOfSpaceTimeStreams;
    const std::optional<bool> dcm;
    const std::optional<Ieee80211HeCoding> coding;
    const std::optional<uint8_t> ndpFeedbackStatus;
    const std::optional<uint8_t> ndpRuToneSetIndex;
    const std::optional<uint8_t> ndpStartingStsNumber;

    Ieee80211HeUserRxVector(B psduLength, std::optional<uint16_t> staId,
            std::optional<Ieee80211HeRu> ruAllocation, std::optional<int> mcs,
            std::optional<int> numberOfSpaceTimeStreams,
            std::optional<bool> dcm, std::optional<Ieee80211HeCoding> coding,
            std::optional<uint8_t> ndpFeedbackStatus,
            std::optional<uint8_t> ndpRuToneSetIndex,
            std::optional<uint8_t> ndpStartingStsNumber) :
        psduLength(psduLength), staId(std::move(staId)), ruAllocation(std::move(ruAllocation)),
        mcs(std::move(mcs)), numberOfSpaceTimeStreams(std::move(numberOfSpaceTimeStreams)),
        dcm(std::move(dcm)), coding(std::move(coding)),
        ndpFeedbackStatus(std::move(ndpFeedbackStatus)),
        ndpRuToneSetIndex(std::move(ndpRuToneSetIndex)),
        ndpStartingStsNumber(std::move(ndpStartingStsNumber)) {}

    friend class Ieee80211HeRxVectorFactory;

  public:
    Ieee80211HeUserRxVector(const Ieee80211HeUserRxVector&) = default;
    Ieee80211HeUserRxVector(Ieee80211HeUserRxVector&&) = default;
    Ieee80211HeUserRxVector& operator=(const Ieee80211HeUserRxVector&) = delete;

    B getPsduLength() const { return psduLength; }
    const std::optional<uint16_t>& getStaId() const { return staId; }
    const std::optional<Ieee80211HeRu>& getRuAllocation() const { return ruAllocation; }
    const std::optional<int>& getMcs() const { return mcs; }
    const std::optional<int>& getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
    const std::optional<bool>& getDcm() const { return dcm; }
    const std::optional<Ieee80211HeCoding>& getCoding() const { return coding; }
    const std::optional<uint8_t>& getNdpFeedbackStatus() const { return ndpFeedbackStatus; }
    const std::optional<uint8_t>& getNdpRuToneSetIndex() const { return ndpRuToneSetIndex; }
    const std::optional<uint8_t>& getNdpStartingStsNumber() const { return ndpStartingStsNumber; }

    bool operator==(const Ieee80211HeUserRxVector& other) const
    {
        return psduLength == other.psduLength && staId == other.staId &&
                ruAllocation == other.ruAllocation && mcs == other.mcs &&
                numberOfSpaceTimeStreams == other.numberOfSpaceTimeStreams &&
                dcm == other.dcm && coding == other.coding &&
                ndpFeedbackStatus == other.ndpFeedbackStatus &&
                ndpRuToneSetIndex == other.ndpRuToneSetIndex &&
                ndpStartingStsNumber == other.ndpStartingStsNumber;
    }
};

/** Complete immutable reconstructed HE RXVECTOR for one selected recipient. */
class INET_API Ieee80211HeRxVector final
{
  private:
    const Ieee80211HeCommonRxVector common;
    const Ieee80211HeUserRxVector user;

    Ieee80211HeRxVector(Ieee80211HeCommonRxVector common, Ieee80211HeUserRxVector user) :
        common(std::move(common)), user(std::move(user)) {}

    friend class Ieee80211HeRxVectorFactory;

  public:
    Ieee80211HeRxVector(const Ieee80211HeRxVector&) = default;
    Ieee80211HeRxVector(Ieee80211HeRxVector&&) = default;
    Ieee80211HeRxVector& operator=(const Ieee80211HeRxVector&) = delete;

    const Ieee80211HeCommonRxVector& getCommon() const { return common; }
    const Ieee80211HeUserRxVector& getUser() const { return user; }

    bool operator==(const Ieee80211HeRxVector& other) const
    {
        return common == other.common && user == other.user;
    }
};

/** Explicit recipient selection for HE MU and HE TB reconstruction. */
struct Ieee80211HeRxVectorSelection
{
    std::optional<uint16_t> staId;
    std::optional<size_t> userIndex;
};

/** Receiver-provided facts required to reconstruct one HE RXVECTOR. */
struct Ieee80211HeRxVectorReconstructionRequest
{
    Ieee80211HeRxVectorSelection selection;
    std::optional<B> receivedPsduLength;
};

/** Stable errors local to RXVECTOR reconstruction; append new values only. */
enum class Ieee80211HeRxVectorValidationErrorCode {
    NONE = 0,
    VECTOR_LAYOUT_MISMATCH = 1,
    MISSING_USER_SELECTION = 2,
    USER_NOT_FOUND = 3,
    AMBIGUOUS_USER_SELECTION = 4,
    INTERNAL_ERROR = 5,
    UNSUPPORTED_RX_PROJECTION = 6,
    MISSING_RECEIVED_PSDU_LENGTH = 7,
    INVALID_RECEIVED_PSDU_LENGTH = 8,
    RECEIVED_PSDU_LENGTH_MISMATCH = 9,
};

/** Invalid reconstruction results never contain a usable RXVECTOR. */
class INET_API Ieee80211HeRxVectorValidationResult final
{
  private:
    Ieee80211HeRxVectorValidationErrorCode errorCode = Ieee80211HeRxVectorValidationErrorCode::NONE;
    Ieee80211HeValidationContext context;
    std::shared_ptr<const Ieee80211HeRxVector> rxVector;

    Ieee80211HeRxVectorValidationResult(Ieee80211HeRxVectorValidationErrorCode errorCode,
            Ieee80211HeValidationContext context) : errorCode(errorCode), context(std::move(context)) {}
    explicit Ieee80211HeRxVectorValidationResult(std::shared_ptr<const Ieee80211HeRxVector> rxVector) :
        rxVector(std::move(rxVector)) {}

    friend class Ieee80211HeRxVectorFactory;

  public:
    explicit operator bool() const
    {
        return errorCode == Ieee80211HeRxVectorValidationErrorCode::NONE && rxVector;
    }

    Ieee80211HeRxVectorValidationErrorCode getErrorCode() const { return errorCode; }
    const Ieee80211HeValidationContext& getContext() const { return context; }
    const std::shared_ptr<const Ieee80211HeRxVector>& getRxVector() const { return rxVector; }
};

/**
 * Format-aware, non-throwing packet-level reconstruction from one canonical
 * TXVECTOR and its matching calculated PPDU description. This foundation does
 * not claim to decode a complete physical bit stream. A receiver-provided PSDU
 * length is mandatory and is checked against the selected model-container range;
 * TX metadata alone can never produce a normative received vector. Timing and
 * PSDU ranges remain model metadata supplied by Ieee80211HePpduLayout.
 */
class INET_API Ieee80211HeRxVectorFactory final
{
  private:
    static Ieee80211HeRxVectorValidationResult makeError(
            Ieee80211HeRxVectorValidationErrorCode errorCode, const char *fieldName,
            const std::string& detail, std::optional<size_t> userIndex = {})
    {
        Ieee80211HeValidationContext context;
        context.userIndex = userIndex;
        context.fieldName = fieldName;
        context.detail = detail;
        return Ieee80211HeRxVectorValidationResult(errorCode, std::move(context));
    }

  public:
    static Ieee80211HeRxVectorValidationResult reconstruct(const Ieee80211HeTxVector& txVector,
            const Ieee80211HePpduLayout& ppduLayout,
            const Ieee80211HeRxVectorReconstructionRequest& request)
    {
        try {
            if (!ppduLayout.matches(txVector))
                return makeError(Ieee80211HeRxVectorValidationErrorCode::VECTOR_LAYOUT_MISMATCH,
                        "txVector/ppduLayout", "HE TXVECTOR and PPDU layout are not the same canonical description");

            const auto format = txVector.getCommon().getParameters().ppduFormat;
            const auto& users = txVector.getUsers();
            size_t selectedUserIndex = 0;
            if (format == HE_MU_DOWNLINK || format == HE_TRIGGER_BASED_UPLINK) {
                if (!request.selection.staId && !request.selection.userIndex)
                    return makeError(Ieee80211HeRxVectorValidationErrorCode::MISSING_USER_SELECTION,
                            "selection", "HE MU/TB RXVECTOR reconstruction requires a STA ID or user index");
                if (request.selection.userIndex) {
                    if (*request.selection.userIndex >= users.size())
                        return makeError(Ieee80211HeRxVectorValidationErrorCode::USER_NOT_FOUND,
                                "userIndex", "selected HE user index is outside the canonical user list");
                    selectedUserIndex = *request.selection.userIndex;
                    if (request.selection.staId &&
                            users[selectedUserIndex].getParameters().staId != *request.selection.staId)
                        return makeError(Ieee80211HeRxVectorValidationErrorCode::AMBIGUOUS_USER_SELECTION,
                                "selection", "HE STA ID and user index select different users", selectedUserIndex);
                }
                else {
                    std::optional<size_t> matchingUserIndex;
                    for (size_t userIndex = 0; userIndex < users.size(); userIndex++) {
                        if (users[userIndex].getParameters().staId != *request.selection.staId)
                            continue;
                        if (matchingUserIndex)
                            return makeError(Ieee80211HeRxVectorValidationErrorCode::AMBIGUOUS_USER_SELECTION,
                                    "staId", "HE STA ID identifies multiple users; supply a user index");
                        matchingUserIndex = userIndex;
                    }
                    if (!matchingUserIndex)
                        return makeError(Ieee80211HeRxVectorValidationErrorCode::USER_NOT_FOUND,
                                "staId", "HE STA ID is absent from the canonical user list");
                    selectedUserIndex = *matchingUserIndex;
                }
            }

            const auto& commonParameters = txVector.getCommon().getParameters();
            const auto& userParameters = users[selectedUserIndex].getParameters();
            if (!request.receivedPsduLength)
                return makeError(Ieee80211HeRxVectorValidationErrorCode::MISSING_RECEIVED_PSDU_LENGTH,
                        "receivedPsduLength", "HE RXVECTOR reconstruction requires a receiver-provided PSDU length",
                        selectedUserIndex);
            const int64_t receivedPsduBytes = request.receivedPsduLength->get<B>();
            if (receivedPsduBytes < 0 || receivedPsduBytes > std::numeric_limits<int64_t>::max() / 8)
                return makeError(Ieee80211HeRxVectorValidationErrorCode::INVALID_RECEIVED_PSDU_LENGTH,
                        "receivedPsduLength", "received HE PSDU length exceeds the model offset representation",
                        selectedUserIndex);
            const b receivedPsduBitLength(receivedPsduBytes * 8);
            if (ppduLayout.isNdp()) {
                if (*request.receivedPsduLength != B(0))
                    return makeError(Ieee80211HeRxVectorValidationErrorCode::RECEIVED_PSDU_LENGTH_MISMATCH,
                            "receivedPsduLength", "an HE NDP has no received PSDU", selectedUserIndex);
            }
            else {
                auto range = std::find_if(ppduLayout.getPsduBitRanges().begin(),
                        ppduLayout.getPsduBitRanges().end(),
                        [=] (const auto& candidate) { return candidate.getUserIndex() == selectedUserIndex; });
                if (range == ppduLayout.getPsduBitRanges().end())
                    return makeError(Ieee80211HeRxVectorValidationErrorCode::VECTOR_LAYOUT_MISMATCH,
                            "ppduLayout.psduBitRanges", "selected HE user has no model-container PSDU range",
                            selectedUserIndex);
                if (range->getBitLength() != receivedPsduBitLength)
                    return makeError(Ieee80211HeRxVectorValidationErrorCode::RECEIVED_PSDU_LENGTH_MISMATCH,
                            "receivedPsduLength", "received HE PSDU length disagrees with the canonical layout range",
                            selectedUserIndex);
            }
            // The current canonical request contract has no STBC input and validates a
            // non-STBC foundation, where its spatial-stream count is exactly NUM_STS.
            // Reject a future/mixed STBC description instead of inventing a transform.
            if (commonParameters.sigA.stbc)
                return makeError(Ieee80211HeRxVectorValidationErrorCode::UNSUPPORTED_RX_PROJECTION,
                        "stbc", "STBC NUM_STS reconstruction is outside the canonical HE foundation");
            Ieee80211HeCommonRxVector commonRx(commonParameters, userParameters);
            const bool isTb = format == HE_TRIGGER_BASED_UPLINK;
            const bool isMu = format == HE_MU_DOWNLINK;
            const bool isNdpFeedback = isTb && ppduLayout.isNdp() && userParameters.ndpFeedbackReport;
            Ieee80211HeUserRxVector userRx(*request.receivedPsduLength,
                    isMu ? std::optional<uint16_t>(userParameters.staId) : std::optional<uint16_t>(),
                    isMu ? std::optional<Ieee80211HeRu>(userParameters.ru) : std::optional<Ieee80211HeRu>(),
                    !isTb ? std::optional<int>(userParameters.mcs) : std::optional<int>(),
                    !isTb ? std::optional<int>(userParameters.numberOfSpatialStreams) : std::optional<int>(),
                    !isTb ? std::optional<bool>(userParameters.dcm) : std::optional<bool>(),
                    !isTb ? std::optional<Ieee80211HeCoding>(userParameters.coding) :
                            std::optional<Ieee80211HeCoding>(),
                    isNdpFeedback ? std::optional<uint8_t>(userParameters.ndpFeedbackStatus) : std::optional<uint8_t>(),
                    isNdpFeedback ? std::optional<uint8_t>(userParameters.ndpRuToneSetIndex) : std::optional<uint8_t>(),
                    isNdpFeedback ? std::optional<uint8_t>(userParameters.ndpStartingStsNumber) : std::optional<uint8_t>());
            auto rxVector = std::shared_ptr<const Ieee80211HeRxVector>(
                    new Ieee80211HeRxVector(std::move(commonRx), std::move(userRx)));
            return Ieee80211HeRxVectorValidationResult(std::move(rxVector));
        }
        catch (const std::exception& error) {
            return makeError(Ieee80211HeRxVectorValidationErrorCode::INTERNAL_ERROR,
                    "reconstruction", error.what());
        }
        catch (...) {
            return makeError(Ieee80211HeRxVectorValidationErrorCode::INTERNAL_ERROR,
                    "reconstruction", "unknown HE RXVECTOR reconstruction error");
        }
    }
};

/**
 * Short-lived packet-model handoff from radio encapsulation to the transmitter.
 * The pointed-to objects are immutable and are deliberately not wire content.
 */
class INET_API Ieee80211HeTxVectorReq final : public TagBase
{
  private:
    std::shared_ptr<const Ieee80211HeTxVector> txVector;
    std::shared_ptr<const Ieee80211HePpduLayout> ppduLayout;

  public:
    Ieee80211HeTxVectorReq() = default;
    Ieee80211HeTxVectorReq(const Ieee80211HeTxVectorReq&) = default;
    virtual Ieee80211HeTxVectorReq *dup() const override { return new Ieee80211HeTxVectorReq(*this); }

    void setCanonicalPair(std::shared_ptr<const Ieee80211HeTxVector> txVector,
            std::shared_ptr<const Ieee80211HePpduLayout> ppduLayout)
    {
        if (!txVector || !ppduLayout || !ppduLayout->matches(*txVector))
            throw cRuntimeError("Invalid canonical HE TXVECTOR/PPDU-layout handoff");
        this->txVector = std::move(txVector);
        this->ppduLayout = std::move(ppduLayout);
    }

    const std::shared_ptr<const Ieee80211HeTxVector>& getTxVector() const { return txVector; }
    const std::shared_ptr<const Ieee80211HePpduLayout>& getPpduLayout() const { return ppduLayout; }
};

/** Receiver-decoded, model-only HE RXVECTOR indication. */
class INET_API Ieee80211HeRxVectorInd final : public TagBase
{
  private:
    std::shared_ptr<const Ieee80211HeRxVector> rxVector;

  public:
    Ieee80211HeRxVectorInd() = default;
    Ieee80211HeRxVectorInd(const Ieee80211HeRxVectorInd&) = default;
    virtual Ieee80211HeRxVectorInd *dup() const override { return new Ieee80211HeRxVectorInd(*this); }

    void setRxVector(std::shared_ptr<const Ieee80211HeRxVector> rxVector)
    {
        if (!rxVector)
            throw cRuntimeError("Invalid empty HE RXVECTOR indication");
        this->rxVector = std::move(rxVector);
    }

    const std::shared_ptr<const Ieee80211HeRxVector>& getRxVector() const { return rxVector; }
};

/**
 * Model-only Trigger recipient context for an HE TB reception. These parameters
 * come from the selected canonical PPDU-layout user, not from decoded RXVECTOR
 * fields, and are deliberately not serialized as received PHY signaling.
 */
class INET_API Ieee80211HeTbRecipientContextInd final : public TagBase
{
  private:
    std::shared_ptr<const Ieee80211HeUserPhyParameters> recipientParameters;

  public:
    Ieee80211HeTbRecipientContextInd() = default;
    Ieee80211HeTbRecipientContextInd(const Ieee80211HeTbRecipientContextInd&) = default;
    virtual Ieee80211HeTbRecipientContextInd *dup() const override { return new Ieee80211HeTbRecipientContextInd(*this); }

    void setRecipientParameters(std::shared_ptr<const Ieee80211HeUserPhyParameters> recipientParameters)
    {
        if (!recipientParameters || this->recipientParameters)
            throw cRuntimeError("Invalid HE TB recipient context indication");
        this->recipientParameters = std::move(recipientParameters);
    }

    const std::shared_ptr<const Ieee80211HeUserPhyParameters>& getRecipientParameters() const
    {
        return recipientParameters;
    }
};

} // namespace physicallayer
} // namespace inet

#endif
