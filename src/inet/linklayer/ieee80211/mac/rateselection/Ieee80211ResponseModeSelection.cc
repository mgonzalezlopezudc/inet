// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inet/linklayer/ieee80211/mac/rateselection/Ieee80211ResponseModeSelection.h"

#include "inet/linklayer/ieee80211/mgmt/Ieee80211RateContextTag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet::ieee80211 {

using namespace physicallayer;
using ModulationClass = IIeee80211Mode::ModulationClass;

namespace {

bool isNonHt(const IIeee80211Mode *mode)
{
    auto modulation = mode->getModulationClass();
    return modulation == ModulationClass::OFDM || modulation == ModulationClass::ERP_OFDM ||
            modulation == ModulationClass::DSSS_HRDSSS;
}

bool sameMode(const IIeee80211Mode *left, const IIeee80211Mode *right)
{
    return left && right && left->getModulationClass() == right->getModulationClass() &&
            left->getLegacyPreambleType() == right->getLegacyPreambleType() &&
            left->getDataMode()->getBandwidth() == right->getDataMode()->getBandwidth() &&
            left->getDataMode()->getNetBitrate() == right->getDataMode()->getNetBitrate();
}

bool permitsRate(const ResponseRateContext& rates, bps rate)
{
    if (!rates.localRates.supported.known || !rates.localRates.operational.known)
        return false;
    for (auto set : {&rates.localRates.supported, &rates.localRates.operational,
                    &rates.peerRates.supported, &rates.peerRates.operational})
        if (set->known && set->legacyRates.count(rate) == 0)
            return false;
    return true;
}

std::string unknownFacts(const ResponseRateContext& rates)
{
    if (!rates.known)
        return "unknown or conflicting BSS relationship";
    if (!rates.localRates.supported.known || !rates.localRates.operational.known)
        return "unknown local supported or operational rates";
    if (rates.context.kind != BssRateContextRef::NONE && !rates.bssRates.basic.known)
        return "unknown BSS basic rates";
    return {};
}

std::set<bps> mandatoryRates(const Ieee80211ModeSet *modes)
{
    std::set<bps> result;
    for (int i = 0; i < modes->getNumModes(); i++)
        if (modes->isMandatory(i) && isNonHt(modes->getMode(i)))
            result.insert(modes->getMode(i)->getDataMode()->getNetBitrate());
    return result;
}

bps highestRate(const std::set<bps>& rates, bps bound)
{
    bps result(0);
    for (auto rate : rates)
        if (rate <= bound && rate > result)
            result = rate;
    return result;
}

bool matchesRequest(const IIeee80211Mode *mode, const IIeee80211Mode *request)
{
    if (!isNonHt(mode) || mode->getDataMode()->getBandwidth() != request->getDataMode()->getBandwidth())
        return false;
    if (isNonHt(request))
        return mode->getModulationClass() == request->getModulationClass() &&
                mode->getLegacyPreambleType() == request->getLegacyPreambleType();
    return mode->getModulationClass() == ModulationClass::OFDM || mode->getModulationClass() == ModulationClass::ERP_OFDM;
}

} // namespace

