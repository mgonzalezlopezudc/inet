//
// Copyright (C) 2006 OpenSim Ltd.
// Copyright (C) 2001 Eric Wu and Steve Woon, Monash University, Melbourne, Australia
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

//
// Required for MacAddress declarations
//
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.linklayer.ieee80211.mac.Ieee80211Frame;
import inet.linklayer.common.MacAddress;

namespace inet::ieee80211;

//
// Capability Information field format. This field is not used
// by the model (and is omitted from management frame formats too),
// because its fields are related to features not supported by this model
// (PCF, privacy, and beacons in ad-hoc mode). It is only provided here
// for completeness.
//
struct Ieee80211CapabilityInformation
{
    bool ESS;
    bool IBSS;
    bool CFPollable;
    bool CFPollRequest;
    bool privacy;
    bool DelayedBlockAck;
    bool InmediateBlockAck;
}

//
// Additional parameters for handover decisions
// (Not part of standard, for experimental purposes)
//
struct Ieee80211HandoverParameters
{
    @packetData;
    double avgBackoffTime;
    double avgWaitTime;
    double avgErrorRate;
    double estAvailBW;
}

//
// 802.11 reason code; values correspond to the standard.
//
enum Ieee80211ReasonCode
{

    RC_UNSPECIFIED = 1;
    RC_PREV_AUTH_EXPIRED = 2;
    RC_DEAUTH_MS_LEAVING = 3;
    RC_DISASS_INACTIVITY = 4;
    RC_DISASS_TOO_MANY_MS = 5;
    RC_NONAUTH_CLASS2 = 6;
    RC_NONASS_CLASS3 = 7;
    RC_DIASS_MS_LEAVING = 8;
    RC_NONAUTH_ASS_REQUEST = 9;
    RC_MESH_PEERING_CANCELLED = 52;
    RC_MESH_MAX_PEERS = 53;
    RC_MESH_CONFIGURATION_POLICY_VIOLATION = 54;
    RC_MESH_CLOSE_RCVD = 55;
    RC_MESH_MAX_RETRIES = 56;
    RC_MESH_CONFIRM_TIMEOUT = 57;
    RC_MESH_INVALID_GTK = 58;
    RC_MESH_INCONSISTENT_PARAMETERS = 59;
    RC_MESH_INVALID_SECURITY_CAPABILITY = 60;
    RC_MESH_PATH_ERROR_NO_PROXY_INFORMATION = 61;
    RC_MESH_PATH_ERROR_NO_FORWARDING_INFORMATION = 60;
    RC_MESH_PATH_ERROR_DESTINATION_UNREACHABLE = 63;
    RC_MAC_ADDRESS_ALREADY_EXISTS_IN_MBSS = 64;
    RC_MESH_CHANNEL_SWITCH_REGULATORY_REQUIREMENTS = 65;
    RC_MESH_CHANNEL_SWITCH_UNSPECIFIED = 66;
}

//
// 802.11 status code; values correspond to the standard.
//
enum Ieee80211StatusCode
{

    SC_SUCCESSFUL = 0;
    SC_UNSPECIFIED = 1;
    SC_UNSUP_CAP = 10;
    SC_REASS_DENIED = 11;
    SC_ASS_DENIED_UNKNOWN = 12;
    SC_AUTH_ALG0_UNSUP = 13;
    SC_AUTH_OUT_OF_SEQ = 14;
    SC_AUTH_CHALLENGE_FAIL = 15;
    SC_AUTH_TIMEOUT = 16;
    SC_ASS_TOO_MANY_MS = 17;
    SC_DATARATE_UNSUP = 18;
    SC_AUTH_REJECT_ANTI_CLOGING = 76;
    SC_AUTH_REJECT_FINITE_CYC_GROUP_UNS = 77;
    SC_TBTT_REQUEST = 78;
}

enum Ieee80211HwmpCode
{
    IE11S_GANN = 125;
    IE11S_RANN = 126;
    IE11S_PREQ = 130;
    IE11S_PREP = 131;
    IE11S_PERR = 132;
}

//
// Structure to store supported rates
//
struct Ieee80211SupportedRatesElement
{
    short numRates; // number of rates (max 8)
    double rate[8]; // in Mbit/s; should be multiple of 500 kbit/s
}

//
// HE-MCS map used by the HE Capabilities and HE Operation elements.
// Each entry is the maximum supported HE-MCS for NSS n+1; -1 means unsupported.
//
struct Ieee80211HeMcsNssMapElement
{
    short maxMcsForNss[8];
}

