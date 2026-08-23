//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel;

// Selectable scalar flat-AWGN HT/VHT-SU radio with exact LDPC data decoding.
// NSS>1 is represented as ideal already-separated spatial streams: the same
// scalar SNIR is applied independently to each stream.  No channel matrices,
// precoding, inter-stream interference, or equalization are modeled here.
// Unsupported tuples and BCC/legacy frames retain the packet-level behavior
// or are rejected by the exact LDPC boundary as documented by the C++ model.
// Exact encoding serializes the PSDU, so protocol checksums and the 802.11 FCS
// must be configured as computed or otherwise serializable.
module Ieee80211LdpcSoftRadio extends Ieee80211Radio
{
    parameters:
        transmitter.typename = default("Ieee80211LdpcSoftTransmitter");
        receiver.typename = default("Ieee80211LdpcSoftReceiver");
        // Keep the inherited packet/PER model for legacy and BCC frames;
        // exact LDPC DATA decisions bypass it in the selectable receiver.
        receiver.errorModel.typename = default(signalAnalogRepresentation != "unitDisk" ? "Ieee80211NistErrorModel" : "");
        separateTransmissionParts = true;
        // Exact WHOLE reception composes gated PREAMBLE/HEADER decisions with
        // decoder-owned DATA.  Keeping reception unsplit preserves inherited
        // BCC/control-response timing and behavior.
        separateReceptionParts = false;
}