ResponseModeResult Ieee80211ResponseModeSelection::computeResponse(const ResponseModeInput& input)
{
    ResponseModeResult result;
    result.input = input;
    auto fail = [&](const char *reason) { result.reason = reason; return result; };
    if (!input.requestMode || !input.transmitModes)
        return fail("missing request mode or transmit catalog");
    if (input.length != (input.kind == ControlResponseKind::ACK ? LENGTH_ACK : LENGTH_CTS))
        return fail("unsupported complete response length");
    auto requestClass = input.requestMode->getModulationClass();
    if (!isNonHt(input.requestMode) && requestClass != ModulationClass::HT && requestClass != ModulationClass::VHT)
        return fail("unsupported request format");
    if (input.requiresHtVhtResponse || (input.kind == ControlResponseKind::CTS && requestClass == ModulationClass::HT))
        return fail("required HT/VHT response format is unsupported");
    if (input.kind == ControlResponseKind::CTS && requestClass == ModulationClass::VHT)
        return fail("VHT RTS HT Control restrictions are not represented");
    result.reason = unknownFacts(input.rates);
    if (!result.reason.empty())
        return result;
    auto bound = input.requestMode->getNonHtReferenceRate();
    if (!std::isfinite(bound.get()) || bound <= bps(0))
        return fail("unavailable non-HT reference rate");
    auto mandatory = mandatoryRates(input.transmitModes);
    auto primaryRate = highestRate(input.rates.bssRates.basic.legacyRates, bound);
    if (primaryRate == bps(0))
        primaryRate = highestRate(mandatory, bound);
    if (primaryRate == bps(0))
        return fail("no primary basic or mandatory rate below the request bound");
    // Select the required rate first. An absent representation never lowers it.
    for (int i = 0; i < input.transmitModes->getNumModes(); i++) {
        auto mode = input.transmitModes->getMode(i);
        if (mode->getDataMode()->getNetBitrate() == primaryRate && matchesRequest(mode, input.requestMode)) {
            if (result.primaryMode && !sameMode(result.primaryMode, mode))
                return fail("ambiguous primary response format");
            result.primaryMode = mode;
        }
    }
    if (!result.primaryMode)
        return fail("required primary format, preamble, or width is not represented");
    if (!permitsRate(input.rates, primaryRate))
        return fail("primary rate violates local or known peer limits");
    result.airtime = result.primaryMode->getDuration(input.length);
    for (int i = 0; i < input.transmitModes->getNumModes(); i++) {
        auto mode = input.transmitModes->getMode(i);
        auto rate = mode->getDataMode()->getNetBitrate();
        bool member = input.rates.bssRates.basic.legacyRates.count(rate) || mandatory.count(rate);
        if (member && matchesRequest(mode, input.requestMode) && permitsRate(input.rates, rate) &&
                mode->getModulationClass() == result.primaryMode->getModulationClass() &&
                mode->getDuration(input.length) == result.airtime)
            result.permittedModes.push_back(mode);
    }
    result.status = ModePreparationStatus::READY;
    return result;
}

bool Ieee80211ResponseModeSelection::containsMode(const ResponseModeResult& result, const IIeee80211Mode *mode)
{
    if (result.status != ModePreparationStatus::READY || !mode)
        return false;
    for (auto permitted : result.permittedModes)
        if (sameMode(permitted, mode) && mode->getDuration(result.input.length) == result.airtime)
            return true;
    return false;
}

const IIeee80211Mode *Ieee80211ResponseModeSelection::selectResponse(const ResponseModeResult& result, const IIeee80211Mode *overrideMode)
{
    if (result.status != ModePreparationStatus::READY)
        return nullptr;
    if (!overrideMode)
        return result.primaryMode;
    if (!containsMode(result, overrideMode)) {
        auto rate = overrideMode->getDataMode()->getNetBitrate();
        auto mandatory = mandatoryRates(result.input.transmitModes);
        const char *rule = "equal complete airtime";
        if (!result.input.rates.bssRates.basic.legacyRates.count(rate) && !mandatory.count(rate))
            rule = "basic or mandatory rate membership";
        else if (overrideMode->getModulationClass() != result.primaryMode->getModulationClass())
            rule = "response modulation class";
        else if (overrideMode->getLegacyPreambleType() != result.primaryMode->getLegacyPreambleType())
            rule = "response preamble";
        else if (overrideMode->getDataMode()->getBandwidth() != result.primaryMode->getDataMode()->getBandwidth())
            rule = "response channel width";
        else if (!permitsRate(result.input.rates, rate))
            rule = "local or known peer rate permission";
        throw cRuntimeError("Illegal %s override: request=%s primary=%s override=%s; failed rule: %s",
            result.input.kind == ControlResponseKind::ACK ? "ACK" : "CTS", result.input.requestMode->getName(),
            result.primaryMode->getName(), overrideMode->getName(), rule);
    }
    return overrideMode;
}

std::string Ieee80211ResponseModeSelection::validateResponse(const ResponseModeResult& result, const Packet *response)
{
    auto header = response->peekAtFront<Ieee80211MacHeader>();
    bool correctKind = header->getType() == (result.input.kind == ControlResponseKind::ACK ? ST_ACK : ST_CTS) &&
        (result.input.kind == ControlResponseKind::ACK ?
            dynamicPtrCast<const Ieee80211AckFrame>(header) != nullptr : dynamicPtrCast<const Ieee80211CtsFrame>(header) != nullptr);
    if (!correctKind)
        return "wrong response kind";
    if (response->getDataLength() != result.input.length || header->getChunkLength() + makeShared<Ieee80211MacTrailer>()->getChunkLength() != result.input.length)
        return "wrong complete response length";
    auto indication = response->findTag<Ieee80211ModeInd>();
    if (!indication || !indication->getMode())
        return "missing response mode indication";
    if (!containsMode(result, indication->getMode()))
        return "response mode violates the retained permission or equal-airtime rule";
    return {};
}

