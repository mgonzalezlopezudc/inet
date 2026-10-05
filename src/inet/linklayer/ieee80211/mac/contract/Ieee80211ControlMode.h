// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IEEE80211CONTROLMODE_H
#define __INET_IEEE80211CONTROLMODE_H

#include <optional>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211RateContext.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet::ieee80211 {

using ResponseRateContext = RateContextSnapshot;

enum class ResponseRequestRole { ORIGINATED, RECEIVED };
enum class ControlResponseKind { ACK, CTS };
enum class ModePreparationStatus { READY, UNSUPPORTED };
enum class PreparedControlPosition { TXOP_INITIAL, TXOP_CONTINUATION };

struct INET_API ResponseModeInput
{
    ControlResponseKind kind = ControlResponseKind::ACK;
    const physicallayer::IIeee80211Mode *requestMode = nullptr;
    b length = LENGTH_ACK;
    ResponseRateContext rates;
    const physicallayer::Ieee80211ModeSet *transmitModes = nullptr;
    // The supported profile has no HT Control, beamforming, or STBC procedure.
    bool requiresHtVhtResponse = false;
};

struct INET_API ResponseModeResult
{
    ModePreparationStatus status = ModePreparationStatus::UNSUPPORTED;
    const physicallayer::IIeee80211Mode *primaryMode = nullptr;
    std::vector<const physicallayer::IIeee80211Mode *> permittedModes;
    simtime_t airtime;
    ResponseModeInput input;
    std::string reason;
};

struct INET_API PreviousPeerTransmission
{
    MacAddress peer;
    const physicallayer::IIeee80211Mode *mode = nullptr;
    int64_t packetId = -1;
    uint64_t txopGeneration = 0;
};

struct INET_API PreparedModeInput
{
    const Packet *frame = nullptr;
    Ptr<const Ieee80211MacHeader> header;
    PreparedControlPosition position = PreparedControlPosition::TXOP_INITIAL;
    std::optional<PreviousPeerTransmission> previousToSamePeer;
    ResponseRateContext rates;
    const physicallayer::Ieee80211ModeSet *transmitModes = nullptr;
};

struct INET_API PreparedModeResult
{
    ModePreparationStatus status = ModePreparationStatus::UNSUPPORTED;
    const physicallayer::IIeee80211Mode *mode = nullptr;
    std::string reason;
};

} // namespace inet::ieee80211

#endif
