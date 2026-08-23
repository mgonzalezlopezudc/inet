//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCode.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace inet {
namespace physicallayer {

LdpcCode::LdpcCode(int codewordLength, int informationLength, int expansionFactor, const std::vector<int16_t>& shifts) :
    codewordLength(codewordLength),
    informationLength(informationLength),
    expansionFactor(expansionFactor),
    blockRows(0),
    shifts(shifts)
{
    if (codewordLength <= 0 || informationLength <= 0 || informationLength >= codewordLength)
        throw cRuntimeError("Invalid LDPC dimensions N=%d K=%d", codewordLength, informationLength);
    if (expansionFactor <= 0 || codewordLength != getBlockColumns() * expansionFactor)
        throw cRuntimeError("LDPC codeword length %d is not 24 times expansion factor %d", codewordLength, expansionFactor);

    int parityLength = codewordLength - informationLength;
    if (parityLength % expansionFactor != 0)
        throw cRuntimeError("LDPC parity length %d is not a multiple of expansion factor %d", parityLength, expansionFactor);
    blockRows = parityLength / expansionFactor;
    if (blockRows <= 0 || blockRows > getBlockColumns())
        throw cRuntimeError("Invalid LDPC prototype row count %d", blockRows);
    if (shifts.size() != static_cast<size_t>(blockRows * getBlockColumns()))
        throw cRuntimeError("LDPC prototype has %zu entries, expected %d", shifts.size(), blockRows * getBlockColumns());
    for (int16_t shift : shifts) {
        if (shift < -1 || shift >= expansionFactor)
            throw cRuntimeError("LDPC circulant shift %d is outside [-1,%d)", static_cast<int>(shift), expansionFactor);
    }

    buildGraph();
    buildParitySolver();
}

LdpcCode::LdpcCode(int codewordLength, int informationLength, int expansionFactor, const std::vector<int>& shifts) :
    LdpcCode(codewordLength, informationLength, expansionFactor,
             [&shifts]() {
                 std::vector<int16_t> result;
                 result.reserve(shifts.size());
                 for (int shift : shifts) {
                     if (shift < std::numeric_limits<int16_t>::min() || shift > std::numeric_limits<int16_t>::max())
                         throw cRuntimeError("LDPC shift %d does not fit in int16_t", shift);
                     result.push_back(static_cast<int16_t>(shift));
                 }
                 return result;
             }())
{
}

void LdpcCode::buildGraph()
{
    int parityLength = getParityLength();
    checkEdges.assign(parityLength, {});
    variableEdges.assign(codewordLength, {});
    edges.clear();

    for (int blockRow = 0; blockRow < blockRows; blockRow++) {
        for (int row = 0; row < expansionFactor; row++) {
            int checkNode = blockRow * expansionFactor + row;
            for (int blockColumn = 0; blockColumn < getBlockColumns(); blockColumn++) {
                int shift = getShift(blockRow, blockColumn);
                if (shift < 0)
                    continue;

                // IEEE Std 802.11-2024 19.3.11.7.4 defines P_i as a
                // right shift of the identity columns by i positions.
                int variableNode = blockColumn * expansionFactor + (row + shift) % expansionFactor;
                int edge = edges.size();
                edges.push_back({checkNode, variableNode});
                checkEdges[checkNode].push_back(edge);
                variableEdges[variableNode].push_back(edge);
            }
        }
    }
}

bool LdpcCode::getPackedBit(const std::vector<uint64_t>& row, int bit)
{
    return ((row[bit / 64] >> (bit % 64)) & 1U) != 0;
}

void LdpcCode::togglePackedRow(std::vector<uint64_t>& destination, const std::vector<uint64_t>& source)
{
    if (destination.size() != source.size())
        throw cRuntimeError("Cannot combine packed LDPC rows of different sizes");
    for (size_t i = 0; i < destination.size(); i++)
        destination[i] ^= source[i];
}

int LdpcCode::parityOfPackedAnd(const std::vector<uint64_t>& left, const std::vector<uint64_t>& right)
{
    if (left.size() != right.size())
        throw cRuntimeError("Cannot combine packed LDPC rows of different sizes");
    unsigned int parity = 0;
    for (size_t i = 0; i < left.size(); i++)
        parity ^= static_cast<unsigned int>(__builtin_popcountll(left[i] & right[i]) & 1U);
    return static_cast<int>(parity);
}

void LdpcCode::buildParitySolver()
{
    int parityLength = getParityLength();
    int wordCount = (parityLength + 63) / 64;
    std::vector<std::vector<uint64_t>> parityMatrix(parityLength, std::vector<uint64_t>(wordCount, 0));
    std::vector<std::vector<uint64_t>> inverse(parityLength, std::vector<uint64_t>(wordCount, 0));

    for (int row = 0; row < parityLength; row++)
        inverse[row][row / 64] |= uint64_t(1) << (row % 64);
    for (const Edge& edge : edges) {
        if (edge.variableNode >= informationLength)
            parityMatrix[edge.checkNode][(edge.variableNode - informationLength) / 64] |= uint64_t(1) << ((edge.variableNode - informationLength) % 64);
    }

    for (int column = 0; column < parityLength; column++) {
        int pivot = column;
        while (pivot < parityLength && !getPackedBit(parityMatrix[pivot], column))
            pivot++;
        if (pivot == parityLength)
            throw cRuntimeError("LDPC parity submatrix is rank deficient at column %d", column);
        if (pivot != column) {
            std::swap(parityMatrix[pivot], parityMatrix[column]);
            std::swap(inverse[pivot], inverse[column]);
        }

        for (int row = 0; row < parityLength; row++) {
            if (row != column && getPackedBit(parityMatrix[row], column)) {
                togglePackedRow(parityMatrix[row], parityMatrix[column]);
                togglePackedRow(inverse[row], inverse[column]);
            }
        }
    }

    parityInverse = std::move(inverse);
}

int LdpcCode::getShift(int blockRow, int blockColumn) const
{
    if (blockRow < 0 || blockRow >= blockRows || blockColumn < 0 || blockColumn >= getBlockColumns())
        throw cRuntimeError("LDPC prototype block index (%d,%d) is out of range", blockRow, blockColumn);
    return shifts[blockRow * getBlockColumns() + blockColumn];
}

const std::vector<int>& LdpcCode::getEdgesOfCheck(int checkNode) const
{
    if (checkNode < 0 || checkNode >= static_cast<int>(checkEdges.size()))
        throw cRuntimeError("LDPC check node %d is out of range", checkNode);
    return checkEdges[checkNode];
}

const std::vector<int>& LdpcCode::getEdgesOfVariable(int variableNode) const
{
    if (variableNode < 0 || variableNode >= static_cast<int>(variableEdges.size()))
        throw cRuntimeError("LDPC variable node %d is out of range", variableNode);
    return variableEdges[variableNode];
}

const LdpcCode::Edge& LdpcCode::getEdge(int edge) const
{
    if (edge < 0 || edge >= static_cast<int>(edges.size()))
        throw cRuntimeError("LDPC edge %d is out of range", edge);
    return edges[edge];
}

BitVector LdpcCode::computeSyndrome(const BitVector& codeword) const
{
    if (codeword.getSize() != static_cast<unsigned int>(codewordLength))
        throw cRuntimeError("LDPC codeword has %u bits, expected %d", codeword.getSize(), codewordLength);

    BitVector syndrome;
    for (int checkNode = 0; checkNode < getParityLength(); checkNode++) {
        bool value = false;
        for (int edge : checkEdges[checkNode])
            value = value != codeword.getBit(edges[edge].variableNode);
        syndrome.appendBit(value);
    }
    return syndrome;
}

bool LdpcCode::isCodeword(const BitVector& codeword) const
{
    if (codeword.getSize() != static_cast<unsigned int>(codewordLength))
        return false;
    BitVector syndrome = computeSyndrome(codeword);
    for (unsigned int i = 0; i < syndrome.getSize(); i++) {
        if (syndrome.getBit(i))
            return false;
    }
    return true;
}

BitVector LdpcCode::encode(const BitVector& informationBits) const
{
    if (informationBits.getSize() != static_cast<unsigned int>(informationLength))
        throw cRuntimeError("LDPC information word has %u bits, expected %d", informationBits.getSize(), informationLength);

    int parityLength = getParityLength();
    int wordCount = (parityLength + 63) / 64;
    std::vector<uint64_t> rhs(wordCount, 0);
    for (const Edge& edge : edges) {
        if (edge.variableNode < informationLength && informationBits.getBit(edge.variableNode))
            rhs[edge.checkNode / 64] ^= uint64_t(1) << (edge.checkNode % 64);
    }

    BitVector codeword;
    for (unsigned int i = 0; i < informationBits.getSize(); i++)
        codeword.appendBit(informationBits.getBit(i));
    for (int parityBit = 0; parityBit < parityLength; parityBit++)
        codeword.appendBit(parityOfPackedAnd(parityInverse[parityBit], rhs));

    if (!isCodeword(codeword))
        throw cRuntimeError("LDPC systematic encoder produced a nonzero syndrome");
    return codeword;
}

double LdpcCode::getCodeRate() const
{
    return static_cast<double>(informationLength) / codewordLength;
}

int LdpcCode::getEncodedLength(int decodedLength) const
{
    if (decodedLength < 0 || decodedLength % informationLength != 0)
        throw cRuntimeError("LDPC decoded length %d is not a multiple of K=%d", decodedLength, informationLength);
    long long encodedLength = static_cast<long long>(decodedLength / informationLength) * codewordLength;
    if (encodedLength > std::numeric_limits<int>::max())
        throw cRuntimeError("LDPC encoded length %lld exceeds int range", encodedLength);
    return static_cast<int>(encodedLength);
}

int LdpcCode::getDecodedLength(int encodedLength) const
{
    if (encodedLength < 0 || encodedLength % codewordLength != 0)
        throw cRuntimeError("LDPC encoded length %d is not a multiple of N=%d", encodedLength, codewordLength);
    long long decodedLength = static_cast<long long>(encodedLength / codewordLength) * informationLength;
    if (decodedLength > std::numeric_limits<int>::max())
        throw cRuntimeError("LDPC decoded length %lld exceeds int range", decodedLength);
    return static_cast<int>(decodedLength);
}

double LdpcCode::computeNetBitErrorRate(double grossBitErrorRate) const
{
    if (!std::isfinite(grossBitErrorRate))
        throw cRuntimeError("LDPC gross bit error rate must be finite");
    throw cRuntimeError("LDPC net bit error rate is decoder- and channel-model dependent; use a reliability-aware decoder or calibrated success model");
}

std::ostream& LdpcCode::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "LdpcCode" << EV_FIELD(codewordLength) << EV_FIELD(informationLength) << EV_FIELD(expansionFactor);
    return stream;
}

} // namespace physicallayer
} // namespace inet