PreparedModeResult Ieee80211ResponseModeSelection::computeRts(const PreparedModeInput& input, const IIeee80211Mode *overrideMode)
{
    PreparedModeResult result;
    result.reason = unknownFacts(input.rates);
    if (!result.reason.empty())
        return result;
    if (!input.header || !input.transmitModes || input.header->getType() != ST_RTS) {
        result.reason = "missing RTS header or transmit catalog";
        return result;
    }
    auto mandatory = mandatoryRates(input.transmitModes);
    const auto& basic = input.rates.bssRates.basic.legacyRates;
    bps bound(INFINITY);
    bool initial = input.position == PreparedControlPosition::TXOP_INITIAL;
    if (!initial) {
        if (!input.previousToSamePeer || input.previousToSamePeer->peer != input.header->getReceiverAddress() || !input.previousToSamePeer->mode) {
            result.reason = "missing previous transmission to the same peer";
            return result;
        }
        auto reference = input.previousToSamePeer->mode->getNonHtReferenceRate();
        if (!std::isfinite(reference.get()) || reference <= bps(0)) {
            result.reason = "unavailable previous non-HT reference rate";
            return result;
        }
        bound = highestRate(basic, reference);
        if (bound == bps(0))
            bound = highestRate(mandatory, reference);
        if (bound == bps(0)) {
            result.reason = "no later RTS rate bound";
            return result;
        }
    }
    std::vector<const IIeee80211Mode *> legal;
    for (int i = 0; i < input.transmitModes->getNumModes(); i++) {
        auto mode = input.transmitModes->getMode(i);
        auto rate = mode->getDataMode()->getNetBitrate();
        bool member = !initial || (basic.empty() ? mandatory.count(rate) : basic.count(rate));
        if (member && isNonHt(mode) && rate <= bound && permitsRate(input.rates, rate)) {
            legal.push_back(mode);
            if (!result.mode || rate > result.mode->getDataMode()->getNetBitrate())
                result.mode = mode;
        }
    }
    if (overrideMode) {
        bool permitted = std::any_of(legal.begin(), legal.end(), [&](auto mode) { return sameMode(mode, overrideMode); });
        if (!permitted) {
            auto rate = overrideMode->getDataMode()->getNetBitrate();
            const char *rule = "represented RTS mode";
            if (!isNonHt(overrideMode))
                rule = "non-HT RTS format";
            else if (!permitsRate(input.rates, rate))
                rule = "local or known peer rate permission";
            else if (initial && !(basic.empty() ? mandatory.count(rate) : basic.count(rate)))
                rule = "initial RTS basic or mandatory rate membership";
            else if (!initial && rate > bound)
                rule = "continuation RTS same-peer rate bound";
            throw cRuntimeError("Illegal RTS override: request=%s primary=%s override=%s; failed rule: %s",
                input.previousToSamePeer && input.previousToSamePeer->mode ? input.previousToSamePeer->mode->getName() : "TXOP initial",
                result.mode ? result.mode->getName() : "unavailable", overrideMode->getName(), rule);
        }
        result.mode = overrideMode;
    }
    if (!result.mode) {
        result.reason = "no represented legal RTS mode";
        return result;
    }
    result.status = ModePreparationStatus::READY;
    return result;
}

std::optional<BssRateContextRef> Ieee80211ResponseModeSelection::getFrameContext(const Packet *frame)
{
    if (auto tag = frame->findTag<Ieee80211RateContextTag>())
        return BssRateContextRef{static_cast<BssRateContextRef::Kind>(tag->getContextKind()), tag->getBssid(), tag->getTransactionId(), tag->getGeneration()};
    return std::nullopt;
}

std::optional<MacAddress> Ieee80211ResponseModeSelection::getRequestBssid(const Ptr<const Ieee80211MacHeader>& header)
{
    auto dataOrMgmt = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header);
    if (!dataOrMgmt || (header->getToDS() && header->getFromDS()))
        return std::nullopt;
    auto bssid = header->getToDS() ? header->getReceiverAddress() :
        (header->getFromDS() ? dataOrMgmt->getTransmitterAddress() : dataOrMgmt->getAddress3());
    return bssid.isUnspecified() || bssid.isMulticast() ? std::nullopt : std::optional<MacAddress>(bssid);
}

} // namespace inet::ieee80211
