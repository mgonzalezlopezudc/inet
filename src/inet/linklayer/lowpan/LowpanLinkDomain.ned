// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

// One static PAN on a dedicated compatibility medium. Paths in config are
// relative to the network root; peers name interface, native and alias attributes,
// neighbors name interface, ipv6 and native attributes.
simple LowpanLinkDomain
{
    parameters:
        int panId = default(-1); // Required non-broadcast PAN; -1 fails validation.
        string radioMediumModule = default("^.radioMedium");
        xml config = default(xml("<domain/>")); // Must explicitly configure peers.
        @class(LowpanLinkDomain);
        @display("i=block/table");
}
