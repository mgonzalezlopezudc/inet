//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Tx.h"

#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/checksum/Checksum.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"

namespace inet {
namespace ieee80211 {

Define_Module(Tx);

static bool isControlResponseFrame(const Ptr<const Ieee80211MacHeader>& header)
{
    return dynamicPtrCast<const Ieee80211AckFrame>(header) ||
           dynamicPtrCast<const Ieee80211CtsFrame>(header) ||
           dynamicPtrCast<const Ieee80211BlockAck>(header);
}

Tx::~Tx()
{
    cancelAndDelete(endIfsTimer);
    if (frame)
        delete frame;
}

void Tx::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        mac = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this)->getSubmodule("mac"));
        endIfsTimer = new cMessage("endIFS");
        rx = dynamic_cast<IRx *>(findModuleByPath(par("rxModule")));
        WATCH(transmitting);
        WATCH_EXPR("txState", endIfsTimer != nullptr && endIfsTimer->isScheduled() ? "WAIT_IFS" : transmitting ? "TRANSMIT" : "IDLE");
        WATCH_EXPR("txFramePrefix", frame ? std::string(frame->getName()) + "\n" : "");
    }
}

bool Tx::isBusy() const
{
    return txCallback != nullptr || transmitting || (endIfsTimer != nullptr && endIfsTimer->isScheduled());
}

class Tx::PreparedTransmissionImpl : public ITx::PreparedTransmission
{
  public:
    std::unique_ptr<Packet> frame;
    std::unique_ptr<Packet> immediateFrame;
    Ptr<const Ieee80211MacHeader> header;
    simtime_t ifs = SIMTIME_ZERO;
    ITx::ICallback *callback = nullptr;
    bool ampdu = false;
};

void Tx::transmitFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, ITx::ICallback *txCallback)
{
    transmitFrame(packet, header, SIMTIME_ZERO, txCallback);
}

void Tx::transmitFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, simtime_t ifs, ITx::ICallback *txCallback)
{
    Enter_Method("transmitFrame(\"%s\")", packet->getName());
    auto prepared = prepareTransmission(packet, header, ifs, txCallback);
    commitTransmission(std::move(prepared));
}

std::unique_ptr<ITx::PreparedTransmission> Tx::prepareTransmission(
        Packet *packet, const Ptr<const Ieee80211MacHeader>& header,
        simtime_t ifs, ITx::ICallback *txCallback)
{
    Enter_Method("prepareTransmission(\"%s\")", packet->getName());
    if (packet == nullptr || header == nullptr || txCallback == nullptr ||
            ifs < SIMTIME_ZERO || mac == nullptr || rx == nullptr ||
            endIfsTimer == nullptr || endIfsTimer->isScheduled() || isBusy())
        throw cRuntimeError("Cannot prepare an invalid or overlapping IEEE 802.11 transmission");
    auto prepared = std::make_unique<PreparedTransmissionImpl>();
    prepared->ifs = ifs;
    prepared->callback = txCallback;
    prepared->frame.reset(packet->dup());
    auto workingPacket = prepared->frame.get();
    auto macAddressInd = workingPacket->addTagIfAbsent<MacAddressInd>();
    if (auto oneAddressHeader = dynamicPtrCast<const Ieee80211OneAddressHeader>(header)) {
        macAddressInd->setDestAddress(oneAddressHeader->getReceiverAddress());
    }
    if (auto twoAddressHeader = dynamicPtrCast<const Ieee80211TwoAddressHeader>(header)) {
        macAddressInd->setSrcAddress(twoAddressHeader->getTransmitterAddress());
    }
    // An HE TB NDP (preamble-only) carries no PSDU: the packet is empty.
    // Skip A-MPDU detection and MAC header/trailer stamping for such frames.
    bool isNdp = (workingPacket->getDataLength() == B(0));
    prepared->ampdu = !isNdp && dynamicPtrCast<const Ieee80211MpduSubframeHeader>(workingPacket->peekAtFront()) != nullptr;
    if (!prepared->ampdu && !isNdp) {
        const auto& updatedHeader = workingPacket->removeAtFront<Ieee80211MacHeader>();
        if (auto twoAddressHeader = dynamicPtrCast<Ieee80211TwoAddressHeader>(updatedHeader)) {
            twoAddressHeader->setTransmitterAddress(mac->getAddress());
            macAddressInd->setSrcAddress(twoAddressHeader->getTransmitterAddress());
        }
        workingPacket->insertAtFront(updatedHeader);
        const auto& updatedTrailer = workingPacket->removeAtBack<Ieee80211MacTrailer>(B(4));
        updatedTrailer->setFcsMode(mac->getFcsMode());
        if (mac->getFcsMode() == FCS_COMPUTED) {
            const auto& fcsBytes = workingPacket->peekAllAsBytes();
            auto bufferLength = fcsBytes->getChunkLength().get<B>();
            auto buffer = new uint8_t[bufferLength];
            fcsBytes->copyToBuffer(buffer, bufferLength);
            auto fcs = ethernetFcs(buffer, bufferLength);
            updatedTrailer->setFcs(fcs);
            delete[] buffer;
        }
        workingPacket->insertAtBack(updatedTrailer);
    }
    // Store the passed-in header for A-MPDU and NDP frames (both have no peekable MAC header).
    prepared->header = (prepared->ampdu || isNdp) ? header : nullptr;
    if (ifs == SIMTIME_ZERO)
        prepared->immediateFrame.reset(prepared->frame->dup());
    return prepared;
}

