// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanFragmenter.h"

namespace inet { namespace lowpan {

std::vector<LowpanFragmenter::Range> LowpanFragmenter::plan(int size, int firstBudget, int subsequentBudget,
        int encodedHeaderLength, int originalHeaderLength)
{
    if (size < 40 || size > 2047 || firstBudget < 0 || subsequentBudget < 0 ||
        encodedHeaderLength < 1 || encodedHeaderLength > 2048 || originalHeaderLength < 0 || originalHeaderLength > size)
        return {};
    if (size - originalHeaderLength + encodedHeaderLength <= firstBudget)
        return {{0, size}};
    // RFC 4944 section 5.3: four/five-octet headers; original coverage in units of eight.
    if (firstBudget < 4 + encodedHeaderLength)
        return {};
    int firstCoverage = originalHeaderLength + firstBudget - 4 - encodedHeaderLength;
    firstCoverage = (firstCoverage / 8) * 8;
    int nextCoverage = ((subsequentBudget - 5) / 8) * 8;
    if (firstCoverage <= 0 || firstCoverage < originalHeaderLength ||
        (nextCoverage <= 0 && size - firstCoverage > subsequentBudget - 5))
        return {};
    std::vector<Range> ranges = {{0, firstCoverage}};
    for (int offset = firstCoverage; offset < size;) {
        int length = size - offset <= subsequentBudget - 5 ? size - offset : nextCoverage;
        ranges.push_back({offset, length});
        offset += length;
    }
    return ranges;
}

} } // namespace inet::lowpan
