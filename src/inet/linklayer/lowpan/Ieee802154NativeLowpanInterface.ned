// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

// Native IEEE 802.15.4 data/ACK wire path, retaining the adaptation-facing contract.
module Ieee802154NativeLowpanInterface extends Ieee802154LowpanInterface
{
    parameters:
        boundary.typename = default("LowpanNativeBoundary");
        mac.typename = default("LowpanNativeMac");
}