void Tx::commitTransmission(
        std::unique_ptr<ITx::PreparedTransmission> basePrepared) noexcept
{
    // All recoverable validation, packet allocation/copying and stamping was
    // completed by prepareTransmission(). This final boundary only moves the
    // prepared state and publishes one scheduler event (or one immediate
    // send). An OMNeT++ invariant or allocator failure here is process-fatal;
    // it is not reported as a recoverable protocol-transaction failure.
    auto prepared = static_cast<PreparedTransmissionImpl *>(basePrepared.get());
    ASSERT(prepared != nullptr && !isBusy() && !endIfsTimer->isScheduled() &&
            !transmitting && prepared->frame != nullptr &&
            prepared->callback != nullptr);
    txCallback = prepared->callback;
    frame = prepared->frame.release();
    frameHeader = prepared->header;
    frameIsAmpdu = prepared->ampdu;
    if (prepared->ifs == SIMTIME_ZERO) {
        // do directly what handleMessage() would do
        transmitting = true;
        mac->sendDownFrame(prepared->immediateFrame.release());
    }
    else
        scheduleAfter(prepared->ifs, endIfsTimer);
}

void Tx::radioTransmissionFinished()
{
    Enter_Method("radioTransmissionFinished");
    if (transmitting) {
        EV_DETAIL << "Tx: radioTransmissionFinished()\n";
        transmitting = false;
        ASSERT(txCallback != nullptr);
        const auto& header = (frameIsAmpdu || frame->getDataLength() == B(0)) ? frameHeader : frame->peekAtFront<Ieee80211MacHeader>();
        auto duration = header->getDurationField();
        auto tmpFrame = frame;
        auto tmpTxCallback = txCallback;
        frame = nullptr;
        frameHeader = nullptr;
        frameIsAmpdu = false;
        txCallback = nullptr;
        tmpTxCallback->transmissionComplete(tmpFrame, header);
        delete tmpFrame;
        if (!isControlResponseFrame(header))
            rx->frameTransmitted(duration);
    }
}

void Tx::handleMessage(cMessage *msg)
{
    if (msg == endIfsTimer) {
        EV_DETAIL << "Tx: endIfsTimer expired\n";
        transmitting = true;
        mac->sendDownFrame(frame->dup());
    }
    else
        ASSERT(false);
}

} // namespace ieee80211
} // namespace inet
