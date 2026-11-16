// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANSOURCEFRAGMENTATION_H
#define __INET_LOWPANSOURCEFRAGMENTATION_H
#include <map>
#include "inet/linklayer/lowpan/LowpanFragmenter.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"

namespace inet { namespace lowpan {
class LowpanLayer;

// Owns admitted source datagrams; lower originals are borrowed only at completion.
class INET_API LowpanSourceFragmentation : public cListener
{
  protected:
    struct Flow {
        std::unique_ptr<Packet> original;
        Ptr<const Chunk> header;
        std::vector<LowpanFragmenter::Range> ranges;
        Ieee802154AddressReq peer;
        int headerLength;
        int payloadLimit;
        uint16_t tag;
        size_t next = 0;
        int64_t inFlight = -1;
        simtime_t deadline, nextSend;
    };
    LowpanLayer& owner;
    std::map<int64_t, Flow> flows;
    std::map<int64_t, int64_t> frames;
    cMessage *timer = nullptr;
    opp_component_ptr<cModule> interfaceNotifications;
    int maxDatagrams, maxBytes, retainedBytes = 0;
    simtime_t gap, residence, protection;
    void schedule();
    void sendNext(int64_t token);
    void retire(int64_t token, bool successful, LowpanDropReason reason = LOWPAN_SOURCE_ABORTED);
    virtual void receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details) override;

  public:
    explicit LowpanSourceFragmentation(LowpanLayer& owner);
    ~LowpanSourceFragmentation();
    bool isTimer(const cMessage *message) const { return message == timer; }
    void handleTimer();
    void admit(Packet *packet, const Ptr<const Chunk>& header, int headerLength,
            const std::vector<LowpanFragmenter::Range>& ranges,
            const Ieee802154AddressReq& peer, int payloadLimit);
    void completed(int64_t frameId, bool successful);
    void clear();
    int getNumDatagrams() const { return flows.size(); }
    int getRetainedBytes() const { return retainedBytes; }
    simtime_t getResidence() const { return residence; }
    simtime_t getTagProtection() const { return protection; }
};
} }
#endif
