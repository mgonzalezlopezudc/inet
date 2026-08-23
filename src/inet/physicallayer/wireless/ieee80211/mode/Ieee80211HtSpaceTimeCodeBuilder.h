//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTSPACETIMECODEBUILDER_H
#define __INET_IEEE80211HTSPACETIMECODEBUILDER_H

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"

namespace inet {
namespace physicallayer {

/**
 * Constructs the bounded HT Alamouti descriptor.  The constants are the sole
 * implementation of IEEE Std 802.11-2024 Clause 19.3.11.9.2/Table 19-18;
 * the 1/sqrt(2) amplitude follows the normalization requirement in Clause
 * 19.3.11.11.2.
 */
class INET_API Ieee80211HtSpaceTimeCodeBuilder final
{
  public:
    static SpaceTimeCodeDescriptor build(int numberOfSpatialStreams,
        int numberOfSpaceTimeStreams, int stbc);
};

} // namespace physicallayer
} // namespace inet

#endif
