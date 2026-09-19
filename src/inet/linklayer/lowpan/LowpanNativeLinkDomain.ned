// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

// Static native PAN membership and routed next-hop bindings, without aliases.
simple LowpanNativeLinkDomain
{
    parameters:
        int panId = default(-1);
        string radioMediumModule = default("^.radioMedium");
        xml config = default(xml("<domain/>"));
        @class(LowpanNativeLinkDomain);
        @display("i=block/table");
}
