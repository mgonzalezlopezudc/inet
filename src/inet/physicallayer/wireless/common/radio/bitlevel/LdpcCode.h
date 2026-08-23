//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_LDPCCODE_H
#define __INET_LDPCCODE_H

#include <cstdint>
#include <vector>

#include "inet/physicallayer/wireless/common/contract/bitlevel/IFecCoder.h"

namespace inet {
namespace physicallayer {

/**
 * A generic quasi-cyclic binary LDPC code.
 *
 * The prototype contains 24 block columns.  A non-negative prototype entry
 * denotes a right-shifted identity circulant; -1 denotes an all-zero block.
 * The information bits occupy the first K positions and the parity bits the
 * remaining positions.  The parity submatrix is factorized once at
 * construction time so systematic encoding does not perform a matrix inverse
 * for every packet.
 */
class INET_API LdpcCode : public IForwardErrorCorrection
{
  public:
    struct Edge {
        int checkNode;
        int variableNode;
    };

  protected:
    int codewordLength;
    int informationLength;
    int expansionFactor;
    int blockRows;
    std::vector<int16_t> shifts;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> checkEdges;
    std::vector<std::vector<int>> variableEdges;
    std::vector<std::vector<uint64_t>> parityInverse;

  protected:
    void buildGraph();
    void buildParitySolver();
    static bool getPackedBit(const std::vector<uint64_t>& row, int bit);
    static void togglePackedRow(std::vector<uint64_t>& destination, const std::vector<uint64_t>& source);
    static int parityOfPackedAnd(const std::vector<uint64_t>& left, const std::vector<uint64_t>& right);

  public:
    LdpcCode(int codewordLength, int informationLength, int expansionFactor, const std::vector<int16_t>& shifts);
    LdpcCode(int codewordLength, int informationLength, int expansionFactor, const std::vector<int>& shifts);

    int getCodewordLength() const { return codewordLength; }
    int getInformationLength() const { return informationLength; }
    int getParityLength() const { return codewordLength - informationLength; }
    int getExpansionFactor() const { return expansionFactor; }
    int getBlockRows() const { return blockRows; }
    static constexpr int getBlockColumns() { return 24; }
    int getShift(int blockRow, int blockColumn) const;
    const std::vector<int16_t>& getShifts() const { return shifts; }

    const std::vector<Edge>& getEdges() const { return edges; }
    const std::vector<int>& getEdgesOfCheck(int checkNode) const;
    const std::vector<int>& getEdgesOfVariable(int variableNode) const;
    const Edge& getEdge(int edge) const;

    BitVector computeSyndrome(const BitVector& codeword) const;
    bool isCodeword(const BitVector& codeword) const;
    BitVector encode(const BitVector& informationBits) const;

    virtual double getCodeRate() const override;
    virtual int getEncodedLength(int decodedLength) const override;
    virtual int getDecodedLength(int encodedLength) const override;
    virtual double computeNetBitErrorRate(double grossBitErrorRate) const override;

    std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
