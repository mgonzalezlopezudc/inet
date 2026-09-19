// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanProtocol.h"
namespace inet { namespace lowpan {
const Protocol lowpanNativeProtocol("lowpanNative", "Native IEEE 802.15.4 LoWPAN", Protocol::LinkLayer);
const Protocol lowpanProtocol("lowpan", "6LoWPAN adaptation", Protocol::LinkLayer);
} } // namespace inet::lowpan
