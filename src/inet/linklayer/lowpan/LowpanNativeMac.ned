// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;
import inet.linklayer.base.MacProtocolBase;
import inet.linklayer.contract.IMacProtocol;

// Fixed 2.4GHz, unsecured, same-PAN data/ACK profile. PHY timing is in symbols.
simple LowpanNativeMac extends MacProtocolBase like IMacProtocol
{
    parameters:
        string linkDomainModule = default("");
        string radioModule = default("^.radio");
        double maxLowerDeliveryLifetime @unit(s) = default(1s);
        int macMaxCSMABackoffs = default(4);
        int macMaxFrameRetries = default(3);
        int macMinBE = default(3);
        int macMaxBE = default(5);
        @class(LowpanNativeMac);
        @display("i=block/rxtx");
}
