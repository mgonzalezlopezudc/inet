// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee802154.contract;

// Paired gate-free PHY role. A concrete provider must resolve its typed client.
moduleinterface IIeee802154PhyServiceProvider
{
    parameters:
        string clientModule;
}
