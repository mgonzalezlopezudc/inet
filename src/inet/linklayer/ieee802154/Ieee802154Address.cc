// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inet/linklayer/ieee802154/Ieee802154Address.h"

#include <iomanip>
#include <sstream>

namespace inet {

Ieee802154Address::Ieee802154Address(Mode mode, uint64_t value) : mode(mode), value(value)
{
    if ((mode != UNSPECIFIED && mode != SHORT && mode != EXTENDED) ||
        (mode == UNSPECIFIED && value != 0) || (mode == SHORT && value > 0xffff))
        throw cRuntimeError("Invalid IEEE 802.15.4 address mode or width");
}

Ieee802154Address::Ieee802154Address(const char *text)
{
    if (text == nullptr)
        throw cRuntimeError("Null IEEE 802.15.4 address");
    std::string input(text);
    if (input == "unspecified")
        return;
    if (input.size() != 4 && input.size() != 16)
        throw cRuntimeError("IEEE 802.15.4 address requires 4 or 16 hexadecimal digits: %s", text);
    mode = input.size() == 4 ? SHORT : EXTENDED;
    for (char c : input) {
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            throw cRuntimeError("Invalid hexadecimal IEEE 802.15.4 address: %s", text);
        value = (value << 4) | digit;
    }
}

std::string Ieee802154Address::str() const
{
    if (isUnspecified())
        return "unspecified";
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(mode == SHORT ? 4 : 16) << value;
    return stream.str();
}

void doParsimPacking(cCommBuffer *buffer, const Ieee802154Address& address)
{
    buffer->pack(static_cast<int>(address.getMode()));
    buffer->pack(address.getInt());
}

void doParsimUnpacking(cCommBuffer *buffer, Ieee802154Address& address)
{
    int mode;
    uint64_t value;
    buffer->unpack(mode);
    buffer->unpack(value);
    address = Ieee802154Address(static_cast<Ieee802154Address::Mode>(mode), value);
}

} // namespace inet
