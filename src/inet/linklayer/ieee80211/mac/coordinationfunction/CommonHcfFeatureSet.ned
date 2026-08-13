//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IHcfFeatureSet;

// Composition-only default for common HCF exchanges.
simple CommonHcfFeatureSet extends SimpleModule like IHcfFeatureSet
{
    parameters:
        @class(CommonHcfFeatureSet);
        @display("i=block/control");
}
