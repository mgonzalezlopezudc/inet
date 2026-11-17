// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IEEE802154ADDRESS_H
#define __INET_IEEE802154ADDRESS_H

#include "inet/common/INETDefs.h"

namespace inet {

/** Native IEEE 802.15.4 address; PAN scope is carried separately by the link metadata. */
class INET_API Ieee802154Address
{
  public:
    enum Mode : int { UNSPECIFIED = 0, SHORT = 2, EXTENDED = 3 };

  protected:
    Mode mode = UNSPECIFIED;
    uint64_t value = 0;

  public:
    Ieee802154Address() = default;
    Ieee802154Address(Mode mode, uint64_t value);
    explicit Ieee802154Address(const char *text);
    Mode getMode() const { return mode; }
    uint64_t getInt() const { return value; }
    bool isUnspecified() const { return mode == UNSPECIFIED; }
    bool isBroadcast() const { return mode == SHORT && value == 0xffff; }
    std::string str() const;
    bool operator==(const Ieee802154Address& other) const { return mode == other.mode && value == other.value; }
    bool operator!=(const Ieee802154Address& other) const { return !(*this == other); }
    bool operator<(const Ieee802154Address& other) const { return mode != other.mode ? mode < other.mode : value < other.value; }
};

inline std::ostream& operator<<(std::ostream& stream, const Ieee802154Address& address) { return stream << address.str(); }
void INET_API doParsimPacking(cCommBuffer *buffer, const Ieee802154Address& address);
void INET_API doParsimUnpacking(cCommBuffer *buffer, Ieee802154Address& address);

} // namespace inet

#endif
