// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee802154.contract;

// Paired gate-free client role. Binding belongs to concrete module composition.
moduleinterface IIeee802154PhyServiceClient
{
    parameters:
        string providerModule;
}
