//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/signal/WirelessSignal.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/common/packet/chunk/SliceChunk.h"
#include "inet/common/packet/chunk/cPacketChunk.h"

namespace inet {
namespace physicallayer {

static Ptr<Chunk> cloneChunkDeep(const Ptr<const Chunk>& chunk)
{
    if (chunk == nullptr)
        return nullptr;
    if (auto cPktChunk = dynamicPtrCast<const cPacketChunk>(chunk)) {
        auto cloned = cPktChunk->dupShared();
        cloned->markImmutable();
        return cloned;
    }
    else if (auto seqChunk = dynamicPtrCast<const SequenceChunk>(chunk)) {
        auto newSeq = makeShared<SequenceChunk>();
        for (const auto& child : seqChunk->getChunks()) {
            newSeq->insertAtBack(cloneChunkDeep(child));
        }
        newSeq->markImmutable();
        return newSeq;
    }
    else if (auto sliceChunk = dynamicPtrCast<const SliceChunk>(chunk)) {
        auto clonedInner = cloneChunkDeep(sliceChunk->getChunk());
        auto newSlice = makeShared<SliceChunk>(clonedInner, sliceChunk->getOffset(), sliceChunk->getChunkLength());
        newSlice->markImmutable();
        return newSlice;
    }
    return constPtrCast<Chunk>(chunk);
}

Define_Module(Ieee80211RadioMedium);

void Ieee80211RadioMedium::addTransmission(const IRadio *transmitterRadio, const ITransmission *transmission)
{
    auto packet = transmission->getPacket();
    auto heMuTag = packet ? packet->findTag<Ieee80211HeMuTag>() : nullptr;
    auto ieee80211Trans = dynamic_cast<const Ieee80211Transmission *>(transmission);

    if (heMuTag != nullptr && !heMuTag->getAllocations().empty() && ieee80211Trans != nullptr) {
        auto flatTransmitter = check_and_cast<const FlatTransmitterBase *>(transmitterRadio->getTransmitter());
        W totalPower = flatTransmitter->getPower();
        Hz totalBandwidth = ieee80211Trans->getMode()->getDataMode()->getBandwidth();
        Hz centerFreq = ieee80211Trans->getChannel()->getCenterFrequency();

        const auto& allocations = heMuTag->getAllocations();
        int numRUs = allocations.size();
        auto rus = calculateHeRus(centerFreq, totalBandwidth, numRUs);

        // Validate derived RUs match allocations count
        if ((int)rus.size() != numRUs) {
            throw cRuntimeError("RU count mismatch: derived %d RUs but tag has %d allocations",
                (int)rus.size(), numRUs);
        }

        EV_DEBUG << "HE MU transmission: " << numRUs << " RUs on " << totalBandwidth.get() / 1e6 << " MHz channel "
                 << "at " << centerFreq.get() / 1e9 << " GHz, total power " << totalPower.get() << " W\n";

        std::vector<const ITransmission *> subTransmissions;
        W accumulatedPower = W(0);

        for (const auto& alloc : allocations) {
            int ruIndex = alloc.ruIndex;
            if (ruIndex < 0 || ruIndex >= (int)rus.size())
                throw cRuntimeError("Invalid RU index %d out of range [0, %d)", ruIndex, (int)rus.size());
            
            const auto& ru = rus[ruIndex];

            W ruPower = totalPower * (ru.bandwidth.get() / totalBandwidth.get());
            accumulatedPower = accumulatedPower + ruPower;

            EV_DEBUG << "  RU[" << ruIndex << "]: centerFreq=" << ru.centerFrequency.get() / 1e9 << " GHz, "
                     << "bandwidth=" << ru.bandwidth.get() / 1e6 << " MHz, power=" << ruPower.get() << " W\n";

            auto ruAnalogModel = flatTransmitter->getAnalogModel()->createAnalogModel(
                transmission->getPreambleDuration(),
                transmission->getHeaderDuration(),
                transmission->getDataDuration(),
                ru.centerFrequency,
                ru.bandwidth,
                ruPower
            );

            auto subPacket = alloc.packet->dup();
            auto frontOffset = subPacket->getFrontOffset();
            auto backOffset = subPacket->getBackOffset();
            auto clonedContent = cloneChunkDeep(subPacket->peekAll());
            subPacket->eraseAll();
            subPacket->insertAll(clonedContent);
            subPacket->setFrontOffset(frontOffset);
            subPacket->setBackOffset(backOffset);
            if (subPacket->getDataLength() >= B(4)) {
                auto macTrailer = subPacket->removeAtBack<ieee80211::Ieee80211MacTrailer>(B(4));
                macTrailer->setFcsMode(FCS_DECLARED_CORRECT);
                subPacket->insertAtBack(macTrailer);
            }
            take(subPacket);
            auto phyHeader = makeShared<Ieee80211HeMuPhyHeader>();
            phyHeader->setRuIndex(ruIndex);
            for (const auto& a : allocations) {
                Ieee80211HeMuRuAllocationInfo info;
                info.ruIndex = a.ruIndex;
                const auto& macHdr = a.packet->peekAtFront<ieee80211::Ieee80211MacHeader>();
                info.staAddress = macHdr->getReceiverAddress();
                phyHeader->appendAllocations(info);
            }
            phyHeader->setLengthField(B(subPacket->getDataLength()));
            phyHeader->setChunkLength(b(16 + phyHeader->getAllocationsArraySize() * 56));
            subPacket->insertAtFront(phyHeader);
            subPacket->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211HePhy);

            auto ruTransmission = new Ieee80211Transmission(
                transmitterRadio,
                subPacket,
                transmission->getStartTime(),
                transmission->getEndTime(),
                transmission->getPreambleDuration(),
                transmission->getHeaderDuration(),
                transmission->getDataDuration(),
                transmission->getStartPosition(),
                transmission->getEndPosition(),
                transmission->getStartOrientation(),
                transmission->getEndOrientation(),
                nullptr, nullptr, nullptr, nullptr, // models
                ruAnalogModel,
                ieee80211Trans->getMode(),
                ieee80211Trans->getChannel()
            );

            subTransmissions.push_back(ruTransmission);
            // Add the sub-transmission to the medium
            RadioMedium::addTransmission(transmitterRadio, ruTransmission);
        }

        muSubTransmissions[transmission] = subTransmissions;
        // Keep the main transmission registered on the medium for compatibility/logical tracking
        RadioMedium::addTransmission(transmitterRadio, transmission);

        // Audit power conservation: accumulated RU power should match total power
        double powerRatio = accumulatedPower.get() / totalPower.get();
        if (powerRatio < 0.99 || powerRatio > 1.01) {
            EV_WARN << "Power conservation drift detected: accumulated " << accumulatedPower.get() << " W "
                    << "vs. total " << totalPower.get() << " W (ratio: " << powerRatio << ")\n";
        } else {
            EV_DEBUG << "Power conservation check passed: accumulated " << accumulatedPower.get() << " W "
                     << "matches total " << totalPower.get() << " W\n";
        }
    }
    else {
        RadioMedium::addTransmission(transmitterRadio, transmission);
    }
}

void Ieee80211RadioMedium::removeTransmission(const ITransmission *transmission)
{
    auto it = muSubTransmissions.find(transmission);
    if (it != muSubTransmissions.end()) {
        for (auto subTransmission : it->second) {
            auto pkt = subTransmission->getPacket();
            delete const_cast<Packet *>(pkt);
            RadioMedium::removeTransmission(subTransmission);
        }
        muSubTransmissions.erase(it);
    }
    RadioMedium::removeTransmission(transmission);
}

void Ieee80211RadioMedium::sendToRadio(IRadio *transmitter, const IRadio *receiver, const IWirelessSignal *signal)
{
    auto mainTransmission = signal->getTransmission();
    auto it = muSubTransmissions.find(mainTransmission);
    if (it != muSubTransmissions.end()) {
        for (auto subTransmission : it->second) {
            auto subSignal = new WirelessSignal(subTransmission);
            RadioMedium::sendToRadio(transmitter, receiver, subSignal);
            delete subSignal;
        }
    }
    else {
        RadioMedium::sendToRadio(transmitter, receiver, signal);
    }
}

Ieee80211RadioMedium::~Ieee80211RadioMedium()
{
    for (const auto& pair : muSubTransmissions) {
        for (auto subTransmission : pair.second) {
            auto pkt = subTransmission->getPacket();
            delete const_cast<Packet *>(pkt);
        }
    }
}

void Ieee80211RadioMedium::initialize(int stage)
{
    RadioMedium::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        subscribe(IRadioMedium::signalRemovedSignal, this);
    }
}

