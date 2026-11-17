// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154STANDARDMAC_H
#define __INET_IEEE802154STANDARDMAC_H

#include <memory>
#include <optional>
#include <deque>
#include <map>
#include "inet/common/ModuleRefByPar.h"
#include "inet/common/SimpleModule.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/linklayer/ieee802154/Ieee802154MacPib.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154MacServiceProvider.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessProvider.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154SuperframeClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154SuperframeProvider.h"
#include "inet/linklayer/ieee802154/Ieee802154Frame_m.h"
#include "inet/linklayer/ieee802154/Ieee802154PendingTransactions.h"
#include "inet/linklayer/ieee802154/Ieee802154CommandCodec.h"
#include "inet/linklayer/ieee802154/Ieee802154GtsManager.h"
#include "inet/linklayer/ieee802154/Ieee802154MacObservations.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154ManagementServiceClient.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154ManagementServiceProvider.h"
#include "inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceClient.h"
#include "inet/physicallayer/wireless/ieee802154/contract/IIeee802154PhyServiceProvider.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {

class INET_API Ieee802154StandardMac : public SimpleModule, public IIeee802154MacServiceProvider, public ILifecycle,
        public IIeee802154AccessClient, public physicallayer::IIeee802154PhyServiceClient, public IIeee802154ManagementServiceProvider,
        public IIeee802154SuperframeClient
{
  private:
    ModuleRefByPar<IIeee802154MacServiceClient> client;
    ModuleRefByPar<IIeee802154ManagementServiceClient> managementClient;
    ModuleRefByPar<NetworkInterface> networkInterface;
    std::unique_ptr<Ieee802154MacPib> pib;
    Ieee802154GtsManager gtsManager;
    Ieee802154EnhancedAckContent enhancedAckResponse;
    uint64_t lastRequestId = 0;
    bool started = true;
    bool phyAvailable = true;
    bool initialPanCoordinator = false;
    bool panCoordinator = false;
    bool coordinator = false; // Coordinator services remain available without an outgoing superframe.
    bool parentAssociated = false; // Committed association, independent of the parent's PAN-coordinator role.
    simtime_t transactionPersistence() const;
    enum class Phase { WAIT_IFS, WAIT_RX, WAIT_CAP, WAIT_GTS, ACCESS, WAIT_TX, TRANSMIT, WAIT_ACK };
    enum class PhyPurpose { IDLE, RX_FOR_ACCESS, CCA, TX_READY, TRANSMIT, ACK_LISTEN, ACK_READY, ACK_TRANSMIT, MANAGEMENT_GET, MANAGEMENT_SET, START_CHANNEL, BEACON_CHANNEL, ASSOCIATE_CHANNEL, ORPHAN_CHANNEL_GET, SCAN_GET, SCAN_CCA_GET, SCAN_OFF, SCAN_CHANNEL, SCAN_RX, SCAN_ED_DURATION, SCAN_ED, SCAN_RESTORE_OFF, SCAN_CCA_RESTORE, SCAN_PHY_CCA_RESTORE, SCAN_RESTORE, SCAN_REALIGN_OFF, SCAN_REALIGN_CHANNEL, RESET, SUPERFRAME_GET, SUPERFRAME_CHANNEL, SUPERFRAME_RESTORE, SUPERFRAME_PHY_RESTORE, SUPERFRAME_READY, SUPERFRAME_ACTIVATION_READY, SUPERFRAME_RETUNE_OFF, SUPERFRAME_TRANSMIT, SYNC_CHANNEL, SYNC_READY };
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
        std::optional<Ieee802154EnhancedAckContent> enhancedAckContent;
        std::optional<simtime_t> accessStartAt;
        enum class Search { NONE, SEARCHING, FALLBACK } search = Search::NONE;
        bool slotted = false;
        bool accessPending = false;
        bool transmittedInCap = false;
        std::optional<Ieee802154GtsManager::Key> gtsKey;
        std::optional<simtime_t> gtsEnd;
        uint8_t capBeaconOrder = 15;
        std::optional<std::pair<uint16_t,Ieee802154Address>> pendingDevice; ///< Queue key retained across allocation changes.
        enum class Kind { DATA, POLL, INDIRECT, BEACON, AUTO_BEACON, SCAN_REQUEST, ASSOCIATE_REQUEST, DISASSOCIATE, ORPHAN_RESPONSE, PAN_CONFLICT, SUPERFRAME_REALIGN, GTS_REQUEST } kind = Kind::DATA;
    };
    struct AckExchange {
        Ptr<Ieee802154Frame> frame;
        Packet *msdu = nullptr;
        Ieee802154MacDataIndication indication;
        std::optional<Ieee802154MacStatus> securityError;
        simtime_t target;
        b receivedLength;
        std::optional<std::pair<uint16_t, Ieee802154Address>> polledDevice;
        std::optional<uint64_t> receivedPollResponse;
        bool pollDataPresent = false;
        std::optional<uint8_t> associationCapability;
        std::optional<Ieee802154CommandCodec::AssociationResponse> associationResponse;
        std::optional<uint8_t> disassociationReason;
        std::optional<Ieee802154CommandCodec::CoordinatorRealignment> realignment;
        std::optional<physicallayer::Ieee802154PhyChannel> panConflictChannel;
        std::optional<std::pair<Ieee802154GtsManager::Key,Ieee802154GtsCharacteristics>> gtsCommand;
    };
    struct PhyOperation { uint64_t id; PhyPurpose purpose; bool transmitStarted = false; std::optional<simtime_t> expectedTransmitEnd; };
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
    struct StartRequest { uint64_t id; Ieee802154MacStartRequest parameters; };
    std::optional<StartRequest> startRequest;
    struct SuperframeStartRequest {
        uint64_t id;
        Ieee802154MacStartSuperframeRequest parameters;
        std::optional<physicallayer::Ieee802154PhyChannel> originalChannel;
        std::optional<Ieee802154MacServiceResult> failure;
        bool awaitingFirstBoundary = false;
        bool preparingFirstBeacon = true;
        std::optional<clocktime_t> firstTarget;
        bool awaitingShutdownBoundary = false;
        enum class Phase { INITIAL, WAIT_OLD_BEACON, COMMAND, WAIT_ACTIVATION, IMMEDIATE_ACTIVATION } phase = Phase::INITIAL;
    };
    std::optional<SuperframeStartRequest> superframeStartRequest;
    std::optional<physicallayer::Ieee802154PhyChannel> interruptedSuperframeChannel;
    std::optional<simtime_t> interruptedScanCcaDuration;
    bool periodicBeaconing = false;
    std::optional<clocktime_t> periodicTarget;
    std::optional<clocktime_t> beaconAfterAck;
    struct BeaconConfiguration {
        uint8_t beaconOrder;
        uint8_t superframeOrder;
        bool batteryLifeExtension;
        uint32_t batteryLifePeriods;
        uint32_t backoffPeriod;
        uint8_t finalCapSlot = 15;
    };
    std::optional<BeaconConfiguration> constructedBeacon;
    std::vector<Ieee802154GtsManager::Advertisement> constructedGtsAdvertisements;
    std::vector<Ieee802154GtsManager::Grant> constructedCfpGrants;
    struct GtsCandidate { Ieee802154GtsManager ledger; std::vector<Ieee802154GtsManager::Grant> removed; bool agesImmediateEpoch = false; };
    std::optional<GtsCandidate> constructedGtsCandidate;
    std::optional<clocktime_t> lastGtsAgedBoundary;
    void ageGtsEpoch(clocktime_t boundary);
    std::optional<BeaconConfiguration> outgoingEpoch;
    // The last located parent superframe outlives both its CAP and a one-shot SYNC.
    struct IncomingSuperframe {
        clocktime_t anchor;
        uint8_t beaconOrder;
        uint8_t superframeOrder;
        simtime_t physicalStart;
        simtime_t physicalEnd;
        bool panCoordinator;
        uint16_t panId;
        Ieee802154Address coordinator;
        physicallayer::Ieee802154PhyChannel channel;
    };
    std::optional<IncomingSuperframe> incomingSuperframe;
    struct CfpWindow {
        Ieee802154GtsManager::Key key;
        simtime_t physicalStart;
        simtime_t physicalEnd;
        bool receive;
        bool outgoing;
        bool activitySettled = false;
        uint8_t beaconOrder;
        int64_t epoch;
        std::optional<int64_t> rephaseAfter;
    };
    std::vector<CfpWindow> cfpWindows;
    bool cfpReceive = false;
    void installCfpWindows(clocktime_t anchor, uint8_t superframeOrder, bool outgoing, const std::vector<Ieee802154GtsManager::Grant>& grants);
    void refreshCfp();
    void clearCfp();
    void retireInvalidGtsData();
    void expireGtsGrant(const Ieee802154GtsManager::Key& key);
    bool settleSkippedGtsOpportunities();
    void emitGtsObservation(simsignal_t signal, const Ieee802154GtsManager::Grant& grant);
    struct GtsRequestState { uint64_t id; Ieee802154MacGtsRequest request; bool waiting = false; };
    std::optional<GtsRequestState> gtsRequest;
    cMessage *gtsTimer = nullptr;
    void completeGts(const Ieee802154MacServiceResult& result);
    void decideGts(const Ieee802154GtsManager::Key& key, const Ieee802154GtsCharacteristics& characteristics);
    void removeGtsGrant(const Ieee802154GtsManager::Key& key, bool advertiseRemoval = true, bool notify = true);
    static void removeGtsFromLedger(Ieee802154GtsManager& ledger, const Ieee802154GtsManager::Key& key, bool advertiseRemoval = true);
    void maintainGtsCapacity(uint8_t superframeOrder);
    simtime_t gtsPhysicalCapDuration(uint8_t superframeOrder, unsigned int slots, bool immediate = false, std::optional<clocktime_t> target = std::nullopt) const;
    std::optional<clocktime_t> childPhase; // Recurring local-clock phase from the parent beacon.
    std::optional<clocktime_t> transmittedBeaconAnchor;
    simtime_t transmittedBeaconIfs;
    bool transmittedBeaconBroadcastPending = false;
    bool outgoingCapActive = false;
    std::optional<clocktime_t> outgoingCapEnd;
    std::optional<clocktime_t> incomingCapEnd;
    simsignal_t periodicBeaconStartSignal = SIMSIGNAL_NULL;
    simsignal_t capWindowInstalledSignal = SIMSIGNAL_NULL;
    simsignal_t cfpWindowInstalledSignal = SIMSIGNAL_NULL;
    simsignal_t gtsAllocatedSignal = SIMSIGNAL_NULL;
    simsignal_t gtsDeallocatedSignal = SIMSIGNAL_NULL;
    simsignal_t gtsActivitySignal = SIMSIGNAL_NULL;
    simsignal_t outgoingCapActiveSignal = SIMSIGNAL_NULL;
    void setOutgoingCapActive(bool active);
    bool scheduleCapDutyEnd();
    void completeSuperframeStart(const Ieee802154MacServiceResult& result);
    bool commitSuperframeChannel();
    void retireParentContext(bool clearAssociation);
    void failSuperframeStart(const Ieee802154MacServiceResult& result);
    void preparePeriodicBeacon();
    std::optional<clocktime_t> nextBeaconBoundary() const;
    bool ownsBeaconBoundary(clocktime_t boundary) const;
    clocktime_t firstChildBeaconTarget(const Ieee802154MacStartSuperframeRequest& request) const;
    clocktime_t advanceChildBeaconTarget(const Ieee802154MacStartSuperframeRequest& request, clocktime_t target) const;
    void transmitSuperframeRealignment();
    void sendPeriodicBeacon();
    void scheduleNextPeriodicBeacon();
    void skipPeriodicBeacon();
    void retirePeriodicBeaconing(const Ieee802154MacServiceResult& result);
    void resumeAvailablePhy();
    void startOutgoingCap();
    void publishAccessWindow(clocktime_t anchor, simtime_t beaconEnd, simtime_t beaconIfs,
            uint8_t beaconOrder, uint8_t superframeOrder, uint8_t finalCapSlot, bool batteryLifeExtension,
            uint32_t backoffPeriod, uint32_t batteryLifePeriods, Ieee802154AccessWindowOrigin origin);
    void resumeCapAccess();
    Ptr<Ieee802154Frame> makeBeacon(uint16_t panId, uint8_t beaconOrder, uint8_t superframeOrder, bool batteryLifeExtension, bool consumeSequence = true, const Ieee802154GtsManager *ledger = nullptr);
    bool overlapsIncomingSuperframe(clocktime_t target, uint8_t beaconOrder, uint8_t superframeOrder, b psduLength) const;
    bool fitsBeaconPayload(const std::vector<uint8_t>& payload) const;
    struct Synchronization {
        uint64_t id;
        Ieee802154MacSyncRequest request;
        bool installed = false;
        bool wantsRx = false;
        bool anchored = false;
        uint8_t misses = 0;
        uint8_t beaconOrder = 15;
        clocktime_t anchor;
        clocktime_t windowStart;
        clocktime_t deadline;
        simtime_t physicalDeadline;
    };
    std::optional<Synchronization> synchronization;
    bool beaconScheduleActive() const { return periodicBeaconing || (synchronization && synchronization->installed) || (superframe && getInteger("macBeaconOrder") < 15); }
    simsignal_t syncMissedSignal = SIMSIGNAL_NULL;
    simsignal_t syncLostSignal = SIMSIGNAL_NULL;
    void retireSynchronization(const Ieee802154MacServiceResult& pendingResult);
    void completeSyncInstallation(const Ieee802154MacServiceResult& result);
    void scheduleAcquisitionWindow();
    void scheduleTrackingWindow();
    bool matchesSyncCoordinator(const Ieee802154PanDescriptor& descriptor) const;
    bool renewSynchronization(const Ieee802154PanDescriptor& descriptor, const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor, simtime_t physicalEnd);
    struct ScanRequest {
        uint64_t id;
        Ieee802154MacScanRequest parameters;
        Ieee802154MacScanConfirm confirm;
        std::optional<physicallayer::Ieee802154PhyChannel> originalChannel;
        size_t index = 0;
        bool autoRequest;
        bool closing = false;
        simtime_t listeningSince;
        std::optional<simtime_t> originalCcaDuration;
        simtime_t deadline;
        uint8_t energyPeak = 0;
        bool requestSent = false;
        bool beaconFound = false;
        std::optional<std::pair<Ieee802154CommandCodec::CoordinatorRealignment,Ieee802154Address>> realignment;
    };
    std::optional<ScanRequest> scanRequest;
    cMessage *scanTimer = nullptr;
    void beginScanChannel();
    void setScanChannel(bool restore);
    void measureScanEnergy();
    void finishScan(const Ieee802154MacServiceResult& result);
    void completeScan();
    void receiveScanBeacon(const Ptr<Ieee802154Frame>& frame,
            const physicallayer::Ieee802154PhyReceiveIndication& quality,
            const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor);
    void beginResetPhy();
    std::optional<uint64_t> beaconPreparation;
    uint64_t beaconResponses = 0;
    void completeBeaconPreparation(const Ieee802154MacServiceResult& result);
    void transmitBeacon(uint64_t id, bool automatic);
    void transmitScanRequest();
    struct AssociationRequest {
        uint64_t id;
        Ieee802154MacAssociateRequest parameters;
        std::optional<Ieee802154CommandCodec::AssociationResponse> response;
        Ieee802154Address coordinatorExtendedAddress;
        bool waitingBeacon = false;
    };
    std::optional<AssociationRequest> associationRequest;
    cMessage *associationTimer = nullptr;
    void transmitAssociationRequest();
    void completeAssociate(const Ieee802154MacServiceResult& result);
    void startPoll();
    struct OrphanResponseRequest { uint64_t id; Ieee802154MacOrphanResponse parameters; };
    std::optional<OrphanResponseRequest> orphanResponseRequest;
    void completeOrphanResponse(const Ieee802154MacServiceResult& result);
    void transmitOrphanResponse(const physicallayer::Ieee802154PhyChannel& channel);
    void commitOrphanRealignment(const Ieee802154CommandCodec::CoordinatorRealignment& realignment,
            const Ieee802154Address& coordinator);
    struct PanConflict {
        uint16_t panId;
        Ieee802154Address coordinator;
        physicallayer::Ieee802154PhyChannel channel;
    };
    std::optional<PanConflict> pendingPanConflict;
    bool broadcastReceivePending = false;
    bool isAssociatedCoordinator(uint16_t panId, const Ieee802154Address& address) const;
    bool hasEnhancedPeerContext(uint16_t panId, const Ieee802154Address& address) const;
    struct ReceiverWindow {
        uint64_t id;
        std::optional<simtime_t> startsAt;
        simtime_t prepareAt;
        uint32_t duration;
        bool autoOff;
        std::optional<simtime_t> endsAt;
        std::optional<uint32_t> endTimestamp;
    };
    std::optional<ReceiverWindow> receiverWindow;
    std::optional<uint64_t> rxEnableRequest;
    cMessage *receiverPrepareTimer = nullptr;
    cMessage *receiverEndTimer = nullptr;
    bool receiverWantsRx() const;
    void clearReceiverWindow();
    void finishReceiverWindow();
    bool consumeReceiverWindow();
    void processReceivedPacket(Packet *packet, const physicallayer::Ieee802154PhyReceiveIndication& quality,
            const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor, uint64_t receiveEpoch);
    void transmitPanConflict();
    bool receiveManagedBeacon(const Ptr<Ieee802154Frame>& frame,
            const physicallayer::Ieee802154PhyReceiveIndication& quality,
            const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor, uint64_t receiveEpoch);
    std::optional<Ieee802154PanDescriptor> describeBeacon(const Ptr<Ieee802154Frame>& frame,
            const physicallayer::Ieee802154PhyReceiveIndication& quality,
            const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor) const;
    std::optional<AckExchange> ackExchange;
    ModuleRefByPar<IIeee802154AccessProvider> access;
    ModuleRefByPar<IIeee802154AccessProvider> slottedAccess;
    ModuleRefByPar<physicallayer::IIeee802154PhyServiceProvider> phy;
    ModuleRefByPar<IIeee802154SuperframeProvider> superframe;
    // A synchronous callback may retire and restart the receiver while an old beacon is being processed.
    uint64_t receiveRetirementEpoch = 0;
    void retireReceives();
    physicallayer::Ieee802154PhyState phyState = physicallayer::Ieee802154PhyState::OFF;
    uint64_t phyId = 0, accessId = 0;
    simtime_t eligibleAt;
    cMessage *dataTimer = nullptr;
    cMessage *ackTimer = nullptr;
    bool ccaDeferred = false;
    bool slottedCcaNeedsAlignment = false;
    struct Disassociation {
        Ieee802154MacDisassociateRequest parameters;
        Ieee802154Address canonicalPeer;
        bool leavingCoordinator;
    };
    struct PendingOriginal {
        Packet *msdu;
        uint8_t handle;
        std::variant<std::monostate,Ieee802154Address,Disassociation> command;
    };
    std::optional<std::pair<uint64_t,Disassociation>> disassociationRequest;
    bool retiringRequests = false;
    void completeDisassociate(uint64_t id, const Disassociation& request, const Ieee802154MacServiceResult& result);
    void cleanupAssociation(uint16_t panId, const std::optional<Ieee802154Address>& peer,
            const std::optional<uint64_t>& receivedPoll = {});
    void reportPendingResult(uint64_t token, const PendingOriginal& original, const Ieee802154MacServiceResult& result,
            std::optional<uint8_t> backoffs = {}, std::optional<bool> ackFramePending = {}, std::optional<uint8_t> ackRssi = {},
            std::optional<Ieee802154EnhancedAckContent> enhancedAckContent = {});
    struct PollRequest {
        uint64_t id;
        Ieee802154MacPollRequest request;
        bool waiting = false;
        simtime_t deadline;
        bool association = false;
        bool automatic = false;
        std::optional<Ieee802154Address> advertisedSource;
    };
    struct PendingPoll { uint16_t panId; Ieee802154Address device; simtime_t receivedAt; };
    std::unique_ptr<Ieee802154PendingTransactions> pending;
    std::map<uint64_t, PendingOriginal> pendingOriginals;
    std::map<std::pair<uint16_t,Ieee802154Address>,uint16_t> associatedDevices;
    Ieee802154Address canonicalAddress(uint16_t panId, const Ieee802154Address& address) const;
    std::deque<PendingPoll> pendingPolls;
    std::optional<PollRequest> pollRequest;
    std::optional<Ieee802154Address> autoPollPending;
    bool pendingCallbacksActive = false;
    bool syncLossNotificationsActive = false;
    cMessage *pendingTimer = nullptr;
    cMessage *pollTimer = nullptr;
    void completePending(uint64_t token, const Ieee802154MacServiceResult& result,
            std::optional<uint8_t> backoffs = {}, std::optional<bool> ackFramePending = {}, std::optional<uint8_t> ackRssi = {},
            std::optional<Ieee802154EnhancedAckContent> enhancedAckContent = {});
    void expirePending();
    void schedulePendingExpiry();
    void clearPending(const Ieee802154MacServiceResult& result);
    void startPendingResponse();
    bool startQuickPendingResponse(simtime_t ackEnd);
    void completePoll(const Ieee802154MacServiceResult& result);
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
    bool matchesAck(const Ieee802154Frame& frame, const physicallayer::Ieee802154PhyReceiveIndication& quality) const;
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
    void completeStart(const Ieee802154MacServiceResult& result);

  public:
    virtual ~Ieee802154StandardMac();
    virtual Ieee802154MacServiceAdmission submitData(uint64_t id, Packet *packet, const Ieee802154MacDataRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitGet(uint64_t id, const Ieee802154MacGetRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitSet(uint64_t id, const Ieee802154MacSetRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitReset(uint64_t id, const Ieee802154MacResetRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitPoll(uint64_t id, const Ieee802154MacPollRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitPurge(uint64_t id, uint8_t msduHandle) override;
    virtual Ieee802154MacServiceAdmission submitStart(uint64_t id, const Ieee802154MacStartRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitStartSuperframe(uint64_t id, const Ieee802154MacStartSuperframeRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitSync(uint64_t id, const Ieee802154MacSyncRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitScan(uint64_t id, const Ieee802154MacScanRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitBeacon(uint64_t id, const Ieee802154MacBeaconRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitAssociateResponse(uint64_t id, const Ieee802154MacAssociateResponse& response) override;
    virtual Ieee802154MacServiceAdmission submitAssociate(uint64_t id, const Ieee802154MacAssociateRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitDisassociate(uint64_t id, const Ieee802154MacDisassociateRequest& request) override;
    virtual Ieee802154MacServiceAdmission submitOrphanResponse(uint64_t id, const Ieee802154MacOrphanResponse& response) override;
    virtual Ieee802154MacServiceAdmission submitRxEnable(uint64_t id, const Ieee802154MacRxEnableRequest& request) override;
    Ieee802154MacServiceAdmission submitGts(uint64_t id, const Ieee802154MacGtsRequest& request) override;
    virtual Ieee802154MacServiceCancelResult cancel(uint64_t id) override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
    virtual void handleCcaRequest(uint64_t id) override;
    virtual void handleAccessConfirm(uint64_t id, const Ieee802154MacServiceResult& result, uint8_t numBackoffs, std::optional<simtime_t> startAt) override;
    virtual void handleTransmitStarted(uint64_t id, const physicallayer::Ieee802154PhyTransmitStart& start) override;
    virtual void handleReceiveStarted(const physicallayer::Ieee802154PhyReceiveStart& start) override;
    virtual void handleSuperframeBoundary(clocktime_t boundary) override;
    virtual void handleSuperframeCapEnd() override;
    void handleCfpBoundary() override;
    virtual void handleSyncWake() override;
    virtual void handleSyncDeadline() override;
    virtual void handleAccessSearchDeadline() override;
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
