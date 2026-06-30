//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/modulation/Qam4096Modulation.h"

namespace inet {
namespace physicallayer {

const Qam4096Modulation Qam4096Modulation::singleton;

Qam4096Modulation::Qam4096Modulation() : MqamModulation(12)
{
}

} // namespace physicallayer
} // namespace inet
