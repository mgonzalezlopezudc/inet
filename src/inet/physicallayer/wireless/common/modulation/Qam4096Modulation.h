//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_QAM4096MODULATION_H
#define __INET_QAM4096MODULATION_H

#include "inet/physicallayer/wireless/common/modulation/MqamModulation.h"

namespace inet {
namespace physicallayer {

/**
 * Gray coded rectangular 4096-QAM modulation.
 */
class INET_API Qam4096Modulation : public MqamModulation
{
  public:
    static const Qam4096Modulation singleton;

  public:
    Qam4096Modulation();

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << "Qam4096Modulation"; }
};

} // namespace physicallayer
} // namespace inet

#endif
