// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRecoverySource.h"
#include "inet/linklayer/lowpan/LowpanSelectiveRecovery.h"
#include "inet/linklayer/lowpan/LowpanLayer.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"
#include "inet/linklayer/lowpan/LowpanIpv6.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/common/ProtocolTag_m.h"

namespace inet { namespace lowpan {
std::vector<LowpanRecoverySource::Range> LowpanRecoverySource::partition(int size, int header, int firstMaximum,
        int minimum, int optimum, int maximum)
{
    if (size < 1 || size > 65535 || header < 2 || minimum < 1 || optimum < minimum || maximum < optimum || maximum > 1023)
        return {};
    firstMaximum = std::min(firstMaximum, maximum);
    // Choose the fewest feasible sequences, then the largest feasible first/ranges.
    for (int count = 2; count <= 32; count++) {
        int low = std::max({minimum, header, size - (count - 1) * maximum});
        int high = std::min(firstMaximum, size - (count - 1) * minimum);
        if (low > high) continue;
        int first = high;
        std::vector<Range> result{{0, first}};
        int remaining = size - first, offset = first;
        for (int left = count - 1; left > 0; left--) {
            int length = std::min(maximum, std::max(minimum, std::min(optimum, remaining - (left - 1) * minimum)));
            length = std::max(length, remaining - (left - 1) * maximum);
            result.push_back({offset, length}); offset += length; remaining -= length;
        }
        if (remaining == 0) return result;
    }
    return {};
}

LowpanRecoverySource::LowpanRecoverySource(LowpanSelectiveRecovery& recovery) : recovery(recovery)
{
    auto& owner = recovery.owner;
    window = owner.par("recoveryWindow"); minimum = owner.par("recoveryMinFragmentSize");
    optimum = owner.par("recoveryOptFragmentSize"); maximum = owner.par("recoveryMaxFragmentSize");
    operationLifetime = recovery.quantizeDuration(owner.par("recoveryOperationLifetime"));
    initialRto = recovery.quantizeDuration(owner.par("recoveryInitialRto"));
    minimumRto = recovery.quantizeDuration(owner.par("recoveryMinRto"));
    maximumRto = recovery.quantizeDuration(owner.par("recoveryMaxRto"));
    if (window < 1 || window > 32 || minimum < 1 || minimum > optimum || optimum > maximum || maximum >= 512 ||
            operationLifetime > recovery.quantizeDuration(45) || minimumRto < recovery.quantizeDuration(.85) ||
            initialRto < minimumRto || initialRto > maximumRto || maximumRto > recovery.quantizeDuration(9.6) ||
            recovery.historyLifetime < operationLifetime)
        throw cRuntimeError("Invalid selective recovery source profile");
    auto required = recovery.addTime(operationLifetime, recovery.quantizeDuration(.4));
    required = recovery.addTime(required, recovery.historyLifetime);
    required = recovery.addTime(required, recovery.quantizeDuration(.4));
    if (recovery.tagProtection < required)
        throw cRuntimeError("Recovery tag protection must cover operation, both declared paths and history");
}
int LowpanRecoverySource::getRetainedBytes() const
{
    int bytes = 0;
    for (const auto& item : operations)
        bytes += item.second.original->getDataLength().get<B>() + item.second.header->getChunkLength().get<B>();
    return bytes;
}
simtime_t LowpanRecoverySource::computeResponseInterval(int attempts) const
{
    auto result = initialRto;
    for (int i = 1; i < attempts; i++) result = std::min(maximumRto, recovery.addTime(result, result));
    return std::max(minimumRto, result);
}
void LowpanRecoverySource::admit(Packet *packet, const Ieee802154AddressReq& peer, int nativePayloadLimit)
{
    auto& owner = recovery.owner;
    auto header = LowpanIphcCodec::encode(*packet->peekAtFront<Ipv6Header>(B(40)), peer.getSrcAddress(), peer.getDestAddress());
    int length = header ? header->getChunkLength().get<B>() : 0;
    int size = packet->getDataLength().get<B>() - 40 + length;
    int budget = std::min(nativePayloadLimit, recovery.pathPayloadLimit);
    if (recovery.payloadLimit != 0) budget = std::min(budget, recovery.payloadLimit);
    auto ranges = partition(size, length, std::min(optimum, budget - 6 - 17), minimum, optimum, maximum);
    if (!header || header->getNh() || header->getCid() || header->getSac() || header->getDac() || header->getM() ||
            ranges.empty() || maximum + 6 > budget || !recovery.hasCapacity(packet->getDataLength().get<B>() + length, true)) {
        owner.dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); return;
    }
    if (!owner.isNativeTransmissionOperational()) { owner.dropPacket(packet, INTERFACE_DOWN); return; }
    auto token = recovery.nextToken++;
    auto tag = recovery.allocateTag(owner.getId(), token, peer);
    if (!tag) { owner.dropLowpanPacket(packet, LOWPAN_TAG_EXHAUSTED); return; }
    LowpanSelectiveRecovery::Key key{peer.getSrcAddress(), peer.getDestAddress(), peer.getSrcPanId(), *tag};
    LowpanSelectiveRecovery::Record record;
    record.phase = LowpanSelectiveRecovery::SOURCE; record.token = token;
    record.activeEnd = recovery.addTime(simTime(), operationLifetime);
    record.historyEnd = recovery.addTime(simTime(), recovery.historyLifetime);
    record.outgoingTag = *tag; record.path.layerId = owner.getId(); record.path.peer = peer;
    recovery.records.emplace(key, std::move(record));
    Operation operation;
    operation.original.reset(packet); operation.header = header; operation.peer = peer;
    operation.ranges = std::move(ranges); operation.tag = *tag; operation.size = size;
    operation.batchEnd = std::min(window, int(operation.ranges.size()));
    operation.deadline = recovery.addTime(simTime(), operationLifetime); operation.nextSend = simTime();
    operations.emplace(token, std::move(operation));
    // Observation owns its snapshot independently of a reentrant STOP deleting the operation.
    auto observation = std::unique_ptr<Packet>(packet->dup());
    owner.emit(owner.datagramAcceptedSignal, observation.get());
    if (operations.count(token)) sendNext(token);
    recovery.schedule();
}
void LowpanRecoverySource::retire(int64_t token, bool successful)
{
    auto found = operations.find(token); if (found == operations.end()) return;
    auto original = std::move(found->second.original);
    auto routing = found->second.header;
    auto peer = found->second.peer;
    auto tag = found->second.tag;
    auto frame = found->second.inFlight;
    operations.erase(found);
    recovery.owner.forwardedTransmissions.erase(frame);
    recovery.finishSource(token, successful);
    if (!successful && recovery.owner.isNativeTransmissionOperational()) {
        auto reset = makeShared<LowpanRfragHeader>(); reset->setDatagramTag(tag);
        Packet cleanup("recovery-source-reset", routing); cleanup.insertAtFront(reset);
        recovery.queueControl(peer, cleanup.peekData());
    }
    if (successful) recovery.owner.handlePacketProcessed(original.get());
    else recovery.owner.dropLowpanPacket(original.release(), LOWPAN_RECOVERY_ABORTED);
}
bool LowpanRecoverySource::isOperationValid(const Operation& operation) const
{
    auto native = check_and_cast<LowpanNativeBoundary *>(recovery.owner.link);
    return recovery.owner.isNativeTransmissionOperational() && operation.deadline > simTime() &&
        operation.peer.getSrcAddress() == native->getLinkAddress() && operation.peer.getSrcPanId() == native->getPanId() &&
        operation.peer.getDestPanId() == native->getPanId() && native->getLinkDomain()->contains(operation.peer.getDestAddress()) &&
        recovery.ipv6->isNativeSourceUsable(recovery.owner.networkInterface, operation.original->peekAtFront<Ipv6Header>(B(40))->getSrcAddress());
}
void LowpanRecoverySource::sendNext(int64_t token)
{
    auto found = operations.find(token); if (found == operations.end()) return;
    auto& operation = found->second;
    if (!isOperationValid(operation)) { retire(token, false); return; }
    if (operation.inFlight >= 0 || operation.waiting || operation.nextSend > simTime()) return;
    int sequence = -1;
    while (operation.cursor < operation.batchEnd) {
        int candidate = operation.cursor++;
        if ((operation.probe || !(operation.credit & (uint32_t(1) << (31 - candidate)))) && operation.attempts[candidate] < 4) { sequence = candidate; break; }
    }
    if (sequence < 0) {
        // No positive credit means that missing work must await the bounded X response timer.
        operation.waiting = true;
        operation.xSequence = operation.batchEnd - 1;
        operation.responseEnd = recovery.addTime(simTime(), computeResponseInterval(operation.attempts[operation.xSequence]));
        return;
    }
    int next = operation.cursor;
    while (next < operation.batchEnd && ((operation.credit & (uint32_t(1) << (31 - next))) || operation.attempts[next] >= 4)) next++;
    bool request = next == operation.batchEnd;
    operation.probe = false;
    auto range = operation.ranges[sequence];
    auto frame = std::make_unique<Packet>(operation.original->getName());
    int encoded = operation.header->getChunkLength().get<B>();
    if (range.offset < encoded) frame->insertAtBack(operation.header);
    int tailStart = std::max(range.offset, encoded), tailEnd = range.offset + range.size;
    if (tailEnd > tailStart) {
        frame->insertAtBack(operation.original->peekDataAt(B(40 + tailStart - encoded), B(tailEnd - tailStart)));
        frame->copyRegionTags(*operation.original, operation.original->getFrontOffset() + B(40 + tailStart - encoded),
            B(tailStart - range.offset), B(tailEnd - tailStart));
    }
    auto header = makeShared<LowpanRfragHeader>();
    header->setDatagramTag(operation.tag); header->setSequence(sequence); header->setFragmentSize(range.size);
    header->setFragmentOffset(sequence == 0 ? operation.size : range.offset); header->setAckRequested(request);
    frame->insertAtFront(header);
    auto envelope = makeShared<LowpanTransmissionReq>(*recovery.owner.link->prepareTransmission(operation.peer));
    envelope->setDeliveryDeadline(std::min({envelope->getDeliveryDeadline(), operation.deadline, recovery.addTime(simTime(), recovery.residence)}));
    *frame->addTag<LowpanTransmissionReq>() = *envelope;
    frame->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    auto id = frame->getId();
    operation.attempts[sequence]++; operation.inFlight = id;
    operation.xSequence = request ? sequence : -1;
    auto observation = std::unique_ptr<Packet>(frame->dup());
    recovery.owner.emit(recovery.owner.fragmentSentSignal, observation.get());
    found = operations.find(token);
    if (found == operations.end()) return;
    if (!isOperationValid(found->second)) { retire(token, false); return; }
    // Cross-layer dispatch also handles this same-Layer source; ID is installed before lower transfer.
    bool accepted = recovery.owner.pushForwardedPacket(frame.release(), recovery.owner.getId(), token, true);
    if (!accepted) handleTransmissionCompleted(token, id, false);
}
void LowpanRecoverySource::handleTransmissionCompleted(int64_t token, int64_t frameId, bool)
{
    auto found = operations.find(token); if (found == operations.end() || found->second.inFlight != frameId) return;
    if (!isOperationValid(found->second)) { retire(token, false); return; }
    auto& operation = found->second;
    operation.inFlight = -1; operation.nextSend = recovery.addTime(simTime(), recovery.gap);
    if (operation.xSequence >= 0) {
        operation.waiting = true;
        operation.responseEnd = recovery.addTime(simTime(), computeResponseInterval(operation.attempts[operation.xSequence]));
    }
    recovery.schedule();
}
void LowpanRecoverySource::handleAck(int64_t token, uint32_t bitmap)
{
    auto found = operations.find(token); if (found == operations.end()) return;
    auto& operation = found->second;
    if (!isOperationValid(operation)) { retire(token, false); return; }
    if (bitmap == 0) { retire(token, false); return; }
    if (bitmap == UINT32_MAX) {
        for (size_t i = 0; i < operation.ranges.size(); i++) if (operation.attempts[i] == 0) return;
        retire(token, true); return;
    }
    uint32_t valid = operation.ranges.size() == 32 ? UINT32_MAX : UINT32_MAX << (32 - operation.ranges.size());
    if (bitmap & ~valid) return;
    for (size_t i = 0; i < operation.ranges.size(); i++)
        if ((bitmap & (uint32_t(1) << (31 - i))) && operation.attempts[i] == 0) return;
    for (size_t i = 0; i < operation.ranges.size(); i++)
        if (operation.attempts[i]) operation.credit |= bitmap & (uint32_t(1) << (31 - i));
    if (operation.xSequence >= 0 && (operation.credit & (uint32_t(1) << (31 - operation.xSequence)))) advanceBatch(operation);
    recovery.schedule();
}
void LowpanRecoverySource::advanceBatch(Operation& operation)
{
    int count = operation.ranges.size();
    int begin = operation.batchEnd;
    if (begin == count) { operation.initialPass = false; begin = 0; }
    // During initial pass every new sequence precedes repair of a missing earlier one.
    if (!operation.initialPass) {
        int scanned = 0;
        while (scanned < count && (operation.credit & (uint32_t(1) << (31 - begin)))) { begin = (begin + 1) % count; scanned++; }
        if (scanned == count) {
            operation.waitFull = true;
            return; // Retain the armed X response deadline; its bounded probe obtains lost FULL.
        }
    }
    operation.batchBegin = begin; operation.batchEnd = std::min(count, begin + window);
    operation.cursor = begin; operation.waiting = false; operation.xSequence = -1;
}
void LowpanRecoverySource::handleTimer()
{
    std::vector<int64_t> tokens;
    for (const auto& item : operations) tokens.push_back(item.first);
    for (auto token : tokens) {
        auto found = operations.find(token); if (found == operations.end()) continue;
        auto& operation = found->second;
        if (operation.deadline <= simTime() || !recovery.owner.isNativeTransmissionOperational()) { retire(token, false); continue; }
        if (operation.waiting && operation.responseEnd <= simTime()) {
            operation.waiting = false;
            // If all data is credited, probe the batch's X sequence until FULL or the cumulative cap.
            if (operation.waitFull || operation.initialPass) {
                int sequence = operation.batchEnd - 1;
                if (operation.attempts[sequence] >= 4) { retire(token, false); continue; }
                operation.probe = true; operation.cursor = sequence;
            }
            else operation.cursor = operation.batchBegin;
            bool remaining = false;
            for (int i = operation.cursor; i < operation.batchEnd; i++)
                if ((operation.probe || !(operation.credit & (uint32_t(1) << (31 - i)))) && operation.attempts[i] < 4) remaining = true;
            if (!remaining) { retire(token, false); continue; }
        }
        sendNext(token);
    }
}
std::optional<simtime_t> LowpanRecoverySource::getNextEvent() const
{
    std::optional<simtime_t> next;
    for (const auto& item : operations) {
        const auto& operation = item.second;
        auto when = operation.deadline;
        if (operation.waiting) when = std::min(when, operation.responseEnd);
        else if (operation.inFlight < 0) when = std::min(when, operation.nextSend);
        if (!next || when < *next) next = when;
    }
    return next;
}
void LowpanRecoverySource::clear()
{
    std::vector<int64_t> tokens;
    for (const auto& item : operations) tokens.push_back(item.first);
    for (auto token : tokens) retire(token, false);
}
} }
