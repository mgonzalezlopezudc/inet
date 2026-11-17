// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154STANDARDMAC_H
#define __INET_IEEE802154STANDARDMAC_H

#include <memory>
#include <optional>
#include "inet/common/ModuleRefByPar.h"
#include "inet/common/SimpleModule.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/linklayer/ieee802154/Ieee802154MacPib.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceProvider.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessProvider.h"
#include "inet/linklayer/ieee802154/Ieee802154Frame_m.h"
#include "inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceClient.h"
#include "inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceProvider.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {

class INET_API Ieee802154StandardMac : public SimpleModule, public IIeee802154MacServiceProvider, public ILifecycle,
        public IIeee802154AccessClient, public physicallayer::IIeee802154PhyServiceClient
{
  private:
    ModuleRefByPar<IIeee802154MacServiceClient> client;
    ModuleRefByPar<NetworkInterface> networkInterface;
    std::unique_ptr<Ieee802154MacPib> pib;
    uint64_t lastRequestId = 0;
    bool started = true;
    bool phyAvailable = true;
    bool initialPanCoordinator = false;
    bool panCoordinator = false;
    enum class Phase { WAIT_IFS, WAIT_RX, ACCESS, WAIT_TX, TRANSMIT, WAIT_ACK };
    enum class PhyPurpose { IDLE, RX_FOR_ACCESS, CCA, TX_READY, TRANSMIT, ACK_LISTEN, ACK_READY, ACK_TRANSMIT, MANAGEMENT_GET, MANAGEMENT_SET, RESET };
    struct Transaction {
        uint64_t id;
        Packet *msdu;
        uint8_t handle;
        Ptr<Ieee802154Frame> frame;
        Phase phase = Phase::WAIT_RX;
        uint64_t accessId = 0;
        uint8_t backoffs = 0;
        uint8_t retries = 0;
        simtime_t transmittedEnd;
        simtime_t ackDeadline;
        std::optional<bool> ackFramePending;
        std::optional<uint8_t> ackRssi;
    };
    struct AckExchange {
        Ptr<Ieee802154Frame> frame;
        Packet *msdu = nullptr;
        Ieee802154MacDataIndication indication;
        std::optional<Ieee802154MacStatus> securityError;
        simtime_t target;
        b receivedLength;
    };
    struct PhyOperation { uint64_t id; PhyPurpose purpose; };
    struct ResetRequest {
        uint64_t id;
        bool defaults;
        bool retiring = true;
        std::optional<Ieee802154MacServiceResult> deferredResult;
    };
    struct ManagementRequest {
        uint64_t id;
        std::string attribute;
        std::optional<physicallayer::Ieee802154PhyPibValue> value;
    };
    std::optional<Transaction> transaction;
    std::optional<PhyOperation> phyOperation;
    std::optional<ResetRequest> resetRequest;
    std::optional<ManagementRequest> managementRequest;
    std::optional<AckExchange> ackExchange;
    ModuleRefByPar<IIeee802154AccessProvider> access;
    ModuleRefByPar<physicallayer::IIeee802154PhyServiceProvider> phy;
    physicallayer::Ieee802154PhyState phyState = physicallayer::Ieee802154PhyState::OFF;
    uint64_t phyId = 0, accessId = 0;
    simtime_t eligibleAt;
    cMessage *dataTimer = nullptr;
    cMessage *ackTimer = nullptr;
    bool ccaDeferred = false;
    bool retiringRequests = false;
    void processReceivedPacket(Packet *packet, const physicallayer::Ieee802154PhyReceiveIndication& quality);
    void beginResetPhy();
    void retireDataRadio();
    virtual void handleMessage(cMessage *message) override;
    virtual int numInitStages() const override;
    virtual void initialize(int stage) override;
    Ieee802154MacServiceAdmission admit(uint64_t id, bool concurrent = false, bool duringManagement = false);
    Ieee802154MacServiceAdmission admitRadio(uint64_t id, bool concurrent = false, bool duringManagement = false);
    void publishIdentity();
    uint64_t getInteger(const char *name) const;
    bool getBoolean(const char *name) const;
    void startData();
    void startAccess();
    void requestState(physicallayer::Ieee802154PhyState state, PhyPurpose purpose);
    void restoreIdle();
    void completeData(const Ieee802154MacServiceResult& result);
    void finishReset(const Ieee802154MacServiceResult& result);
    void stop();
    void continueData();
    void performCca();
    void ackTimeout();
    void startAck();
    void finishAck(bool sent);
    void discardAck();
    void deliverReceived(AckExchange& exchange, bool ackSent);
    simtime_t interframeSpacing(b frameLength) const;
    static uint8_t receivedRssi(const physicallayer::Ieee802154PhyReceiveIndication& indication);
    uint64_t allocatePhyId();
    void completeManagement(const Ieee802154MacServiceResult& result, const std::optional<Ieee802154MacPibValue>& value = {});

  public:
    virtual ~Ieee802154StandardMac();
    virtual Ieee802154MacServiceAdmission submitData(uint64_t id, Packet *packet, const Ieee802154MacDataRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitGet(uint64_t id, const Ieee802154MacGetRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitSet(uint64_t id, const Ieee802154MacSetRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitReset(uint64_t id, const Ieee802154MacResetRequest& request) override;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id) override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
    virtual void handleCcaRequest(uint64_t id) override;
    virtual void handleAccessConfirm(uint64_t id, const Ieee802154MacServiceResult& result, uint8_t numBackoffs) override;
    virtual void handleTransmitStarted(uint64_t id, const physicallayer::Ieee802154PhyTransmitStart& start) override;
    virtual void handleAvailabilityIndication(const physicallayer::Ieee802154PhyAvailabilityIndication& indication) override;
    virtual void handleTransmitConfirm(uint64_t id, const physicallayer::Ieee802154PhyTransmitConfirm& confirm) override;
    virtual void handleCcaConfirm(uint64_t id, const physicallayer::Ieee802154PhyCcaConfirm& confirm) override;
    virtual void handleEdConfirm(uint64_t id, const physicallayer::Ieee802154PhyEdConfirm& confirm) override;
    virtual void handleStateConfirm(uint64_t id, const physicallayer::Ieee802154PhyStateConfirm& confirm) override;
    virtual void handleGetConfirm(uint64_t id, const physicallayer::Ieee802154PhyGetConfirm& confirm) override;
    virtual void handleSetConfirm(uint64_t id, const physicallayer::Ieee802154PhySetConfirm& confirm) override;
    virtual void handleResetConfirm(uint64_t id, const physicallayer::Ieee802154PhyResetConfirm& confirm) override;
    virtual void handleReceiveIndication(Packet *packet, const physicallayer::Ieee802154PhyReceiveIndication& indication) override;
};

} // namespace inet
#endif
