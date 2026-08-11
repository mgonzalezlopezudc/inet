//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mib/Ieee80211PeerCapabilityState.h"

#include <sstream>

namespace inet {
namespace ieee80211 {

namespace {

template<typename T>
std::string arrayKey(const T& values)
{
    std::stringstream stream;
    for (auto value : values)
        stream << value << ',';
    return stream.str();
}

template<typename T>
void appendSet(std::stringstream& stream, const T& values)
{
    for (const auto& value : values)
        stream << value << ',';
    stream << ';';
}

void append(std::stringstream& stream, const Ieee80211HtCapabilities& value)
{
    auto fields = [&stream](auto... values) { ((stream << values << ','), ...); };
    appendSet(stream, value.supportedChannelWidths);
    stream << arrayKey(value.rxMcsNss.maxMcsPerNss) << arrayKey(value.txMcsNss.maxMcsPerNss);
    fields(value.ldpc, value.shortGi20, value.shortGi40, value.maxAmpduLengthExponent,
            (int)value.mcsFeedback, value.htcSupport, value.receiveNdp, value.transmitNdp,
            (int)value.explicitCsiFeedback, (int)value.explicitNoncompressedFeedback,
            (int)value.explicitCompressedFeedback);
}

void append(std::stringstream& stream, const Ieee80211HtDirectionalCapabilities& value)
{
    auto fields = [&stream](auto... values) { ((stream << values << ','), ...); };
    fields(value.valid);
    appendSet(stream, value.supportedChannelWidths);
    stream << arrayKey(value.mcsNss.maxMcsPerNss);
    fields(value.ldpc, value.shortGi20, value.shortGi40, value.receiverMaxAmpduLengthExponent,
            value.mcsRequestAllowed, value.mcsFeedbackAllowed, value.htcSupported,
            value.transmitterCanSendNdp, value.receiverCanReceiveNdp, (int)value.explicitCsiFeedback,
            (int)value.explicitNoncompressedFeedback, (int)value.explicitCompressedFeedback);
}

void append(std::stringstream& stream, const Ieee80211HtOperation& value)
{
    stream << value.operatingChannelWidth << ',' << value.primaryChannel << ',' << value.secondaryChannelOffset
           << ',' << (int)value.protectionMode << ',' << arrayKey(value.basicMcsNss.maxMcsPerNss) << ';';
}

std::string key(const Ieee80211HtCapabilities& value, const Ieee80211NegotiatedHtCapabilities& negotiated)
{
    std::stringstream stream;
    append(stream, value);
    append(stream, negotiated.localAdvertisement);
    append(stream, negotiated.peerAdvertisement);
    append(stream, negotiated.localTxPeerRx);
    append(stream, negotiated.localRxPeerTx);
    append(stream, negotiated.operation);
    return stream.str();
}

void append(std::stringstream& stream, const Ieee80211VhtCapabilities& value)
{
    auto fields = [&stream](auto... values) { ((stream << values << ','), ...); };
    appendSet(stream, value.supportedChannelWidths);
    fields(value.supports80Plus80MHz);
    stream << arrayKey(value.rxMcsNss.maxMcsPerNss) << arrayKey(value.txMcsNss.maxMcsPerNss);
    fields(value.rxLdpc, value.ldpc, value.stbc, value.maxAmpduLengthExponent, value.maxNss,
            value.maxMcs, value.rxHighestLongGiDataRateMbps, value.txHighestLongGiDataRateMbps,
            value.maxNstsTotal, value.shortGi80, value.shortGi160, value.suBeamformer,
            value.suBeamformee, value.beamformeeSts, value.soundingDimensions,
            value.muBeamformer, value.muBeamformee);
}

void append(std::stringstream& stream, const Ieee80211VhtDirectionalCapabilities& value)
{
    auto fields = [&stream](auto... values) { ((stream << values << ','), ...); };
    fields(value.valid);
    appendSet(stream, value.supportedChannelWidths);
    stream << arrayKey(value.mcsNss.maxMcsPerNss);
    fields(value.ldpc, value.shortGi80, value.shortGi160, value.suBeamforming, value.soundingNsts,
            value.maxNstsTotal, value.muMimo, value.receiverMaxAmpduLengthExponent);
}

void append(std::stringstream& stream, const Ieee80211VhtOperation& value)
{
    stream << value.operatingChannelWidth << ',' << value.centerFrequencySegment0 << ',' << value.centerFrequencySegment1
           << ',' << value.nonContiguous << ',' << value.numSpatialStreams << ',' << value.shortGi << ',' << value.ldpc << ','
           << arrayKey(value.basicMcsNss.maxMcsPerNss) << ';';
}

std::string key(const Ieee80211VhtCapabilities& value, const Ieee80211NegotiatedVhtCapabilities& negotiated)
{
    std::stringstream stream;
    append(stream, value);
    append(stream, negotiated.localAdvertisement);
    append(stream, negotiated.peerAdvertisement);
    append(stream, negotiated.localTxPeerRx);
    append(stream, negotiated.localRxPeerTx);
    append(stream, negotiated.intersection);
    append(stream, negotiated.operation);
    stream << negotiated.valid;
    return stream.str();
}

std::string ratesKey(const std::vector<Ieee80211LegacyRate>& rates)
{
    std::stringstream stream;
    for (const auto& rate : rates)
        stream << rate.rate << ':' << rate.basic << ',';
    return stream.str();
}

} // namespace

bool Ieee80211PeerCapabilityState::Snapshot::isPresent() const
{
    return advertisedHt || advertisedVht || advertisedHe || advertisedEht || legacyRates;
}

void Ieee80211PeerCapabilityState::advanceGeneration(uint64_t& generation)
{
    if (++generation == 0)
        generation = 1;
}

void Ieee80211PeerCapabilityState::advance(Record& record, uint64_t *domainGeneration)
{
    advanceGeneration(record.generation);
    if (domainGeneration != nullptr)
        advanceGeneration(*domainGeneration);
}

Ieee80211PeerCapabilityState::Snapshot Ieee80211PeerCapabilityState::getSnapshot(const MacAddress& address) const
{
    auto it = records.find(address);
    if (it == records.end())
        return Snapshot(address, {}, {}, {}, {}, {}, {}, {}, {}, {}, 0, 0, 0, 0, 0, 0);
    const auto& r = it->second;
    return Snapshot(address, r.advertisedHt, r.negotiatedHt, r.advertisedVht, r.negotiatedVht,
            r.advertisedHe, r.negotiatedHe, r.advertisedEht, r.negotiatedEht, r.legacyRates,
            r.generation, r.htGeneration, r.vhtGeneration, r.heGeneration, r.ehtGeneration,
            r.legacyRatesGeneration);
}

std::vector<Ieee80211PeerCapabilityState::Snapshot> Ieee80211PeerCapabilityState::getSnapshots() const
{
    std::vector<Snapshot> result;
    for (const auto& entry : records)
        if (auto snapshot = getSnapshot(entry.first); snapshot.isPresent())
            result.push_back(snapshot);
    return result;
}

void Ieee80211PeerCapabilityState::setHt(const MacAddress& address, const Ieee80211HtCapabilities& advertised, const Ieee80211NegotiatedHtCapabilities& negotiated)
{
    auto& r = records[address];
    auto newKey = key(advertised, negotiated);
    if (r.htKey != newKey || !r.advertisedHt) {
        r.advertisedHt = advertised;
        r.negotiatedHt = negotiated;
        r.htKey = newKey;
        advance(r, &r.htGeneration);
    }
}

void Ieee80211PeerCapabilityState::setVht(const MacAddress& address, const Ieee80211VhtCapabilities& advertised, const Ieee80211NegotiatedVhtCapabilities& negotiated)
{
    auto& r = records[address];
    auto newKey = key(advertised, negotiated);
    if (r.vhtKey != newKey || !r.advertisedVht) {
        r.advertisedVht = advertised;
        r.negotiatedVht = negotiated;
        r.vhtKey = newKey;
        advance(r, &r.vhtGeneration);
    }
}

void Ieee80211PeerCapabilityState::setHe(const MacAddress& address, const Ieee80211HeCapabilities& advertised, const Ieee80211NegotiatedHeCapabilities& negotiated)
{
    auto& r = records[address];
    if (!r.advertisedHe || !r.negotiatedHe ||
            !(*r.advertisedHe == advertised) || !(*r.negotiatedHe == negotiated)) {
        r.advertisedHe = advertised;
        r.negotiatedHe = negotiated;
        advance(r, &r.heGeneration);
    }
}

void Ieee80211PeerCapabilityState::setEht(const MacAddress& address, const Ieee80211EhtCapabilities& advertised, const Ieee80211NegotiatedEhtCapabilities& negotiated)
{
    auto& r = records[address];
    if (!r.advertisedEht || !r.negotiatedEht ||
            !(*r.advertisedEht == advertised) || !(*r.negotiatedEht == negotiated)) {
        r.advertisedEht = advertised;
        r.negotiatedEht = negotiated;
        advance(r, &r.ehtGeneration);
    }
}

void Ieee80211PeerCapabilityState::setLegacyRates(const MacAddress& address, const std::vector<Ieee80211LegacyRate>& rates)
{
    auto& r = records[address];
    auto newKey = ratesKey(rates);
    if (r.ratesKey != newKey || !r.legacyRates) {
        r.legacyRates = rates;
        r.ratesKey = newKey;
        advance(r, &r.legacyRatesGeneration);
    }
}

void Ieee80211PeerCapabilityState::removeHt(const MacAddress& address)
{
    auto it = records.find(address);
    if (it != records.end() && (it->second.advertisedHt || it->second.negotiatedHt)) {
        auto& r = it->second;
        r.advertisedHt.reset();
        r.negotiatedHt.reset();
        r.htKey.clear();
        advance(r, &r.htGeneration);
    }
}

void Ieee80211PeerCapabilityState::removeVht(const MacAddress& address)
{
    auto it = records.find(address);
    if (it != records.end() && (it->second.advertisedVht || it->second.negotiatedVht)) {
        auto& r = it->second;
        r.advertisedVht.reset();
        r.negotiatedVht.reset();
        r.vhtKey.clear();
        advance(r, &r.vhtGeneration);
    }
}

void Ieee80211PeerCapabilityState::removeHe(const MacAddress& address)
{
    auto it = records.find(address);
    if (it != records.end() && (it->second.advertisedHe || it->second.negotiatedHe)) {
        auto& r = it->second;
        r.advertisedHe.reset();
        r.negotiatedHe.reset();
        advance(r, &r.heGeneration);
    }
}

void Ieee80211PeerCapabilityState::removeEht(const MacAddress& address)
{
    auto it = records.find(address);
    if (it != records.end() && (it->second.advertisedEht || it->second.negotiatedEht)) {
        auto& r = it->second;
        r.advertisedEht.reset();
        r.negotiatedEht.reset();
        advance(r, &r.ehtGeneration);
    }
}

void Ieee80211PeerCapabilityState::removeAll(const MacAddress& address)
{
    removeHt(address);
    removeVht(address);
    removeHe(address);
    removeEht(address);
    auto it = records.find(address);
    if (it != records.end() && it->second.legacyRates) {
        auto& r = it->second;
        r.legacyRates.reset();
        r.ratesKey.clear();
        advance(r, &r.legacyRatesGeneration);
    }
}

} // namespace ieee80211
} // namespace inet
