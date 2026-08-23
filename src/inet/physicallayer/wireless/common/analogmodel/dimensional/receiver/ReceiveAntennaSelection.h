//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_RECEIVEANTENNASELECTION_H
#define __INET_RECEIVEANTENNASELECTION_H

#include <functional>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/** Deterministic validation and lexicographic K-of-N receive-row selection. */
class INET_API ReceiveAntennaSelection final
{
  public:
    using ScoreFunction = std::function<double(const std::vector<int>&)>;

    static std::vector<int> validateRowSet(const std::vector<int>& rows, int numberOfRows);
    static std::vector<std::vector<int>> enumerateSubsets(int numberOfRows, int activeCount);
    static std::vector<int> selectBestSubset(int numberOfRows, int activeCount,
        const ScoreFunction& scoreFunction);
};

} // namespace physicallayer
} // namespace inet

#endif
