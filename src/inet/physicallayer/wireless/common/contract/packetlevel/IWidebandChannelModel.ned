//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.common.contract.packetlevel;

//
// The wideband channel model describes link-specific channel effects whose
// power gain may vary over time and frequency.
//
moduleinterface IWidebandChannelModel
{
    parameters:
        @display("i=block/tunnel");
}
