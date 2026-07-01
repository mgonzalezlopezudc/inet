//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_EHTULMUTXOPFS_H
#define __INET_EHTULMUTXOPFS_H

#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"

namespace inet {
namespace ieee80211 {

/**
 * AP-side frame sequence for an EHT uplink MU-OFDMA TXOP.
 * Inherits from HeUlMuTxOpFs to reuse the Trigger-based exchange logic,
 * but customized for EHT formats and capabilities.
 */
class INET_API EhtUlMuTxOpFs : public HeUlMuTxOpFs
{
  public:
    EhtUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback, const IIeee80211HeUlScheduler::Schedule& schedule,
                  IIeee80211HeUlTriggerPolicy::TriggerType triggerType, physicallayer::Ieee80211ModeSet *modeSet,
                  MacAddress apAddress, bool ehtEnabled = true);
    
    virtual ~EhtUlMuTxOpFs() override;
    
  protected:
    bool ehtEnabled = true;

    // virtual void constructTriggerFrame() override; // Example customization
};

} // namespace ieee80211
} // namespace inet

#endif
