//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCoderModule.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

namespace inet {
namespace physicallayer {

Define_Module(LdpcCoderModule);

namespace {

std::vector<int> parseShifts(const char *text)
{
    std::vector<int> result;
    cStringTokenizer tokenizer(text, " ,;\t\r\n");
    while (tokenizer.hasMoreTokens()) {
        const char *token = tokenizer.nextToken();
        char *end = nullptr;
        errno = 0;
        long value = std::strtol(token, &end, 10);
        if (errno == ERANGE || end == token || *end != '\0' || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
            throw cRuntimeError("Invalid LDPC prototype shift '%s'", token);
        result.push_back(static_cast<int>(value));
    }
    return result;
}

} // namespace

void LdpcCoderModule::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        auto shifts = parseShifts(par("prototypeShifts"));
        code = new LdpcCode(par("codewordLength"), par("informationLength"), par("expansionFactor"), shifts);
        const char *algorithmName = par("decodingAlgorithm");
        LdpcDecodingAlgorithm algorithm;
        if (!strcmp(algorithmName, "sumProduct"))
            algorithm = LdpcDecodingAlgorithm::SUM_PRODUCT;
        else if (!strcmp(algorithmName, "normalizedMinSum"))
            algorithm = LdpcDecodingAlgorithm::NORMALIZED_MIN_SUM;
        else
            throw cRuntimeError("Unknown LDPC decoding algorithm '%s'", algorithmName);
        coder = new LdpcCoder(code, algorithm, par("maxIterations"), par("normalizedMinSumFactor"), par("maximumLlr"));
    }
}

LdpcCoderModule::~LdpcCoderModule()
{
    delete coder;
    delete code;
}

} // namespace physicallayer
} // namespace inet
