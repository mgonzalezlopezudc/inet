//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.common.radio.bitlevel;

import inet.common.SimpleModule;
import inet.physicallayer.wireless.common.contract.bitlevel.IFecCoder;

// Generic quasi-cyclic binary LDPC coder. The prototype is listed row by row;
// -1 denotes a zero circulant and other values denote right-shifted identities.
simple LdpcCoder extends SimpleModule like IFecCoder
{
    parameters:
        int codewordLength = default(-1); // Must be configured to a positive value.
        int informationLength = default(-1); // Must be configured to a positive value.
        int expansionFactor = default(-1); // Must be configured to a positive value.
        string prototypeShifts = default(""); // Must contain the complete configured prototype.
        string decodingAlgorithm @enum("sumProduct", "normalizedMinSum") = default("sumProduct");
        int maxIterations = default(20);
        double normalizedMinSumFactor = default(0.75);
        double maximumLlr = default(20);
        @class(LdpcCoderModule);
        @display("i=block/segm");
}
