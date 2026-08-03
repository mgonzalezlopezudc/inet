//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_TXOPPROCEDURE_H
#define __INET_TXOPPROCEDURE_H

#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/common/ModeSetListener.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateSelection.h"

namespace inet {
namespace ieee80211 {

class INET_API TxopProcedure : public ModeSetListener
{
  public:
    static simsignal_t txopStartedSignal;
    static simsignal_t txopEndedSignal;

  public:
    // [...] transmitted under EDCA by a STA that initiates a TXOP, there are
    // two classes of duration settings: single protection and multiple protection.
    enum ProtectionMechanism {
        SINGLE_PROTECTION,
        MULTIPLE_PROTECTION,
        UNDEFINED_PROTECTION
    };

    enum class InitialProtection {
        NONE,
        LEGACY_RTS_CTS,
    };

    class ProtectionState
    {
      protected:
        ProtectionMechanism mechanism = ProtectionMechanism::UNDEFINED_PROTECTION;
        InitialProtection protection = InitialProtection::NONE;
        bool configured = false;
        bool completed = false;

      public:
        void configure(ProtectionMechanism mechanism, InitialProtection protection);
        void complete();
        void reset();
        bool isPending() const { return protection == InitialProtection::LEGACY_RTS_CTS && !completed; }
        bool isConfigured() const { return configured; }
        bool isCompleted() const { return completed; }
        ProtectionMechanism getMechanism() const { return mechanism; }
        bool allowsMpduAggregation() const { return mechanism != ProtectionMechanism::MULTIPLE_PROTECTION; }
    };

  protected:
    simtime_t start = -1;
    simtime_t limit = -1;
    ProtectionState protectionState;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;

    virtual s getTxopLimit(const physicallayer::IIeee80211Mode *mode, AccessCategory ac);
  public:
    virtual void startTxop(AccessCategory ac);
    virtual void configureProtection(InitialProtection protection);
    virtual void completeInitialProtection();
    virtual void endTxop();

    virtual simtime_t getStart() const;
    virtual simtime_t getLimit() const;
    virtual simtime_t getRemaining() const;
    virtual simtime_t getDuration() const;

    virtual bool isFinalFragment(const Ptr<const Ieee80211MacHeader>& header) const;
    virtual bool isTxopInitiator(const Ptr<const Ieee80211MacHeader>& header) const;
    virtual bool isTxopTerminator(const Ptr<const Ieee80211MacHeader>& header) const;

    virtual ProtectionMechanism getProtectionMechanism() const { return protectionState.getMechanism(); }
    virtual bool isInitialProtectionPending() const { return protectionState.isPending(); }
    virtual bool isProtectionConfigured() const { return protectionState.isConfigured(); }
    virtual bool allowsMpduAggregation() const { return protectionState.allowsMpduAggregation(); }
};

class INET_API TxopDurationFilter : public cObjectResultFilter
{
  public:
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
    using cObjectResultFilter::receiveSignal;
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
