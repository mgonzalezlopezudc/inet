// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanSourceFragmentation.h"
#include "inet/linklayer/lowpan/LowpanLayer.h"
#include "inet/linklayer/lowpan/LowpanNativeBoundary.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Simsignals.h"
#include <cmath>

namespace inet { namespace lowpan {
namespace {
simtime_t duration(double seconds)
{
    if (!std::isfinite(seconds) || seconds < SimTime::fromRaw(1).dbl())
        throw cRuntimeError("Paced source durations must be finite and at least one simulation tick");
    return seconds >= SimTime::getMaxTime().dbl() ? SimTime::getMaxTime() : SimTime(seconds);
}
simtime_t deadline(simtime_t now, simtime_t interval)
{
    auto maximum = SimTime::getMaxTime().raw();
    return SimTime::fromRaw(interval.raw() > maximum - now.raw() ? maximum : now.raw() + interval.raw());
}
}
LowpanSourceFragmentation::LowpanSourceFragmentation(LowpanLayer& owner) : owner(owner)
{
    maxDatagrams = owner.par("maxPacedSourceDatagrams");
    maxBytes = owner.par("maxPacedSourceBytes");
    gap = duration(owner.par("sourceFragmentGap").doubleValue());
    residence = duration(owner.par("sourceFragmentResidence").doubleValue());
    protection = duration(owner.par("sourceTagProtection").doubleValue());
    if (maxDatagrams <= 0 || maxBytes < 40 || protection < deadline(residence, duration(60)))
        throw cRuntimeError("Invalid paced source limits: tag protection must cover residence plus 60 seconds");
    interfaceNotifications = check_and_cast<cModule *>(owner.networkInterface->getInterfaceTable());
    interfaceNotifications->subscribe(interfaceStateChangedSignal, this);
    timer = new cMessage("lowpanSourceFragments");
}
LowpanSourceFragmentation::~LowpanSourceFragmentation()
{
    if (interfaceNotifications) interfaceNotifications->unsubscribe(interfaceStateChangedSignal, this);
    owner.cancelAndDelete(timer);
}
void LowpanSourceFragmentation::receiveSignal(cComponent *, simsignal_t signal, cObject *object, cObject *)
{
    auto change = check_and_cast<NetworkInterfaceChangeDetails *>(object);
    if (signal == interfaceStateChangedSignal && change->getNetworkInterface() == owner.networkInterface &&
            owner.networkInterface->getState() == NetworkInterface::DOWN) {
        cMethodCallContextSwitcher context(&owner);
        context.methodCallSilent();
        clear(); // Includes ACK-to-next-send gaps, when no MAC original exists to report failure.
    }
}
void LowpanSourceFragmentation::clear()
{
    owner.cancelEvent(timer);
    frames.clear();
    flows.clear();
    retainedBytes = 0;
}
void LowpanSourceFragmentation::schedule()
{
    owner.cancelEvent(timer);
    if (flows.empty()) return;
    auto next = SimTime::getMaxTime();
    for (const auto& item : flows)
        next = std::min(next, item.second.inFlight < 0 ? std::min(item.second.nextSend, item.second.deadline) : item.second.deadline);
    owner.scheduleAt(next, timer);
}
void LowpanSourceFragmentation::retire(int64_t token, bool successful, LowpanDropReason reason)
{
    auto found = flows.find(token);
    if (found == flows.end()) return;
    auto original = std::move(found->second.original);
    frames.erase(found->second.inFlight);
    retainedBytes -= original->getDataLength().get<B>();
    flows.erase(found);
    // Detach all state before observations or upstream callbacks can reenter.
    if (successful) owner.handlePacketProcessed(original.get());
    else owner.dropLowpanPacket(original.release(), reason);
}
void LowpanSourceFragmentation::admit(Packet *packet, const Ptr<const Chunk>& header, int headerLength,
        const std::vector<LowpanFragmenter::Range>& ranges, const Ieee802154AddressReq& peer, int payloadLimit)
{
    std::vector<int64_t> expired;
    for (const auto& item : flows)
        if (item.second.deadline <= simTime()) expired.push_back(item.first);
    for (auto token : expired) retire(token, false, LOWPAN_LOWER_LIFETIME_EXPIRED);
    if (!owner.isNativeTransmissionOperational()) { owner.dropPacket(packet, INTERFACE_DOWN); return; }
    auto end = deadline(simTime(), residence);
    if (end <= simTime()) { owner.dropLowpanPacket(packet, LOWPAN_LOWER_LIFETIME_EXPIRED); return; }
    int bytes = packet->getDataLength().get<B>();
    if ((int)flows.size() >= maxDatagrams || bytes > maxBytes - retainedBytes) {
        owner.dropLowpanPacket(packet, LOWPAN_RESOURCE_LIMIT); return;
    }
    auto tag = owner.allocateDatagramTag(simTime(), deadline(simTime(), protection));
    if (!tag) { owner.dropLowpanPacket(packet, LOWPAN_TAG_EXHAUSTED); return; }
    int64_t token = packet->getId();
    Flow flow;
    flow.original.reset(packet); flow.header = header; flow.ranges = ranges; flow.peer = peer;
    flow.headerLength = headerLength; flow.payloadLimit = payloadLimit; flow.tag = *tag;
    flow.deadline = end; flow.nextSend = simTime();
    retainedBytes += bytes;
    flows.emplace(token, std::move(flow));
    // A listener may cancel the flow; all listeners still borrow a live snapshot.
    auto accepted = std::unique_ptr<Packet>(packet->dup());
    owner.emit(LowpanLayer::datagramAcceptedSignal, accepted.get());
    schedule(); // May have been removed by an observer; no borrowed Flow survives.
}
void LowpanSourceFragmentation::sendNext(int64_t token)
{
    auto found = flows.find(token);
    if (found == flows.end()) return;
    auto& flow = found->second;
    if (simTime() >= flow.deadline) { retire(token, false, LOWPAN_LOWER_LIFETIME_EXPIRED); return; }
    if (!owner.isNativeTransmissionOperational() ||
            flow.peer.getSrcAddress() != owner.link->getLinkAddress() || flow.peer.getSrcPanId() != owner.link->getPanId() ||
            !check_and_cast<LowpanNativeBoundary *>(owner.link)->getLinkDomain()->contains(flow.peer.getDestAddress())) {
        retire(token, false); return;
    }
    auto envelope = makeShared<LowpanTransmissionReq>(*owner.link->prepareTransmission(flow.peer));
    if (envelope->getPayloadLimit() != flow.payloadLimit) { retire(token, false); return; }
    envelope->setDeliveryDeadline(std::min(envelope->getDeliveryDeadline(), flow.deadline));
    auto range = flow.ranges.at(flow.next);
    auto fragment = new Packet((std::string(flow.original->getName()) + "-paced-" + std::to_string(range.offset)).c_str());
    int offset = range.offset == 0 ? flow.headerLength : range.offset;
    int length = range.length - (range.offset == 0 ? flow.headerLength : 0);
    if (length != 0) {
        fragment->insertAtBack(flow.original->peekDataAt(B(offset), B(length)));
        fragment->copyRegionTags(*flow.original, flow.original->getFrontOffset() + B(offset), b(0), B(length));
    }
    if (range.offset == 0) {
        fragment->insertAtFront(flow.header);
        auto first = makeShared<LowpanFrag1Header>();
        first->setDatagramSize(flow.original->getDataLength().get<B>()); first->setDatagramTag(flow.tag);
        fragment->insertAtFront(first);
    }
    else {
        auto next = makeShared<LowpanFragnHeader>();
        next->setDatagramSize(flow.original->getDataLength().get<B>()); next->setDatagramTag(flow.tag); next->setDatagramOffset(range.offset / 8);
        fragment->insertAtFront(next);
    }
    *fragment->addTag<LowpanTransmissionReq>() = *envelope;
    fragment->addTag<PacketProtocolTag>()->setProtocol(&lowpanProtocol);
    auto id = fragment->getId();
    flow.inFlight = id;
    frames.emplace(id, token);
    owner.emit(LowpanLayer::fragmentSentSignal, fragment);
    // An observer may stop the interface and clear the flow. It owns no frame.
    if (!flows.count(token)) { delete fragment; return; }
    if (!owner.pushPreparedPacket(fragment)) retire(token, false);
    // A synchronous callback can already have deleted fragment and retired flow.
}
void LowpanSourceFragmentation::handleTimer()
{
    std::vector<int64_t> tokens;
    for (const auto& item : flows) tokens.push_back(item.first);
    for (auto token : tokens) {
        auto found = flows.find(token);
        if (found == flows.end()) continue;
        if (found->second.deadline <= simTime()) retire(token, false, LOWPAN_LOWER_LIFETIME_EXPIRED);
        else if (found->second.inFlight < 0 && found->second.nextSend <= simTime()) sendNext(token);
    }
    schedule();
}
void LowpanSourceFragmentation::completed(int64_t id, bool successful)
{
    auto frame = frames.find(id);
    if (frame == frames.end()) return;
    auto token = frame->second;
    frames.erase(frame);
    auto found = flows.find(token);
    if (found == flows.end()) return;
    found->second.inFlight = -1;
    if (found->second.deadline <= simTime()) retire(token, false, LOWPAN_LOWER_LIFETIME_EXPIRED);
    else if (!successful) retire(token, false);
    else if (++found->second.next == found->second.ranges.size()) retire(token, true);
    else found->second.nextSend = deadline(simTime(), gap);
    schedule();
}
} }
