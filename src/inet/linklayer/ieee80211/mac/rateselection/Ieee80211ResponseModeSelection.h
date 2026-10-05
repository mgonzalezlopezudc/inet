// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IEEE80211RESPONSEMODESELECTION_H
#define __INET_IEEE80211RESPONSEMODESELECTION_H

#include "inet/linklayer/ieee80211/mac/contract/Ieee80211ControlMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet::ieee80211 {

// Pure calculations for IEEE Std 802.11-2024, 10.6.6 and 10.6.11.
class INET_API Ieee80211ResponseModeSelection
{
  public:
    static ResponseModeResult computeResponse(const ResponseModeInput& input);
    static PreparedModeResult computeRts(const PreparedModeInput& input,
            const physicallayer::IIeee80211Mode *overrideMode);
    // A null override selects the primary. An illegal override raises an error.
    static const physicallayer::IIeee80211Mode *selectResponse(const ResponseModeResult& result,
            const physicallayer::IIeee80211Mode *overrideMode);
    // An empty reason means that the actual complete response is permitted.
    static std::string validateResponse(const ResponseModeResult& result, const Packet *response);
    static bool containsMode(const ResponseModeResult& result, const physicallayer::IIeee80211Mode *mode);
    static std::optional<BssRateContextRef> getFrameContext(const Packet *frame);
    static std::optional<MacAddress> getRequestBssid(const Ptr<const Ieee80211MacHeader>& header);

    template<typename Policy>
    static ResponseModeInput makeInput(Policy *policy, const Packet *request,
            const Ptr<const Ieee80211MacHeader>& header, const physicallayer::IIeee80211Mode *mode,
            ControlResponseKind kind, ResponseRequestRole role, const physicallayer::Ieee80211ModeSet *modes)
    {
        auto context = role == ResponseRequestRole::ORIGINATED ? getFrameContext(request) : std::nullopt;
        return {kind, mode, kind == ControlResponseKind::ACK ? LENGTH_ACK : LENGTH_CTS,
            policy->snapshotResponseRateContext(header, role, context), modes, header->getOrder()};
    }

    template<typename Policy>
    static const physicallayer::IIeee80211Mode *predictResponseMode(Policy *policy, const Packet *request,
            const Ptr<const Ieee80211MacHeader>& header, ControlResponseKind kind,
            const physicallayer::Ieee80211ModeSet *modes)
    {
        auto tag = request->findTag<physicallayer::Ieee80211ModeReq>();
        auto result = policy->computeResponseMode(makeInput(policy, request, header, tag ? tag->getMode() : nullptr,
            kind, ResponseRequestRole::ORIGINATED, modes));
        if (result.status != ModePreparationStatus::READY)
            throw cRuntimeError("Unsupported response prediction: %s", result.reason.c_str());
        return result.primaryMode;
    }
};

} // namespace inet::ieee80211

#endif
