//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_LDPCCODERMODULE_H
#define __INET_LDPCCODERMODULE_H

#include "inet/common/SimpleModule.h"
#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCoder.h"

namespace inet {
namespace physicallayer {

/** Declarative wrapper for a generic quasi-cyclic LDPC code and coder. */
class INET_API LdpcCoderModule : public SimpleModule, public IFecCoder
{
  protected:
    LdpcCode *code = nullptr;
    LdpcCoder *coder = nullptr;

  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override { throw cRuntimeError("This module doesn't handle messages"); }

  public:
    virtual ~LdpcCoderModule();

    virtual BitVector encode(const BitVector& informationBits) const override { return coder->encode(informationBits); }
    virtual std::pair<BitVector, bool> decode(const BitVector& encodedBits) const override { return coder->decode(encodedBits); }
    virtual FecDecodingResult decodeReliabilities(const BitReliabilityVector& reliabilities) const override { return coder->decodeReliabilities(reliabilities); }
    virtual const LdpcCode *getForwardErrorCorrection() const override { return code; }
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return coder->printToStream(stream, level, evFlags); }
};

} // namespace physicallayer
} // namespace inet

#endif
