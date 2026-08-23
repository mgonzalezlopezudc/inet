//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MGMTBASE_H
#define __INET_IEEE80211MGMTBASE_H

#include <map>

#include "inet/common/ModuleRefByPar.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/OperationalBase.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mgmt/contract/IIeee80211PeerCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {

namespace ieee80211 {

/**
 * Abstract base class for 802.11 infrastructure mode management components.
 *
 */
class INET_API Ieee80211MgmtBase : public OperationalBase, public cListener, public IIeee80211PeerCapabilities
{
  protected:
    // configuration
    ModuleRefByPar<Ieee80211Mib> mib;
    ModuleRefByPar<IInterfaceTable> interfaceTable;
    NetworkInterface *myIface = nullptr;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    Ieee80211SupportedRatesElement supportedRates;
    bool htLdpcRxSupported = false;
    bool vhtLdpcRxSupported = false;
    int maximumSpatialStreams = 1;
    std::map<MacAddress, uint8_t> latestPeerOperatingModes;
    std::map<MacAddress, uint8_t> latestPeerType0OperatingModes;

    // statistics
    long numMgmtFramesReceived;
    long numMgmtFramesDropped;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

    /** Dispatches incoming messages to handleTimer(), handleUpperMessage() or processFrame(). */
    virtual void handleMessageWhenUp(cMessage *msg) override;

    /** Should be redefined to deal with self-messages */
    virtual void handleTimer(cMessage *frame) = 0;

    /** Should be redefined to handle commands from the "agent" (if present) */
    virtual void handleCommand(int msgkind, cObject *ctrl) = 0;

    /** Utility method for implementing handleUpperMessage(): send message to MAC */
    virtual void sendDown(Packet *frame);

    /** Send an explicitly requested VHT Operating Mode Notification action. */
    virtual void sendOperatingModeNotification(const MacAddress& receiverAddress, uint8_t operatingMode);

    /** Utility method to dispose of an unhandled frame */
    virtual void dropManagementFrame(Packet *frame);

    void setLocalLdpcCapabilities(const Ptr<Ieee80211MgmtFrame>& frame) const;
    Ieee80211PeerLdpcStatus mergePeerLdpcCapabilities(const MacAddress& peer,
            const Ieee80211PeerLdpcStatus& previous, const Ieee80211MgmtFrame& frame);
    Ieee80211PeerLdpcStatus applyLatestPeerOperatingMode(const MacAddress& peer,
            const Ieee80211PeerLdpcStatus& status) const;
    void updateLatestPeerOperatingMode(const MacAddress& peer, uint8_t operatingMode);
    void clearPeerOperatingMode(const MacAddress& peer) { latestPeerOperatingModes.erase(peer); latestPeerType0OperatingModes.erase(peer); }
    void clearPeerOperatingModes() { latestPeerOperatingModes.clear(); latestPeerType0OperatingModes.clear(); }

    /** Dispatch to frame processing methods according to frame type */
    virtual void processFrameFromMac(Packet *packet);
    virtual void processFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header);

    /** @name Processing of different frame types */
    //@{
    virtual void handleAuthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleDeauthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleAssociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleAssociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleReassociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleReassociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleDisassociationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleBeaconFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleProbeRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleProbeResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) = 0;
    virtual void handleOperatingModeNotificationFrame(Packet *packet,
            const Ptr<const Ieee80211OperatingModeNotification>& header);
    //@}

    /** lifecycle support */
    //@{
    virtual bool isInitializeStage(int stage) const override { return stage == INITSTAGE_NETWORK_INTERFACE_CONFIGURATION; } // TODO INITSTAGE
    virtual bool isModuleStartStage(int stage) const override { return stage == ModuleStartOperation::STAGE_PHYSICAL_LAYER; }
    virtual bool isModuleStopStage(int stage) const override { return stage == ModuleStopOperation::STAGE_PHYSICAL_LAYER; }

    virtual void handleStartOperation(LifecycleOperation *operation) override { start(); }
    virtual void handleStopOperation(LifecycleOperation *operation) override { stop(); }
    virtual void handleCrashOperation(LifecycleOperation *operation) override { stop(); }

  protected:
    virtual void start();
    virtual void stop();
    //@}

  public:
    /**
     * Merge capability elements from a newly received frame into stored peer
     * state. Absent elements do not revoke previously learned state.
     */
    static Ieee80211PeerLdpcStatus mergeLdpcCapabilities(
            const Ieee80211PeerLdpcStatus& previous,
            const Ieee80211MgmtFrame& frame);

    virtual Ieee80211PeerLdpcStatus getPeerLdpcStatus(const MacAddress& peer) const override;
    virtual Ieee80211IntendedReceiverSet resolveIntendedReceivers(const MacAddress& receiverAddress) const override;
    virtual Ieee80211VhtSigAParameters getVhtSigAParameters(const MacAddress& receiverAddress) const override;
};

} // namespace ieee80211

} // namespace inet

#endif
