//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211CCAPROVIDER_H
#define __INET_IEEE80211CCAPROVIDER_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/**
 * Local PHY-CCA state per 20 MHz subchannel of the operating channel, in the
 * spirit of the PHY-CCA.indication per20bitmap parameter (IEEE Std 802.11-2024,
 * 27.3.22.6.5 and 802.11be 36.3.21.6.4). When the operating channel is wider
 * than 20 MHz, each bit of the busy mask reports the CCA state of one 20 MHz
 * subchannel, ordered from the lowest to the highest center frequency.
 */
class INET_API Ieee80211CcaSnapshot : public cObject
{
  protected:
    int numSubchannels = 1;
    int primarySubchannelIndex = 0;
    uint32_t busySubchannelMask = 0;

  public:
    Ieee80211CcaSnapshot(int numSubchannels = 1, int primarySubchannelIndex = 0, uint32_t busySubchannelMask = 0) :
        numSubchannels(numSubchannels), primarySubchannelIndex(primarySubchannelIndex), busySubchannelMask(busySubchannelMask) {}

    virtual Ieee80211CcaSnapshot *dup() const override { return new Ieee80211CcaSnapshot(*this); }

    /** True when subchannel CCA state is being tracked (operating channel width >= 40 MHz). */
    bool isWideCca() const { return numSubchannels > 1; }
    int getNumSubchannels() const { return numSubchannels; }
    int getPrimarySubchannelIndex() const { return primarySubchannelIndex; }
    uint32_t getBusySubchannelMask() const { return busySubchannelMask; }
    bool isSubchannelBusy(int index) const { return (busySubchannelMask & (1u << index)) != 0; }
    bool isPrimaryBusy() const { return isSubchannelBusy(primarySubchannelIndex); }

    // HT40 compatibility accessors: a 40 MHz operating channel has exactly one
    // secondary 20 MHz subchannel.
    bool isHt40() const { return numSubchannels == 2; }
    bool isSecondaryBusy() const { return isSubchannelBusy(primarySubchannelIndex ^ 1); }
};

/**
 * Provides the local IEEE 802.11 per-channel PHY-CCA state. The snapshot is
 * local PHY/MAC control information and is not transmitted on air.
 */
class INET_API IIeee80211CcaProvider
{
  public:
    static simsignal_t ccaStateChangedSignal;

    virtual ~IIeee80211CcaProvider() {}

    virtual const Ieee80211CcaSnapshot& getCcaSnapshot() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
