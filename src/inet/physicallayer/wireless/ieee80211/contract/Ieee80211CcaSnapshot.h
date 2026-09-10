//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211CCASNAPSHOT_H
#define __INET_IEEE80211CCASNAPSHOT_H

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"

namespace inet {
namespace physicallayer {

enum Ieee80211CcaGroup {
    IEEE80211_CCA_PRIMARY20,
    IEEE80211_CCA_SECONDARY20,
    IEEE80211_CCA_SECONDARY40,
    IEEE80211_CCA_SECONDARY80,
};

/**
 * Local IEEE 802.11 per-channel PHY-CCA state. The snapshot is local PHY/MAC
 * control information and is not transmitted on air.
 */
class INET_API Ieee80211CcaSnapshot : public cObject
{
  protected:
    bool enabled = false;
    Ieee80211ChannelWidth channelWidth = IEEE80211_CHANNEL_WIDTH_20MHZ;
    uint64_t configurationRevision = 0;
    bool primary20Busy = false;
    bool secondary20Busy = false;
    bool secondary40Busy = false;
    bool secondary80Busy = false;

  public:
    Ieee80211CcaSnapshot(bool enabled = false,
            Ieee80211ChannelWidth channelWidth = IEEE80211_CHANNEL_WIDTH_20MHZ,
            uint64_t configurationRevision = 0,
            bool primary20Busy = false,
            bool secondary20Busy = false,
            bool secondary40Busy = false,
            bool secondary80Busy = false) :
        enabled(enabled), channelWidth(channelWidth), configurationRevision(configurationRevision),
        primary20Busy(primary20Busy), secondary20Busy(secondary20Busy),
        secondary40Busy(secondary40Busy), secondary80Busy(secondary80Busy) {}

    // Compatibility constructor for code that only knows the pre-VHT HT40
    // snapshot shape.  New producers should use the complete constructor.
    Ieee80211CcaSnapshot(bool ht40, bool primaryBusy, bool secondaryBusy) :
        Ieee80211CcaSnapshot(ht40, IEEE80211_CHANNEL_WIDTH_40MHZ, 0, primaryBusy, secondaryBusy) {}

    virtual Ieee80211CcaSnapshot *dup() const override { return new Ieee80211CcaSnapshot(*this); }

    bool isEnabled() const { return enabled; }
    Ieee80211ChannelWidth getChannelWidth() const { return channelWidth; }
    uint64_t getConfigurationRevision() const { return configurationRevision; }
    bool isPrimary20Busy() const { return primary20Busy; }
    bool isSecondary20Busy() const { return secondary20Busy; }
    bool isSecondary40Busy() const { return secondary40Busy; }
    bool isSecondary80Busy() const { return secondary80Busy; }

    bool isBusy(Ieee80211CcaGroup group) const {
        switch (group) {
            case IEEE80211_CCA_PRIMARY20: return primary20Busy;
            case IEEE80211_CCA_SECONDARY20: return secondary20Busy;
            case IEEE80211_CCA_SECONDARY40: return secondary40Busy;
            case IEEE80211_CCA_SECONDARY80: return secondary80Busy;
            default: throw cRuntimeError("Unknown IEEE 802.11 CCA group: %d", (int)group);
        }
    }

    // Legacy HT40 aliases retained for existing MAC consumers.
    bool isHt40() const { return enabled && channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ; }
    bool isPrimaryBusy() const { return primary20Busy; }
    bool isSecondaryBusy() const { return secondary20Busy; }
};

} // namespace physicallayer
} // namespace inet

#endif
