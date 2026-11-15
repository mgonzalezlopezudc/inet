// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE802154PHYSERVICERESULT_H
#define __INET_IEEE802154PHYSERVICERESULT_H

#include <cstdint>
#include <variant>

namespace inet {
namespace physicallayer {

using Ieee802154PhyServiceRequestId = uint64_t;

/** Client-issued IDs never repeat across operation kinds, refusal, reset or restart. */
constexpr Ieee802154PhyServiceRequestId IEEE802154_PHY_INVALID_REQUEST_ID = 0;

/** Local admission; a refusal produces no callback and transfers no packet. */
enum class Ieee802154PhyServiceAdmission : uint8_t {
    ACCEPTED,
    BUSY,
    UNAVAILABLE,
    INVALID_REQUEST,
    UNSUPPORTED_OPERATION,
};

enum class Ieee802154PhyServiceCancelResult : uint8_t {
    CANCELLED,
    NOT_PENDING,
    NOT_CANCELLABLE,
};

enum class Ieee802154PhyServiceAbortReason : uint8_t {
    CANCELLED,
    RESET,
    STOPPED,
};

/**
 * Local PHY API outcomes, not IEEE primitive status codes or wire values.
 * Channel occupancy belongs to a successful CCA result, not request admission.
 */
enum class Ieee802154PhyServiceStatus : uint8_t {
    SUCCESS,
    INVALID_PARAMETER,
    UNSUPPORTED_ATTRIBUTE,
    READ_ONLY,
    UNSUPPORTED_CONFIGURATION,
    WRONG_TRANSCEIVER_STATE,
    FRAME_TOO_LONG,
    TRANSMISSION_FAILURE,
};

using Ieee802154PhyServiceResult = std::variant<Ieee802154PhyServiceStatus, Ieee802154PhyServiceAbortReason>;

} // namespace physicallayer
} // namespace inet

#endif
