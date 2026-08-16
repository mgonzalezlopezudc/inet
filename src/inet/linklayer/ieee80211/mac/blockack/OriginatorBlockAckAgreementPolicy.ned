//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.linklayer.ieee80211.mac.blockack;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IOriginatorBlockAckAgreementPolicy;

//
// Implements the default originator block ACK agreement policy
//
simple OriginatorBlockAckAgreementPolicy extends SimpleModule like IOriginatorBlockAckAgreementPolicy
{
    parameters:
        @class(OriginatorBlockAckAgreementPolicy);
        string originatorAckPolicyModule;

        bool delayedAckPolicySupported = default(false);
        bool aMsduSupported = default(true);
        int maximumAllowedBufferSize = default(64);
        double blockAckTimeoutValue @unit(s) = default(0s); // 0 means that it depends on the originator
        // Interim local HT Compressed Block Ack capability assumption until HT Capabilities IE state is modeled.
        bool localCompressedBlockAckSupported = default(false);
        // Explicit peers whose capability state permits HT Compressed Block Ack; empty keeps legacy Basic Block Ack behavior.
        string compressedBlockAckPeerAddresses = default("");
        @display("i=block/control");
}
