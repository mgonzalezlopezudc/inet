//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ReceiveAntennaSelection.h"

#include <cmath>

namespace inet {
namespace physicallayer {

namespace {

void enumerate(int nextIndex, int numberOfRows, int activeCount, std::vector<int>& current,
    std::vector<std::vector<int>>& result)
{
    if ((int)current.size() == activeCount) {
        result.push_back(current);
        return;
    }
    const int remaining = activeCount - current.size();
    for (int index = nextIndex; index <= numberOfRows - remaining; index++) {
        current.push_back(index);
        enumerate(index + 1, numberOfRows, activeCount, current, result);
        current.pop_back();
    }
}

} // namespace

std::vector<int> ReceiveAntennaSelection::validateRowSet(const std::vector<int>& rows, int numberOfRows)
{
    if (numberOfRows <= 0)
        throw cRuntimeError("Receive antenna selection requires a positive row count, got %d", numberOfRows);
    if (rows.empty())
        throw cRuntimeError("Receive antenna selection requires a nonempty row set");
    int previous = -1;
    for (int row : rows) {
        if (row < 0 || row >= numberOfRows)
            throw cRuntimeError("Receive antenna row %d is outside [0,%d)", row, numberOfRows);
        if (row <= previous)
            throw cRuntimeError("Receive antenna rows must be strictly increasing and unique");
        previous = row;
    }
    return rows;
}

std::vector<std::vector<int>> ReceiveAntennaSelection::enumerateSubsets(int numberOfRows, int activeCount)
{
    if (numberOfRows <= 0 || activeCount <= 0 || activeCount > numberOfRows)
        throw cRuntimeError("Receive antenna subset dimensions must satisfy 0 < K <= N, got K=%d N=%d",
            activeCount, numberOfRows);
    std::vector<std::vector<int>> result;
    std::vector<int> current;
    enumerate(0, numberOfRows, activeCount, current, result);
    return result;
}

std::vector<int> ReceiveAntennaSelection::selectBestSubset(int numberOfRows, int activeCount,
    const ScoreFunction& scoreFunction)
{
    if (!scoreFunction)
        throw cRuntimeError("Receive antenna selection requires a score function");
    const auto subsets = enumerateSubsets(numberOfRows, activeCount);
    std::vector<int> best = subsets.front();
    double bestScore = scoreFunction(best);
    if (!std::isfinite(bestScore))
        throw cRuntimeError("Receive antenna score must be finite");
    for (size_t index = 1; index < subsets.size(); index++) {
        const double score = scoreFunction(subsets[index]);
        if (!std::isfinite(score))
            throw cRuntimeError("Receive antenna score must be finite");
        // Strict comparison preserves the first lexicographically enumerated
        // subset on exact ties.
        if (score > bestScore) {
            bestScore = score;
            best = subsets[index];
        }
    }
    return best;
}

} // namespace physicallayer
} // namespace inet
