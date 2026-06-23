//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.linklayer.ieee80211.mib;

import inet.common.SimpleModule;

//
// Management Information Base for IEEE 802.11 networks. Stores configuration and
// state information including network mode (infrastructure, ad-hoc, mesh),
// station type (access point or station), authentication and association status,
// and BSS data (SSID, BSSID). Used by various IEEE 802.11 modules to maintain
// protocol state and control frame addressing.
//
simple Ieee80211Mib extends SimpleModule
{
    parameters:
        @class(Ieee80211Mib);
        bool vhtLdpc = default(false); // Advertises and enables VHT LDPC when the peer also supports it
        bool htLdpc = default(false); // Advertises and enables HT LDPC when the peer also supports it
        // HE features are opt-in so existing configurations retain BCC and
        // the unextended PPDU geometry.
        bool heLdpc = default(false); // Advertises and enables HE LDPC when the peer also supports it
        bool heTwtRequester = default(false); // Advertises HE TWT requester support
        bool heTwtResponder = default(false); // Advertises HE TWT responder support
        bool heBroadcastTwt = default(false); // Advertises HE broadcast TWT support
        bool heMultiTidAggregationRx = default(false); // Advertises support for receiving Multi-TID Block Acks
        bool heMultiTidAggregationTx = default(false); // Advertises support for transmitting Multi-TID Block Acks
        int heBssColor = default(0); // HE BSS color (1-63), 0 indicates disabled / unspecified
        int heDefaultPeDurationUs = default(0); // Default HE packet extension: 0, 4, 8, 12, or 16 microseconds
        bool heDlMuMimoBeamformer = default(false);
        bool heDlMuMimoBeamformee = default(false);
        int heSoundingDimensions = default(0);
        int heBeamformeeSts20Mhz = default(0);
        int heBeamformeeStsAbove20Mhz = default(0);
        int heFeedbackMode = default(0); // 0=None, 1=SU, 2=MU, 3=Both
        int vhtMaxNss = default(8); // Maximum VHT spatial streams advertised by this MIB
        int vhtMaxAmpduLengthExponent = default(7); // VHT maximum A-MPDU-length exponent advertised to peers
        bool vhtBeamforming = default(false); // Enables VHT transmit beamforming capability negotiation
        bool vhtMuMimo = default(false); // Enables VHT multi-user MIMO capability negotiation
        double vhtOperatingChannelWidth @unit(Hz) = default(20MHz); // VHT operating width advertised to peers
        displayStringTextFormat = default("Address: {address}{ssidStr}\n{modeStr}{stationTypeStr}{qosStr}{associatedStr}");
        @display("i=block/table");
}
