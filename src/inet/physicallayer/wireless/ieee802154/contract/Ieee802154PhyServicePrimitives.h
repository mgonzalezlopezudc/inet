// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE802154PHYSERVICEPRIMITIVES_H
#define __INET_IEEE802154PHYSERVICEPRIMITIVES_H

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServiceResult.h"

namespace inet {
namespace physicallayer {

/** PHY-specific descriptor; channel numbers alone do not identify a PHY. */
struct Ieee802154PhyChannel
{
    enum class Phy : uint8_t { O_QPSK, HRP_UWB };
    enum class Band : uint8_t { MHZ_2450, MHZ_915 };

    Phy phy;
    Band band;
    int channel;

    bool operator==(const Ieee802154PhyChannel& other) const
    {
        return phy == other.phy && band == other.band && channel == other.channel;
    }
};

enum class Ieee802154PhyCcaMode : uint8_t {
    MODE_1, MODE_2, MODE_3A, MODE_3B, MODE_4, MODE_5, MODE_6,
};

enum class Ieee802154PhyState : uint8_t {
    OFF, RECEIVE, TRANSMIT,
};

/** Native lifecycle notification, independent of a correlated IEEE service request. */
struct Ieee802154PhyAvailabilityIndication
{
    bool available;
    Ieee802154PhyState state;
};

enum class Ieee802154PhyCcaOutcome : uint8_t {
    IDLE, BUSY,
};

/** Timing snapshot of the selected profile, not universal 802.15.4 constants. */
struct Ieee802154PhyTimingParameters
{
    simtime_t symbolPeriod;
    simtime_t turnaroundTime;
    simtime_t shrDuration;
    simtime_t phrDuration;
    units::values::bps bitrate;
    units::values::B maxPsduLength;
    units::values::B fcsLength;
};

/** Absolute simulation instants. Successful selected O-QPSK PSDUs end with the PPDU. */
struct Ieee802154PhyFrameTiming
{
    simtime_t ppduStart;
    simtime_t psduStart;
    simtime_t psduEnd;
    simtime_t ppduEnd;
};

/** Start is observed; future instants are predictions until terminal confirmation. */
struct Ieee802154PhyTransmitStart
{
    simtime_t ppduStart;
    simtime_t expectedPsduStart;
    simtime_t expectedPpduEnd;
};

struct Ieee802154PhyMeasurementTiming
{
    simtime_t start;
    simtime_t end;
};

/** Native device identity is shared with the MAC; no concrete MAC is referenced. */
struct Ieee802154PhyPeerPower
{
    Ieee802154Address address;
    int64_t powerDbm;
    bool operator==(const Ieee802154PhyPeerPower& other) const { return address == other.address && powerDbm == other.powerDbm; }
};

/**
 * Typed model representation. Durations use simtime_t, Tx power attributes
 * use signed integer dBm, and the implementation-dependent CCA threshold uses
 * W. This variant is not an over-the-air or standardized PD/PLME-SAP encoding.
 */
using Ieee802154PhyPibValue = std::variant<bool, int64_t, uint64_t, simtime_t,
        units::values::W, Ieee802154PhyCcaMode, Ieee802154PhyChannel, std::vector<Ieee802154PhyPeerPower>, Ieee802154Address, std::vector<uint8_t>>;

struct Ieee802154PhyTransmitRequest
{
    // Absence selects phyTxPower; addressed frames use the documented PHY power lookup.
    std::optional<Ieee802154Address> destination;
    // Absolute PPDU target while already TRANSMIT-ready; absence selects the current instant.
    std::optional<simtime_t> startAt;
};

struct Ieee802154PhyTransmitConfirm
{
    Ieee802154PhyServiceResult result;
    std::optional<Ieee802154PhyFrameTiming> timing; ///< Present only on SUCCESS.

    explicit Ieee802154PhyTransmitConfirm(Ieee802154PhyServiceResult result) : result(std::move(result)) {}
};

struct Ieee802154PhyCcaConfirm
{
    Ieee802154PhyServiceResult result;
    std::optional<Ieee802154PhyCcaOutcome> outcome;
    std::optional<Ieee802154PhyMeasurementTiming> timing;

    explicit Ieee802154PhyCcaConfirm(Ieee802154PhyServiceResult result) : result(std::move(result)) {}
};

struct Ieee802154PhyEdConfirm
{
    Ieee802154PhyServiceResult result;
    std::optional<uint8_t> level;
    std::optional<units::values::W> meanPower;
    std::optional<Ieee802154PhyMeasurementTiming> timing;

    explicit Ieee802154PhyEdConfirm(Ieee802154PhyServiceResult result) : result(std::move(result)) {}
};

struct Ieee802154PhyStateConfirm
{
    Ieee802154PhyServiceResult result;
    std::optional<Ieee802154PhyState> state;
    std::optional<simtime_t> readyAt;

    explicit Ieee802154PhyStateConfirm(Ieee802154PhyServiceResult result) : result(std::move(result)) {}
};

struct Ieee802154PhyGetRequest
{
    std::string attribute;
};

struct Ieee802154PhyGetConfirm
{
    std::string attribute;
    Ieee802154PhyServiceResult result;
    std::optional<Ieee802154PhyPibValue> value;

    Ieee802154PhyGetConfirm(std::string attribute, Ieee802154PhyServiceResult result) :
        attribute(std::move(attribute)), result(std::move(result)) {}
};

struct Ieee802154PhySetRequest
{
    std::string attribute;
    Ieee802154PhyPibValue value;
};

struct Ieee802154PhySetConfirm
{
    std::string attribute;
    Ieee802154PhyServiceResult result;

    Ieee802154PhySetConfirm(std::string attribute, Ieee802154PhyServiceResult result) :
        attribute(std::move(attribute)), result(std::move(result)) {}
};

struct Ieee802154PhyResetConfirm
{
    bool setDefaultPib;
    Ieee802154PhyServiceResult result;

    Ieee802154PhyResetConfirm(bool setDefaultPib, Ieee802154PhyServiceResult result) :
        setDefaultPib(setDefaultPib), result(std::move(result)) {}
};

/** Successful PHY reception; the client still owns MAC parsing and MAC FCS verification. */
struct Ieee802154PhyReceiveIndication
{
    Ieee802154PhyChannel channel;
    Ieee802154PhyFrameTiming timing;
    units::values::B fcsLength;
    std::optional<uint8_t> lqi;
    std::optional<units::values::W> meanPower; ///< Mean desired-signal power over the PPDU, excluding noise/interference.
};

} // namespace physicallayer
} // namespace inet

#endif