void Ieee80211RadioMedium::receiveSignal(cComponent *source, simsignal_t signal, cObject *value, cObject *details)
{
    if (signal == IRadioMedium::signalRemovedSignal) {
        auto transmission = dynamic_cast<const ITransmission *>(value);
        if (transmission != nullptr) {
            auto it = muSubTransmissions.find(transmission);
            if (it != muSubTransmissions.end()) {
                for (auto subTransmission : it->second) {
                    auto pkt = subTransmission->getPacket();
                    delete const_cast<Packet *>(pkt);
                }
                muSubTransmissions.erase(it);
            }
        }
    }
    else {
        RadioMedium::receiveSignal(source, signal, value, details);
    }
}

bool Ieee80211RadioMedium::isPotentialReceiver(const IRadio *receiver, const ITransmission *transmission) const
{
    // A main HE MU transmission should not propagate physically (only its sub-transmissions should)
    if (transmission->getPacket() != nullptr && transmission->getPacket()->findTag<Ieee80211HeMuTag>() != nullptr)
        return false;
    return RadioMedium::isPotentialReceiver(receiver, transmission);
}

bool Ieee80211RadioMedium::isInterferingTransmission(const ITransmission *transmission, const IListening *listening) const
{
    // A main HE MU transmission does not cause interference directly (its sub-transmissions do)
    if (transmission->getPacket() != nullptr && transmission->getPacket()->findTag<Ieee80211HeMuTag>() != nullptr)
        return false;
    return RadioMedium::isInterferingTransmission(transmission, listening);
}

bool Ieee80211RadioMedium::isInterferingTransmission(const ITransmission *transmission, const IReception *reception) const
{
    // A main HE MU transmission does not cause interference directly (its sub-transmissions do)
    if (transmission->getPacket() != nullptr && transmission->getPacket()->findTag<Ieee80211HeMuTag>() != nullptr)
        return false;

    // Sub-transmissions of the same MU transmission do not interfere with each other
    auto desiredTransmission = reception->getTransmission();
    for (const auto& pair : muSubTransmissions) {
        bool transmissionIsSub = false;
        bool desiredIsSub = false;
        for (auto sub : pair.second) {
            if (sub == transmission)
                transmissionIsSub = true;
            if (sub == desiredTransmission)
                desiredIsSub = true;
        }
        if (transmissionIsSub && desiredIsSub) {
            return false;
        }
    }

    return RadioMedium::isInterferingTransmission(transmission, reception);
}

} // namespace physicallayer
} // namespace inet
