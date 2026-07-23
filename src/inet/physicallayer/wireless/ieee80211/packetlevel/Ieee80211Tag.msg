//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.TagBase;
import inet.physicallayer.wireless.ieee80211.mode.IIeee80211Mode;
import inet.physicallayer.wireless.ieee80211.mode.Ieee80211Channel;

namespace inet::physicallayer;

enum Ieee80211MpduReceiveStatus
{
    MPDU_SUCCESS = 0;
    MPDU_DELIMITER_ERROR = 1;
    MPDU_HEADER_ERROR = 2;
    MPDU_PAYLOAD_ERROR = 3;
    MPDU_FCS_ERROR = 4;
    MPDU_NOT_EVALUATED = 5;
}

struct Ieee80211MpduReceiveResult
{
    int sequenceNumber = -1;
    int fragmentNumber = -1;
    int tid = -1;
    B offset;
    B length;
    Ieee80211MpduReceiveStatus status = MPDU_NOT_EVALUATED;
}

class Ieee80211MpduReceiveInd extends TagBase
{
    Ieee80211MpduReceiveResult results[];
}

//
// Model-only correlation between a Trigger exchange and its terminal
// Multi-STA Block Ack. This is deliberately not serialized as an 802.11
// field; it prevents a delayed or foreign response from owning another
// Trigger's packet ledger.
//
class Ieee80211HeTriggerCorrelationTag extends TagBase
{
    uint32_t triggerId;
}

//
// Indicates the legacy duration observed for an undecodable IEEE 802.11
// transmission. MAC logic uses it to maintain NAV compatibility without
// treating this model-only notification as a decoded HE PPDU.
//
class Ieee80211LegacyPreambleInd extends TagBase
{
    simtime_t durationField;
}


//
// This is an abstract base class that should not be directly added as a tag.
//
class Ieee80211ModeTagBase extends TagBase
{
    const IIeee80211Mode *mode; // IEEE 802.11 transmission mode
}

//
// This request determines the IEEE 802.11 mode that should be used to transmit the packet.
// It may be present on a packet from the application to the phyisical layer.
//
class Ieee80211ModeReq extends Ieee80211ModeTagBase
{
}

//
// This indication specifies the IEEE 802.11 mode that was used to receive the packet.
// It may be present on a packet from the phyiscal layer to the application.
//
class Ieee80211ModeInd extends Ieee80211ModeTagBase
{
}

//
// This is an abstract base class that should not be directly added as a tag.
//
class Ieee80211ChannelTagBase extends TagBase
{
    const Ieee80211Channel *channel; // IEEE 802.11 band and channel
}

//
// This request determines the IEEE 802.11 channel that should be used to transmit the packet.
// It may be present on a packet from the application to the phyisical layer.
//
class Ieee80211ChannelReq extends Ieee80211ChannelTagBase
{
}

//
// This indication specifies the IEEE 802.11 channel that was used to receive the packet.
// It may be present on a packet from the phyiscal layer to the application.
//
class Ieee80211ChannelInd extends Ieee80211ChannelTagBase
{
}

//
// Optional VHT transmission metadata used by the packet-level error model to
// apply the configured beamforming gain or MU-MIMO penalty.
//
class Ieee80211VhtTransmissionTag extends TagBase
{
    bool beamformed = false;
    bool muMimo = false;
    double beamformingGainDb = 3.0;
    double muMimoPenaltyDb = 2.0;
}
