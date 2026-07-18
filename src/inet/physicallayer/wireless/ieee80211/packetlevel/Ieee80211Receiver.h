//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211RECEIVER_H
#define __INET_IEEE80211RECEIVER_H

#include <set>
#include <string>

#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {

namespace physicallayer {

class INET_API Ieee80211Receiver : public FlatReceiverBase
{
  protected:
    static simsignal_t heSpatialReuseBssTypeSignal;
    static simsignal_t heSpatialReuseReceivedBssColorSignal;
    static simsignal_t heSpatialReuseLocalBssColorSignal;
    static simsignal_t heSpatialReuseEligibleSignal;
    static simsignal_t heSpatialReuseIgnoredPpduSignal;
    static simsignal_t heSpatialReuseObssPdThresholdSignal;
    static simsignal_t heSpatialReuseTransmitPowerLimitSignal;
    static simsignal_t heSpatialReuseReasonSignal;

    const Ieee80211ModeSet *modeSet = nullptr;
    const IIeee80211Band *band = nullptr;
    const Ieee80211Channel *channel = nullptr;
    bool enableSpatialReuse = false;
    W obssPdThreshold = W(NaN);
    W nonSrgObssPdThreshold = W(NaN);
    W srgObssPdThreshold = W(NaN);
    bool enableNonSrgSpatialReuse = true;
    bool enableSrgSpatialReuse = true;
    bool enableParameterizedSpatialReuse = false;
    double obssPdMinThresholdDbm = NaN;
    double spatialReusePowerReferenceDbm = NaN;
    std::set<int> srgBssColors;
    mutable bool lastHeReception = false;
    mutable int lastHePpduFormat = -1;
    mutable int lastHeUserCount = 0;
    mutable int lastHeBssColor = 0;
    mutable bool lastHeRuAssigned = false;
    mutable int lastSpatialReuseBssType = 0;
    mutable bool lastSpatialReuseEligible = false;
    mutable bool lastSpatialReuseIgnoredPpdu = false;
    mutable W lastSpatialReuseObssPdThreshold = W(NaN);
    mutable W lastSpatialReuseTransmitPowerLimit = W(NaN);
    mutable std::string lastSpatialReuseReason = "";

    enum class HeSpatialReuseBssType {
        UNSPECIFIED,
        INTRA_BSS,
        INTER_BSS_NON_SRG,
        INTER_BSS_SRG
    };

    enum class HeSpatialReuseReason {
        SPATIAL_REUSE_DISABLED,
        NOT_HE_PPDU,
        RECEIVED_COLOR_DISABLED,
        LOCAL_COLOR_DISABLED,
        INTRA_BSS_PPDU,
        SRG_DISABLED,
        SRG_DISALLOWED,
        NON_SRG_DISABLED,
        NON_SRG_DISALLOWED,
        TB_EXCLUDED,
        PSR_NOT_PERMITTED,
        INTER_BSS_BELOW_OBSS_PD,
        INTER_BSS_AT_OR_ABOVE_OBSS_PD,
    };

    struct HeSpatialReuseDecision {
        HeSpatialReuseBssType bssType = HeSpatialReuseBssType::UNSPECIFIED;
        int receivedBssColor = 0;
        int localBssColor = 0;
        bool eligible = false;
        bool ignorePpdu = false;
        W obssPdThreshold = W(NaN);
        W transmitPowerLimit = W(NaN);
        HeSpatialReuseReason reasonCode = HeSpatialReuseReason::SPATIAL_REUSE_DISABLED;
        const char *reason = nullptr;
    };

  protected:
    virtual bool isAssignedHeMuRu(const ITransmission *transmission) const;
    bool shouldIgnoreReceptionDueToHeSpatialReuse(const IListening *listening, const IReception *reception, bool logDecision) const;
    virtual HeSpatialReuseDecision computeHeSpatialReuseDecision(const IListening *listening, const IReception *reception) const;
    virtual W computeSpatialReuseTransmitPowerLimit(W threshold) const;

  protected:
    virtual void initialize(int stage) override;
    virtual void recordHeSpatialReuseDecision(const HeSpatialReuseDecision& decision, bool emitSignals) const;
    virtual const char *getLastSpatialReuseBssTypeName() const;
    virtual std::string getLastHeReceptionSummary() const;

    virtual bool computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const override;
    virtual bool computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const override;
    virtual bool computeIsReceptionAttempted(const IListening *listening, const IReception *reception,
            IRadioSignal::SignalPart part, const IInterference *interference) const override;
    virtual const IListeningDecision *computeListeningDecision(const IListening *listening, const IInterference *interference) const override;

    virtual const IReceptionResult *computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const override;

  public:
    virtual ~Ieee80211Receiver();

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual void setModeSet(const Ieee80211ModeSet *modeSet);
    virtual void setBand(const IIeee80211Band *band);
    virtual void setChannel(const Ieee80211Channel *channel);
    virtual void setChannelNumber(int channelNumber);
};

} // namespace physicallayer

} // namespace inet

#endif
