//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MGMTAP_H
#define __INET_IEEE80211MGMTAP_H

#include <cstdint>
#include <map>
#include <optional>

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtApBase.h"
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211MgmtExchangeResult.h"
#include "inet/linklayer/ieee80211/twt/TwtAgreement.h"

namespace inet {

namespace ieee80211 {

class Ieee80211Mac;

/**
 * Used in 802.11 infrastructure mode: handles management frames for
 * an access point (AP). See corresponding NED file for a detailed description.
 */
class INET_API Ieee80211MgmtAp : public Ieee80211MgmtApBase, public IIeee80211MgmtExchangeResultHandler
{
  public:
    /** Describes a STA */
    struct StaInfo {
        MacAddress address;
        int authSeqExpected; // when NOT_AUTHENTICATED: transaction sequence number of next expected auth frame
//        int consecFailedTrans; // TODO
//        double expiry; // TODO association should expire after a while if STA is silent?
    };

    struct PendingAssociation {
        uint64_t transactionId;
        MacAddress peer;
        short associationId;
        bool reassociation;
        Ieee80211SupportedRatesElement supportedRates;
        Ieee80211ExtendedSupportedRatesElement extendedSupportedRates;
        double transmitPowerDbm;
        std::optional<Ieee80211HtCapabilities> htCapabilities;
        std::optional<Ieee80211VhtCapabilities> vhtCapabilities;
        std::optional<Ieee80211HeCapabilities> heCapabilities;
        std::optional<Ieee80211EhtCapabilities> ehtCapabilities;
    };

    class INET_API NotificationInfoSta : public cObject {
        MacAddress apAddress;
        MacAddress staAddress;

      public:
        void setApAddress(const MacAddress& a) { apAddress = a; }
        void setStaAddress(const MacAddress& a) { staAddress = a; }
        const MacAddress& getApAddress() const { return apAddress; }
        const MacAddress& getStaAddress() const { return staAddress; }
    };

    class INET_API MacCompare {
      public:
        bool operator()(const MacAddress& u1, const MacAddress& u2) const { return u1.compareTo(u2) < 0; }
    };
    typedef std::map<MacAddress, StaInfo, MacCompare> StaList;

  protected:
    // configuration
    std::string ssid;
    int channelNumber = -1;
    simtime_t beaconInterval;
    int numAuthSteps = 0;

    // state
    StaList staList; ///< list of STAs
    std::map<MacAddress, PendingAssociation, MacCompare> pendingAssociations;
    uint64_t nextAssociationTransactionId = 1;
    cMessage *beaconTimer = nullptr;
    Ieee80211Mac *mac = nullptr;

  public:
    Ieee80211MgmtAp() {}
    virtual ~Ieee80211MgmtAp();

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int) override;

    /** Implements abstract Ieee80211MgmtBase method */
    virtual void handleTimer(cMessage *msg) override;

    /** Implements abstract Ieee80211MgmtBase method -- throws an error (no commands supported) */
    virtual void handleCommand(int msgkind, cObject *ctrl) override;

    /** Called by the signal handler whenever a change occurs we're interested in */
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

    /** Utility function: return sender STA's entry from our STA list, or nullptr if not in there */
    virtual StaInfo *lookupSenderSTA(const Ptr<const Ieee80211MgmtHeader>& header);

    /** Utility function: set fields in the given frame and send it out to the address */
    virtual void sendManagementFrame(const char *name, const Ptr<Ieee80211MgmtFrame>& body, int subtype, const MacAddress& destAddr);
    virtual void sendManagementFrameWithTransaction(const char *name, const Ptr<Ieee80211MgmtFrame>& body, int subtype, const MacAddress& destAddr, uint64_t transactionId);
    virtual void sendTwtActionFrame(const char *name, const Ptr<Ieee80211ActionFrame>& frame, const MacAddress& destAddr);
    virtual TwtAgreement makeTwtAgreement(const Ptr<const Ieee80211TwtSetupFrame>& frame, const MacAddress& peer) const;

    /** Invalidates MAC-derived state before replacing or removing peer capabilities. */
    virtual void invalidatePeerState(const MacAddress& peer);
    void abortPendingAssociation(const MacAddress& peer);
    void applyPendingAssociation(const PendingAssociation& pending);
    void handleAssociationExchangeResult(const Ieee80211MgmtExchangeResult& result);

    /** Utility function: creates and sends a beacon frame */
    virtual void sendBeacon();

    /** @name Processing of different frame types */
    //@{
    virtual void handleAuthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleDeauthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleAssociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleAssociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleReassociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleReassociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleDisassociationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleBeaconFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleProbeRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleProbeResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header) override;
    virtual void handleActionFrame(Packet *packet, const Ptr<const Ieee80211ActionFrame>& header) override;
    //@}

    void sendAssocNotification(const MacAddress& addr);

    void sendDisAssocNotification(const MacAddress& addr);

    /** lifecycle support */
    //@{

  protected:
    virtual void start() override;
    virtual void stop() override;
    virtual void handleIeee80211MgmtExchangeResult(
            const Ieee80211MgmtExchangeResult& result) override { handleAssociationExchangeResult(result); }
    //@}
};

} // namespace ieee80211

} // namespace inet

#endif
