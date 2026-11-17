// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154PHYSERVICE_H
#define __INET_IEEE802154PHYSERVICE_H

#include <optional>
#include <map>
#include "inet/common/ModuleRefByPar.h"
#include "inet/common/SimpleModule.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154StandardRadio.h"
#include "inet/physicallayer/wireless/ieee802154/Ieee802154EnergyObservation.h"
#include "inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceClient.h"
#include "inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceProvider.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee802154PhyService : public SimpleModule, public IIeee802154PhyServiceProvider, public ILifecycle, public cListener
{
  private:
    enum class Kind { TRANSMIT, CCA, ED, STATE };
    struct Pending {
        Kind kind;
        uint64_t id;
        Packet *packet = nullptr;
        Ieee802154PhyState target = Ieee802154PhyState::OFF;
        simtime_t start;
        int transmissionId = -1;
        Ieee802154PhyFrameTiming timing;
        bool receivedPpdu = false;
    };
    ModuleRefByPar<Ieee802154StandardRadio> radio;
    ModuleRefByPar<IIeee802154PhyServiceClient> client;
    std::optional<Pending> pending;
    std::optional<uint64_t> resetId;
    bool resetDefaults = false;
    uint64_t lastId = 0;
    cMessage *operationTimer = nullptr;
    bool started = true;
    Ieee802154PhyState state = Ieee802154PhyState::OFF;
    Ieee802154PhyChannel channel{Ieee802154PhyChannel::Phy::O_QPSK, Ieee802154PhyChannel::Band::MHZ_2450, 20};
    W threshold;
    simtime_t ccaDuration;
    int64_t txPower = 0, broadcastPower = 0, unicastPower = 0;
    std::vector<Ieee802154PhyPeerPower> peerPowers;
    struct ReceivedQuality { W power; int transmissionId; };
    std::map<int64_t, ReceivedQuality> receivedPowers; ///< Copied reception data, consumed by packet ID at gate delivery.
    Ieee802154EnergyObservation observation;
    virtual int numInitStages() const override;
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual void receiveSignal(cComponent *source, simsignal_t id, cObject *object, cObject *details) override;
    Ieee802154PhyServiceAdmission admit(uint64_t id, bool reset = false);
    Ieee802154PhyServiceAdmission measure(uint64_t id, Kind kind);
    void abort(Ieee802154PhyServiceAbortReason reason);
    std::optional<Pending> retirePending();
    void reportAbort(const std::optional<Pending>& operation, Ieee802154PhyServiceAbortReason reason);
    void stop();
    void setChannel(const Ieee802154PhyChannel& value);
    Ieee802154PhyServiceStatus setAttribute(const Ieee802154PhySetRequest& request);
    std::optional<Ieee802154PhyPibValue> getAttribute(const std::string& name) const;
    static uint8_t powerLevel(W power, double lowDbm, double highDbm);

  public:
    virtual ~Ieee802154PhyService();
    virtual Ieee802154PhyTimingParameters getTimingParameters() const override;
    virtual Ieee802154PhyServiceAdmission submitTransmit(uint64_t id, Packet *packet, const Ieee802154PhyTransmitRequest& request) override;
    virtual Ieee802154PhyServiceAdmission submitCca(uint64_t id) override;
    virtual Ieee802154PhyServiceAdmission submitEd(uint64_t id) override;
    virtual Ieee802154PhyServiceAdmission submitState(uint64_t id, Ieee802154PhyState target) override;
    virtual Ieee802154PhyServiceAdmission submitGet(uint64_t id, const Ieee802154PhyGetRequest& request) override;
    virtual Ieee802154PhyServiceAdmission submitSet(uint64_t id, const Ieee802154PhySetRequest& request) override;
    virtual Ieee802154PhyServiceAdmission submitReset(uint64_t id, bool defaults) override;
    virtual Ieee802154PhyServiceCancelResult cancel(uint64_t id) override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *callback) override;
};

} // namespace physicallayer
} // namespace inet
#endif
