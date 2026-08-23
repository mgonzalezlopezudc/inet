//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCSOFTTRANSMITTER_H
#define __INET_IEEE80211LDPCSOFTTRANSMITTER_H

#include "inet/physicallayer/wireless/common/base/packetlevel/ApskModulationBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211LdpcSoftTransmissionModel.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211LdpcSoftTransmitter : public Ieee80211Transmitter
{
  protected:
    static simsignal_t ldpcDataEncodedSignal;

    static BitVector extractPsduBits(const Packet *packet,
            const Ptr<const Ieee80211PhyHeader>& phyHeader,
            const Ieee80211DataEncodingPlan& plan);
    static std::vector<const ApskModulationBase *> getStreamSubcarrierModulations(
            const IIeee80211DataMode *dataMode);
    static Ieee80211LdpcSoftTransmissionModel::SymbolBlocks makeSymbols(
            const Ieee80211LdpcMappedData& mappedData,
            const std::vector<const ApskModulationBase *>& modulations);

  public:
    virtual const ITransmission *createTransmission(const IRadio *radio, const Packet *packet,
            simtime_t startTime) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