//
// Packet representation of the IEEE 802.11ax HE Capabilities element fields
// used by the packet-level HE OFDMA model. The serializer maps these fields
// into the standard Element ID Extension element body and leaves unrelated
// HE capability bits reserved/zero.
//
struct Ieee80211HeCapabilitiesElement
{
    bool supportedChannelWidth40MHz;
    bool supportedChannelWidth80MHz;
    bool supportedChannelWidth160MHz;
    bool supportedChannelWidth80Plus80MHz;
    Ieee80211HeMcsNssMapElement rxMcsNss;
    Ieee80211HeMcsNssMapElement txMcsNss;
    bool dlOfdma;
    bool ulOfdma;
    bool dcm;
    short maxDcmConstellation;
    short maxDcmNss;
    bool ldpc;
    bool preamblePuncturing;
    bool multiTidAggregationRx;
    bool multiTidAggregationTx;
    bool muBarTriggerRx;
    bool heTbBlockAckTx;
    short maxAmpduLengthExponent;
    int maxMpduLength;
    short maxBlockAckBufferSize;
    bool ru26Tone;
    bool ru52Tone;
    bool ru106Tone;
    bool ru242Tone;
    bool ru484Tone;
    bool ru996Tone;
    bool ru1992Tone;
    bool dlMuMimoBeamformer;
    bool dlMuMimoBeamformee;
    short soundingDimensions;
    short beamformeeSts20Mhz;
    short beamformeeStsAbove20Mhz;
    short feedbackMode;
}

//
// Packet representation of the HE Operation element fields consumed by the
// model. Unsupported optional operation subfields are serialized as absent.
//
struct Ieee80211HeOperationElement
{
    short bssColor;
    int operatingChannelWidthMHz;
    short basicHeMcsNss;
    bool defaultPeDurationPresent;
    short defaultPeDurationUs;
}

//
// Packet representation of HE 6 GHz Band Capabilities. The element is only
// present when explicitly requested by the frame.
//
struct Ieee80211He6GhzBandCapabilitiesElement
{
    short minimumMpduStartSpacing;
    short maxAmpduLengthExponent;
    int maxMpduLength;
}

//
// Frame body base class used to hide various frame body types
//
class Ieee80211MgmtFrame extends FieldsChunk
{
    bool heCapabilitiesPresent;
    Ieee80211HeCapabilitiesElement heCapabilities;
    bool heOperationPresent;
    Ieee80211HeOperationElement heOperation;
    bool he6GhzBandCapabilitiesPresent;
    Ieee80211He6GhzBandCapabilitiesElement he6GhzBandCapabilities;
}

//
// Authentication frame body format.
//
// Note: the "isLast" field is not part of the standard.
// The authentication procedure is simulated by this model by exchanging
// a number of "dummy" authentication frames without real contents;
// the "isLast" field gets set by the AP to signal the STA that
// the authentication procedure has completed and it does not need to
// send any more Authentication frames.
//
class Ieee80211AuthenticationFrame extends Ieee80211MgmtFrame
{
    chunkLength = B(6); // add 2+len bytes for challenge text if present
    unsigned short sequenceNumber;
    Ieee80211StatusCode statusCode;
    bool isLast; // not part of the standard; see above
}

//
// De-authentication frame body format
//
class Ieee80211DeauthenticationFrame extends Ieee80211MgmtFrame
{
    chunkLength = B(2);
    Ieee80211ReasonCode reasonCode;
}

//
// Dis-association frame body format (same as De-authentication)
//
class Ieee80211DisassociationFrame extends Ieee80211MgmtFrame
{
    chunkLength = B(2);
    Ieee80211ReasonCode reasonCode;
}

//
// Probe request frame body format
//
class Ieee80211ProbeRequestFrame extends Ieee80211MgmtFrame
{
    string SSID;
    Ieee80211SupportedRatesElement supportedRates;
}

//
// Association request frame body format
//
class Ieee80211AssociationRequestFrame extends Ieee80211MgmtFrame
{
    string SSID;
    Ieee80211SupportedRatesElement supportedRates;
    double transmitPowerDbm = 15;
}

//
// Re-association request frame body format (same as association)
//
class Ieee80211ReassociationRequestFrame extends Ieee80211AssociationRequestFrame
{
    MacAddress currentAP;
}

//
// Association response frame body format
//
class Ieee80211AssociationResponseFrame extends Ieee80211MgmtFrame
{
    Ieee80211StatusCode statusCode;
    short aid;
    Ieee80211SupportedRatesElement supportedRates;
}

//
// Re-association response frame body format (same as AssociationResponse)
//
class Ieee80211ReassociationResponseFrame extends Ieee80211AssociationResponseFrame
{
}

//
// Beacon frame body format.
//
class Ieee80211BeaconFrame extends Ieee80211MgmtFrame
{
    string SSID;
    Ieee80211SupportedRatesElement supportedRates;
    simtime_t beaconInterval;
    int channelNumber;
    Ieee80211HandoverParameters handoverParameters;     //TODO is it a vendor-specific parameter in serializer?
}

//
// Probe response frame body format (same as Beacon)
//
class Ieee80211ProbeResponseFrame extends Ieee80211BeaconFrame
{
}

struct Ieee80211HeNdpStaInfo
{
    uint16_t aid;
    uint8_t feedbackType;
    uint8_t nc;
}

class Ieee80211HeNdpAnnouncement extends Ieee80211MgmtFrame
{
    uint8_t dialogToken;
    Ieee80211HeNdpStaInfo stations[];
}

class Ieee80211HeCompressedBeamformingFeedback extends Ieee80211MgmtFrame
{
    uint8_t dialogToken;
    uint16_t aid;
    double feedbackBandwidth;
    uint8_t nc;
    uint8_t nr;
    bool valid = true;
}
