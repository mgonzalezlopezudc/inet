The safest implementation is incremental: add a standards-correct LDPC core and PPDU planner first, then integrate signaling, capability gating, packet-level error behavior, and finally the currently incomplete bit-level HT/VHT pipeline. Keep BCC as the default throughout.

Recommended initial scope: HT and VHT single-user, no STBC, all existing supported MCS/NSS/bandwidth combinations. Reject VHT MU-LDPC and STBC explicitly until those transmission paths are modeled.

## Implementation plan

1. Establish the conformance baseline

   Use IEEE Std 802.11-2024’s consolidated HT/VHT rules:

   - Capability gating: Clause 10.15, chunk `05228`.
   - HT LDPC: 19.3.11.7, Tables 19-15/19-16, chunks `08127–08140`.
   - HT signaling: Table 19-11, chunk `08095`.
   - VHT SU delta: 21.3.10.5.4, chunk `08609`.
   - VHT signaling: Table 21-12, chunk `08582`.
   - Annex F parity matrices: chunks `11834–11839`.
   - Annex I reference vectors: chunks `11931–11979`.

   Transcribe Annex F matrices directly from the PDF because the extracted tables do not preserve row/column structure reliably.

2. Implement the generic LDPC engine

   Add reusable components under `src/inet/physicallayer/wireless/common/radio/bitlevel/`:

   - `LdpcCode`: compact quasi-cyclic parity-check matrix representation.
   - `LdpcCoder`: systematic encoder and deterministic decoder implementing the existing [IFecCoder contract](/home/user/omnetpp_ws/inet-ieee80211-ldpc/src/inet/physicallayer/wireless/common/contract/bitlevel/IFecCoder.h).
   - `LdpcCoder.ned` and module wrapper for declarative composition.

   Support codeword lengths 648, 1296, 1944 and rates 1/2, 2/3, 3/4, 5/6. Provide both full sum-product decoding and deterministic layered normalized min-sum decoding, with a fixed iteration cap and syndrome-based success result. Sum-product is the fidelity/reference default; normalized min-sum is the configurable performance alternative. Extend the FEC contract with reliability/LLR input in the first slice: punctured bits must enter the decoder as zero-confidence erasures, which the existing hard-bit-only interface cannot represent correctly.

3. Add an IEEE 802.11 LDPC PPDU planner

   Create an IEEE-specific immutable `Ieee80211LdpcEncodingParameters`/planner under the 802.11 PHY subtree. It should be the single implementation of:

   - `Npld`, `Navbits`, `NCW`, and `LLDPC`.
   - Shortening `Nshrt`.
   - Puncturing `Npunc` and the extra-symbol threshold.
   - Repetition `Nrep`.
   - Final `NSYM` and transmitted codeword layout.
   - HT versus VHT-SU padding rules.

   LDPC has no BCC tail bits. Serialize codewords sequentially. HT uses its stream-parser rules with one LDPC encoder and bypasses frequency interleaving. VHT uses its own stream parser, segment parser for 160/80+80 MHz, and LDPC tone mapper; do not treat these as a generic reuse of the HT/BCC implementation.

4. Make coding part of the authoritative PHY mode

   Generalize [Ieee80211HtCode.h](/home/user/omnetpp_ws/inet-ieee80211-ldpc/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtCode.h) and [Ieee80211VhtCode.h](/home/user/omnetpp_ws/inet-ieee80211-ldpc/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtCode.h):

   - Replace convolutional-only members with a generic FEC descriptor plus explicit `BCC`/`LDPC` coding kind.
   - Include coding kind in HT/VHT mode cache identity.
   - Replace unconditional `numberOfBccEncoders` and six-bit-tail calculations with coding-specific logic.
   - Expose the selected FEC through `IIeee80211DataMode`.
   - Ensure same-bitrate BCC and LDPC modes cannot collide in `Ieee80211ModeSet`.

   Preserve existing BCC modes and selection as the default.

5. Model the on-air signaling

   Extend [Ieee80211PhyHeader.msg](/home/user/omnetpp_ws/inet-ieee80211-ldpc/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg) and its serializer/printer/dissector:

   - HT-SIG FEC Coding bit: `0=BCC`, `1=LDPC`.
   - VHT-SIG-A coding bit.
   - VHT LDPC Extra OFDM Symbol bit.
   - Receiver validation that the signaled coding agrees with the resolved mode and PPDU plan.

   These are transmitted facts and must be typed frame content, not packet tags. Regenerate `.msg` outputs; never edit generated files manually.

6. Implement capability negotiation and selection gating

   Extend [Ieee80211MgmtFrame.msg](/home/user/omnetpp_ws/inet-ieee80211-ldpc/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame.msg) and management state with:

   - HT LDPC Coding Capability.
   - VHT Rx LDPC capability.
   - Local implemented/activated configuration, defaulting to false.
   - Peer capability storage owned by management.

   Rate selection may choose LDPC only when the local option is active and every intended receiver advertises support. Preserve BCC for incapable peers; an explicitly forced illegal LDPC request should fail clearly. Include the standard’s control-response and TXOP-initiating-control-frame restrictions.

7. Integrate both PHY fidelity paths

   For the default packet-level radio:

   - Remove convolutional-only downcasts from [Ieee80211NistErrorModel.cc](/home/user/omnetpp_ws/inet-ieee80211-ldpc/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.cc) and Yans.
   - Keep their BCC behavior unchanged.
   - Add a replaceable LDPC success model backed by validated PER/BLER curves. Interpolate linearly in `log10(PER)` versus SNR in dB, which is equivalent to log-log interpolation of PER versus linear SNR. Do not reuse BCC equations for LDPC; if no applicable LDPC curve is configured, fail explicitly. The current BER-table model is not an HT/VHT fallback because it rejects those modes and expects a bit model that the packet-level transmitter does not produce.

   For exact bit-domain coding:

   - Build a real HT/VHT data pipeline around the new PPDU planner and coder.
   - Do not patch the legacy-only layered OFDM path with more casts; it is currently marked broken and assumes `Ieee80211OfdmMode`.
   - Keep PHY signal fields BCC-coded where required while applying LDPC only to the Data field.

8. Add focused verification

   Add deterministic tests for:

   - Every Annex F length/rate combination: exact encoding, `Hcᵀ=0`, and direct decoder tests for both sum-product and normalized min-sum, including noiseless recovery, deterministic noisy correction, malformed input, and non-convergence.
   - Receive-side reconstruction and decoding of the Annex I examples covering shortening/puncturing and shortening/repetition.
   - Table 19-16 boundary values around 648, 1296, 1944, and 2592 bits.
   - HT/VHT duration, padding, absence of LDPC tail bits, and extra-symbol behavior.
   - Frequency-interleaver bypass, spatial parsing, and VHT tone mapping.
   - Header serialization and coding-bit interpretation.
   - Local/peer capability combinations and unchanged BCC behavior.
   - Packet-level LDPC error-model monotonicity and a minimal end-to-end ACK/retry scenario.

   Reuse [Ieee80211BitDomain.test](/home/user/omnetpp_ws/inet-ieee80211-ldpc/tests/module/Ieee80211BitDomain.test) as the reference-vector test pattern, but eliminate its time-based randomness.

## Completion gates

- Build and test only in debug mode with focused filters.
- Run architecture checks on the affected wireless/802.11 subtrees.
- Perform both general and WLAN semantic checklist reviews.
- Run only directly mapped unit, module, and fingerprint cases.
- Diagnose the first fingerprint divergence before considering any baseline change; updating fingerprint CSVs requires separate approval.

All likely targets are currently unsealed. The recursively sealed `src/inet/common/packet/` subtree does not need modification. No production source files were changed while preparing this plan.

## Critical design and pseudo-code

The following pseudo-code is intentionally close to the proposed C++ boundaries. Names may be adjusted during implementation, but the ownership and invariants should remain stable.

### 1. Generalize the FEC contract before writing the LDPC decoder

The current `IFecCoder::decode(BitVector)` cannot distinguish a received zero from a punctured bit. Preserve that method for existing coders, but add a reliability-aware operation. Use one documented LLR sign convention everywhere; the examples below use positive for bit 0 and negative for bit 1.

```cpp
using BitReliabilityVector = std::vector<double>;

struct FecDecodingResult {
    BitVector informationBits;
    bool converged = false;
    int iterations = 0;
};

class IFecCoder {
  public:
    virtual BitVector encode(const BitVector& informationBits) const = 0;
    virtual BitVector decode(const BitVector& encodedBits) const = 0;

    // Default compatibility implementation for hard-decision coders.
    virtual FecDecodingResult decodeReliabilities(
            const BitReliabilityVector& llrs) const
    {
        return {decode(hardDecision(llrs)), true, 0};
    }
};

BitVector LdpcCoder::decode(const BitVector& encodedBits) const
{
    BitReliabilityVector llrs(encodedBits.size());
    for (int i = 0; i < encodedBits.size(); i++)
        llrs[i] = encodedBits[i] ? -HARD_DECISION_LLR : HARD_DECISION_LLR;
    auto result = decodeReliabilities(llrs);
    if (!result.converged)
        throw LdpcDecodingFailure();
    return result.informationBits;
}
```

For the LDPC override:

- received bits get finite positive/negative LLRs;
- shortened zero bits get `+MAX_LLR`;
- punctured positions get `0.0`;
- repeated observations are combined by LLR addition.

Do not expose decoder-internal mutable state through the generic contract. Each call must be deterministic and reentrant.

### 2. Represent the Annex F quasi-cyclic codes compactly

Store the base-matrix shift values, not a dense parity-check matrix. A shift of `-1` represents an all-zero circulant; a non-negative shift represents a cyclically shifted identity matrix.

```cpp
enum class Ieee80211LdpcRate { R1_2, R2_3, R3_4, R5_6 };

struct QcLdpcPrototype {
    int codewordLength;       // N = 648, 1296, or 1944
    int informationLength;    // K = N * rate
    int expansionFactor;      // Z = N / 24
    int blockRows;            // 24 - K / Z
    static constexpr int blockColumns = 24;
    std::vector<int16_t> shifts; // row-major, -1 or [0, Z)
};

SparseParityCheckGraph expand(const QcLdpcPrototype& p)
{
    SparseParityCheckGraph graph(p.codewordLength - p.informationLength,
                                 p.codewordLength);

    for (int blockRow = 0; blockRow < p.blockRows; blockRow++) {
        for (int blockColumn = 0; blockColumn < p.blockColumns; blockColumn++) {
            int shift = p.shift(blockRow, blockColumn);
            if (shift < 0)
                continue;

            for (int row = 0; row < p.expansionFactor; row++) {
                int check = blockRow * p.expansionFactor + row;
                int variable = blockColumn * p.expansionFactor
                             + (row + shift) % p.expansionFactor;
                graph.addEdge(check, variable);
            }
        }
    }
    graph.sortEdgesByCheckThenVariable();
    return graph;
}
```

Construction-time validation must reject a table unless:

```cpp
require(N == 24 * Z);
require(K == exactRateNumerator * N / exactRateDenominator);
require(shifts.size() == blockRows * 24);
require(each shift == -1 || (0 <= shift && shift < Z));
require(rank(paritySubmatrix(H)) == N - K);
```

Transcribe and review every Annex F base matrix against the standard. Tests should verify exact entries or a stable table digest in addition to dimensions.

### 3. Encode systematically over GF(2)

Partition `H` as `[A | B]`, where information bits are `u` and parity bits are `p`. Solve `B p = A u` over GF(2). Precompute the packed elimination/factorization for each of the 12 immutable code variants; do not perform a general matrix inversion for every packet.

```cpp
BitVector LdpcCoder::encode(const BitVector& informationBits) const
{
    require(informationBits.size() == code.K());

    BitVector rhs(code.N() - code.K(), false);
    for (int check = 0; check < code.M(); check++) {
        bool value = false;
        for (int variable : code.informationNeighbors(check))
            value ^= informationBits[variable];
        rhs[check] = value;
    }

    BitVector parity = code.paritySolver().solve(rhs); // packed GF(2)
    BitVector codeword = concatenate(informationBits, parity);

    require(codeword.size() == code.N());
    require(code.computeSyndrome(codeword).isAllZero());
    return codeword;
}
```

The parity solver should have a slow, obviously correct reference implementation available to unit tests. Compare optimized and reference encoders for deterministic payloads across all 12 variants.

### 4. Decode with selectable sum-product and normalized min-sum

Use the same fixed check-node order, fixed edge order, layered schedule, iteration limit, LLR convention, and finite clamping for both algorithms. This isolates the check-node rule as the only algorithmic difference and keeps both paths deterministic.

```cpp
enum class LdpcDecodingAlgorithm {
    SUM_PRODUCT,
    NORMALIZED_MIN_SUM
};

class LdpcCoder : public IFecCoder {
    LdpcDecodingAlgorithm decodingAlgorithm = SUM_PRODUCT;
    int maxIterations;
    double normalizedMinSumFactor; // used only by NORMALIZED_MIN_SUM
    double maximumLlr;
};
```

Expose the same choices through the `LdpcCoder.ned` wrapper with `sumProduct` as the fidelity/reference default. Validate `maxIterations > 0`, `maximumLlr > 0`, and `0 < normalizedMinSumFactor <= 1` during initialization even when the latter is inactive, so changing algorithms cannot expose a latent invalid configuration.

The following decoder skeleton shows the normalized min-sum check-node branch. A syndrome check after each complete layered iteration is the only success criterion.

```cpp
FecDecodingResult LdpcCoder::decodeReliabilities(
        const BitReliabilityVector& channelLlrs) const
{
    require(channelLlrs.size() == code.N());
    require(all channelLlrs are finite); // shortening already uses finite +MAX_LLR

    vector<double> posterior = clamp(channelLlrs, -maximumLlr, maximumLlr);
    vector<double> checkToVariable(code.numberOfEdges(), 0.0);

    for (int iteration = 1; iteration <= maxIterations; iteration++) {
        for (int check = 0; check < code.M(); check++) {
            double minimum1 = INF;
            double minimum2 = INF;
            int minimumEdge = -1;
            int totalSign = +1;

            // Remove the old message and collect the two smallest magnitudes.
            for (int edge : code.edgesOfCheck(check)) {
                int variable = code.variableOf(edge);
                double extrinsic = posterior[variable] - checkToVariable[edge];
                temporary[edge] = extrinsic;
                totalSign *= signNonZero(extrinsic);
                updateTwoMinima(abs(extrinsic), edge,
                                minimum1, minimum2, minimumEdge);
            }

            // Immediately update posterior values: layered scheduling.
            for (int edge : code.edgesOfCheck(check)) {
                int variable = code.variableOf(edge);
                double magnitude = edge == minimumEdge ? minimum2 : minimum1;
                int sign = totalSign * signNonZero(temporary[edge]);
                double message = normalizedMinSumFactor * sign * magnitude;
                message = clamp(message, -maximumLlr, maximumLlr);
                checkToVariable[edge] = message;
                posterior[variable] = clamp(temporary[edge] + message,
                                            -maximumLlr, maximumLlr);
            }
        }

        BitVector decision = hardDecision(posterior);
        if (code.computeSyndrome(decision).isAllZero())
            return {decision.prefix(code.K()), true, iteration};
    }

    return {hardDecision(posterior).prefix(code.K()), false, maxIterations};
}
```

Never return an unqualified payload on non-convergence. The caller must receive and honor `converged=false`.

For full sum-product, replace the two-minimum approximation inside each check update with the exact belief-propagation check-node operation:

```text
L(c -> v) = 2 * atanh(product over v' != v of tanh(L(v' -> c) / 2))
```

Implement it without division and without directly multiplying signed values. Prefix/suffix products preserve exact erasure behavior and avoid division by zero:

```cpp
void LdpcCoder::updateSumProductCheck(
        int check,
        vector<double>& posterior,
        vector<double>& checkToVariable) const
{
    auto edges = code.edgesOfCheck(check); // stable stored order
    int degree = edges.size();
    vector<double> extrinsic(degree);
    vector<double> tanhMagnitude(degree);
    vector<int> signs(degree);
    vector<double> prefixProduct(degree + 1, 1.0);
    vector<double> suffixProduct(degree + 1, 1.0);
    int totalSign = +1;

    for (int i = 0; i < degree; i++) {
        int edge = edges[i];
        int variable = code.variableOf(edge);
        extrinsic[i] = clamp(posterior[variable] - checkToVariable[edge],
                             -maximumLlr, maximumLlr);
        signs[i] = std::signbit(extrinsic[i]) ? -1 : +1; // zero is positive
        totalSign *= signs[i];
        tanhMagnitude[i] = tanh(0.5 * abs(extrinsic[i]));
        prefixProduct[i + 1] = prefixProduct[i] * tanhMagnitude[i];
    }

    for (int i = degree - 1; i >= 0; i--)
        suffixProduct[i] = suffixProduct[i + 1] * tanhMagnitude[i];

    for (int i = 0; i < degree; i++) {
        int edge = edges[i];
        int variable = code.variableOf(edge);
        double productExceptI = prefixProduct[i] * suffixProduct[i + 1];
        productExceptI = clamp(productExceptI, 0.0, nextBelowOne());

        // Numerically stable 2*atanh(x).
        double magnitude = log1p(productExceptI) - log1p(-productExceptI);
        double message = totalSign * signs[i] * magnitude;
        checkToVariable[edge] = clamp(message, -maximumLlr, maximumLlr);
        posterior[variable] = clamp(extrinsic[i] + checkToVariable[edge],
                                    -maximumLlr, maximumLlr);
    }
}
```

An input LLR of zero therefore remains a genuine erasure: it contributes a zero `tanhMagnitude` rather than an invented hard decision. Underflow of a product to zero also yields a neutral outgoing message. Clamp only the final `atanh` argument and LLRs; never replace zero inputs with an epsilon.

Full sum-product performs the exact check-node marginal update used by belief propagation, whereas normalized min-sum approximates its magnitude with scaled minima. Because IEEE 802.11 LDPC Tanner graphs contain cycles, describe sum-product as the optimum check-node update or full SPA—not as a maximum-likelihood decoder.

### 5. Make the PPDU planner deterministic on both sides of the air boundary

Use exact integer/rational arithmetic for code rates and thresholds. Floating-point equality near Table 19-16 boundaries is not acceptable.

```cpp
enum class Ieee80211FecType { BCC, LDPC };

struct Ieee80211LdpcCodewordPlan {
    int codewordLength;          // L_LDPC
    int informationLength;       // K
    int shortenedBits;           // per-codeword Nshrt distribution
    int puncturedBits;           // per-codeword Npunc distribution
    int repeatedBits;             // per-codeword Nrep distribution
    int transmittedBits;
};

struct Ieee80211DataEncodingPlan {
    Ieee80211FecType fecType;
    int uncodedDataBits;          // Npld; includes format-specific SERVICE/pad
    int availableEncodedBits;     // Navbits
    int numberOfSymbols;          // NSYM
    bool additionalSymbolApplied; // mapped to VHT signaling only for VHT SU
    std::vector<Ieee80211LdpcCodewordPlan> codewords;
};
```

First compute format-specific initial values:

```cpp
int ceilDiv(int numerator, int denominator)
{
    return (numerator + denominator - 1) / denominator;
}

InitialLdpcValues computeHtInitialValues(int psduOctets, const HtMode& mode)
{
    auto [p, q] = mode.codeRate();
    int npld = 8 * psduOctets + 16; // SERVICE; no BCC tail
    int navbits = mode.ncbps() * mode.stbcSymbolFactor()
                * ceilDiv(npld * q,
                          mode.ncbps() * p * mode.stbcSymbolFactor());
    return {npld, navbits, navbits / mode.ncbps()};
}

InitialLdpcValues computeVhtSuInitialValues(int apepOctets, const VhtMode& mode)
{
    int nsymInit = mode.stbcSymbolFactor()
                 * ceilDiv(8 * apepOctets + 16,
                           mode.stbcSymbolFactor() * mode.ndbps());
    return {nsymInit * mode.ndbps(),
            nsymInit * mode.ncbps(),
            nsymInit};
}
```

Then apply Table 19-16. The threshold expressions below must be compared by cross multiplication with the exact rate fraction.

```cpp
CodeSelection selectCodewords(int npld, int navbits, ExactRate rate)
{
    auto [p, q] = rate;
    if (navbits <= 648)
        return navbits * q >= npld * q + 912 * (q - p)
             ? CodeSelection{1, 1296} : CodeSelection{1, 648};

    if (navbits <= 1296)
        return navbits * q >= npld * q + 1464 * (q - p)
             ? CodeSelection{1, 1944} : CodeSelection{1, 1296};

    if (navbits <= 1944)
        return {1, 1944};

    if (navbits <= 2592)
        return navbits * q >= npld * q + 2916 * (q - p)
             ? CodeSelection{2, 1944} : CodeSelection{2, 1296};

    int ncw = ceilDiv(npld * q, 1944 * p);
    return {ncw, 1944};
}
```

Compute shortening and puncturing, then apply the single allowed extra-symbol adjustment:

```cpp
Ieee80211DataEncodingPlan planLdpc(initial, mode)
{
    auto [ncw, lldpc] = selectCodewords(initial.npld,
                                        initial.navbits,
                                        mode.codeRate());
    ExactRate rate = mode.codeRate();
    int informationCapacity = ncw * multiplyExact(lldpc, rate);

    int nshrt = max(0, informationCapacity - initial.npld);
    int npunc = max(0, ncw * lldpc - initial.navbits - nshrt);
    int navbits = initial.navbits;
    bool additionalSymbolApplied = false;

    auto [p, q] = rate;
    int64_t parityUnits = int64_t(ncw) * lldpc * (q - p);
    bool needsExtraSymbol =
        ((int64_t(10) * q * npunc > parityUnits) &&
         (int64_t(5) * (q - p) * nshrt < int64_t(6) * p * npunc)) ||
        (int64_t(10) * q * npunc > int64_t(3) * parityUnits);

    if (needsExtraSymbol) {
        navbits += mode.ncbps() * mode.stbcSymbolFactor();
        npunc = max(0, ncw * lldpc - navbits - nshrt);
        additionalSymbolApplied = true;
        // Do not rerun codeword selection or recompute Nshrt.
    }

    int nrep = max(0,
                   navbits - ncw * multiplyExact(lldpc, oneMinus(rate))
                           - initial.npld);

    auto codewords = distributeAcrossCodewords(
        ncw, lldpc, rate, nshrt, npunc, nrep);

    require(!(npunc > 0 && nrep > 0));
    require(sum(codewords.transmittedBits) == navbits);
    require(navbits % mode.ncbps() == 0);

    return {LDPC, initial.npld, navbits,
            navbits / mode.ncbps(), additionalSymbolApplied, codewords};
}
```

The exact integer predicate above is equivalent to the standard's extra-symbol conditions:

```text
extra symbol is needed if

  (Npunc > 0.1 * NCW * LLDPC * (1-R)
   and Nshrt < 1.2 * Npunc * R / (1-R))

  or

  Npunc > 0.3 * NCW * LLDPC * (1-R)
```

Use strict comparisons exactly as specified. The `q` factors in the pseudo-code are required to put all terms over a common denominator; use a wide integer type.

Distribute each total with quotient/remainder arithmetic in the standard-defined codeword order:

```cpp
vector<int> distribute(int total, int ncw)
{
    vector<int> result(ncw, total / ncw);
    for (int i = 0; i < total % ncw; i++)
        result[i]++;
    return result;
}
```

The production implementation must follow the exact per-codeword equations in Clause 19 rather than assuming that the same remainder order applies independently to every quantity; the simple helper above illustrates the required determinism.

### 6. Map shortened, punctured, and repeated bits exactly

Keep codeword algebra independent of IEEE 802.11 PPDU mapping. For codeword `i`, consume the planned information bits, append known zeros for shortening, encode a full codeword, omit the shortened systematic positions, omit punctured parity positions, and then repeat from the defined start of the shortened codeword when required.

```cpp
BitVector encodeLdpcData(const BitVector& scrambledData,
                         const Ieee80211DataEncodingPlan& plan)
{
    BitReader data(scrambledData);
    BitVector transmitted;

    for (const auto& cw : plan.codewords) {
        int k = cw.informationLength;
        int dataCount = k - cw.shortenedBits;

        BitVector information = data.read(dataCount);
        information.appendZeros(cw.shortenedBits);
        BitVector full = coder(cw.codewordLength).encode(information);

        // Shortened information positions are not transmitted.
        BitVector shortenedCodeword = full.slice(0, dataCount)
                                    + full.slice(k, full.size());

        // Puncturing removes the planned parity positions at the end.
        BitVector base = shortenedCodeword.prefix(
            shortenedCodeword.size() - cw.puncturedBits);
        transmitted.append(base);

        // Repetition cycles through the shortened codeword in specified order.
        for (int j = 0; j < cw.repeatedBits; j++)
            transmitted.appendBit(shortenedCodeword[j % shortenedCodeword.size()]);
    }

    require(data.atEnd());
    require(transmitted.size() == plan.availableEncodedBits);
    return transmitted;
}
```

The receive path reconstructs reliability vectors, not invented hard bits:

```cpp
BitVector decodeLdpcData(const BitReliabilityVector& received,
                         const Ieee80211DataEncodingPlan& plan)
{
    ReliabilityReader input(received);
    BitVector decodedData;

    for (const auto& cw : plan.codewords) {
        int n = cw.codewordLength;
        int k = cw.informationLength;
        int dataCount = k - cw.shortenedBits;
        BitReliabilityVector llr(n, 0.0); // punctures remain erasures

        input.readInto(llr, 0, dataCount);
        fill(llr.begin() + dataCount, llr.begin() + k, +MAX_LLR);

        int transmittedParity = n - k - cw.puncturedBits;
        input.readInto(llr, k, transmittedParity);

        auto repeatTargets = shortenedCodewordPositions(dataCount, k, n);
        for (int j = 0; j < cw.repeatedBits; j++)
            llr[repeatTargets[j % repeatTargets.size()]] += input.read();

        auto result = coder(n).decodeReliabilities(llr);
        if (!result.converged)
            throw LdpcDecodingFailure();
        decodedData.append(result.informationBits.prefix(dataCount));
    }

    require(input.atEnd());
    return decodedData.prefix(plan.uncodedDataBits);
}
```

After codeword concatenation, HT uses the HT stream parser with `NES=1` and bypasses frequency interleaving. VHT uses the VHT stream parser, the segment parser for 160/80+80 MHz, and the VHT LDPC tone mapper. Each stage needs a fixture checkpoint.

### 7. Put coding identity and duration in the mode contract

The HT/VHT data mode already owns timing, so make it compute the canonical per-PSDU plan. Compatibility length and duration methods must delegate to it.

```cpp
class IIeee80211DataMode {
  public:
    virtual Ieee80211FecType getFecType() const = 0;
    virtual ExactRate getCodeRate() const = 0;
    virtual int getNumberOfCodedBitsPerSymbol() const = 0;
    virtual int getNumberOfDataBitsPerSymbol() const = 0;
    virtual Ieee80211DataEncodingPlan computeEncodingPlan(b psduLength) const = 0;
};

simtime_t Ieee80211HtDataMode::getDuration(b psduLength) const
{
    auto plan = computeEncodingPlan(psduLength);
    return plan.numberOfSymbols * symbolInterval;
}
```

Mode identity and lookup must include FEC:

```cpp
struct HtModeKey {
    Hz bandwidth;
    int mcsIndex;
    GuardIntervalType guardInterval;
    Ieee80211FecType fecType;
};

const IIeee80211Mode *findMode(bps bitrate, Hz bandwidth, int nss,
    optional<Ieee80211FecType> fecType = nullopt)
{
    // Compatibility lookup is deliberately BCC, never container-order dependent.
    Ieee80211FecType required = fecType.value_or(Ieee80211FecType::BCC);
    return findUniqueMatch(bitrate, bandwidth, nss, required);
}
```

Keep the encoding plan immutable while it is used inside one local PHY pipeline, but do not store it in `Ieee80211Transmission` and do not carry a sender plan/tag across the medium. On transmit, header creation, duration, and bit generation consume one locally computed plan; the transmitter removes its internal plan tag before constructing the immutable transmission. On receive, reconstruct an independent plan from received HT/VHT fields and the received Data-field duration. HT uses its exact HT-SIG length. VHT-SU LDPC uses `N_SYM`, the LDPC Extra OFDM Symbol bit, resolved mode geometry, and the VHT-SIG-B rounded length only for consistency checks. The receiver must not use packet length or any exact sender APEP metadata as a planning oracle.

### 8. Build headers from the local transmit plan and reconstruct at receive

Before LDPC signaling can work, complete the currently empty HT/VHT PHY header message classes and serializers, and fix the VHT header factory so it creates `Ieee80211VhtPhyHeader` rather than `Ieee80211HtPhyHeader`.

```cpp
auto mode = transmitter->computeTransmissionMode(packet);
auto plan = mode->getDataMode()->computeEncodingPlan(packet->getDataLength());

auto header = mode->getHeaderMode()->createHeader();
header->setPsduLength(B(packet->getDataLength()));

if (auto ht = dynamicPtrCast<Ieee80211HtPhyHeader>(header)) {
    ht->setFecCoding(plan.fecType == Ieee80211FecType::LDPC); // HT-SIG2 B6
}
else if (auto vht = dynamicPtrCast<Ieee80211VhtPhyHeader>(header)) {
    vht->setCoding(plan.fecType == Ieee80211FecType::LDPC);  // VHT-SIG-A2 B2
    vht->setLdpcExtraOfdmSymbol(                              // VHT-SIG-A2 B3
        plan.fecType == Ieee80211FecType::LDPC &&
        plan.additionalSymbolApplied);
}

packet->insertAtFront(header);
packet->removeTagIfPresent<Ieee80211DataEncodingPlanTag>();
return createTransmission(packet, mode, mode->getDuration(plan));
```

Here `mode->getDuration(plan)` composes the preamble/header duration with the data duration derived from planned `NSYM`; it must not redo the LDPC arithmetic. The serializer must emit the standard-defined bit positions, reserved values, CRC, and tail. Deserialization must reject invalid reserved values. The receiver then resolves the mode from typed PHY fields and reconstructs its own plan from those fields plus Data duration. For VHT-SU LDPC, the MAC constructs the complete padded A-MPDU PSDU; VHT-SIG-B carries only `ceil(APEP_LENGTH/4)`, and the decoded A-MPDU delimiter supplies the exact MPDU length. Any receiver-local plan or rounded APEP indication tag must be consumed before the packet leaves PHY/MAC processing.

Centralize duration-to-symbol conversion and HT/VHT-SIG consistency checks in one packet-level receiver helper. Ordinary reception, exact soft decoding, and packet-level PER evaluation must all call this function; none may duplicate OFDM-symbol rounding or reconstruct a subtly different plan:

```cpp
Ieee80211DataEncodingPlan reconstructIeee80211ReceivedDataEncodingPlan(
        const IIeee80211DataMode *dataMode,
        const Ptr<const Ieee80211PhyHeader>& phyHeader,
        simtime_t dataDuration)
{
    require(dataMode != nullptr && phyHeader != nullptr);
    require(dataDuration.raw() > 0);
    require(dataDuration.raw() % dataMode->getSymbolInterval().raw() == 0);
    int receivedNsym = exactQuotient(dataDuration,
                                     dataMode->getSymbolInterval());

    if (auto ht = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader)) {
        require(dataMode->getPhyFormat() == HT);
        require(ht->getFecCoding() == (dataMode->getFecType() == LDPC));
        plan = dataMode->computeEncodingPlan(ht->getLengthField());
    }
    else if (auto vht = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
        require(dataMode->getPhyFormat() == VHT_SU);
        require(vht->getCoding() == (dataMode->getFecType() == LDPC));
        if (dataMode->getFecType() == LDPC) {
            require(receivedNsym > int(vht->getLdpcExtraOfdmSymbol()));
            int initialNsym = receivedNsym -
                    int(vht->getLdpcExtraOfdmSymbol());
            plan = Ieee80211LdpcPlanner::computeVhtSuFromReceivedSymbols(
                    initialNsym, dataMode->getNumberOfCodedBitsPerSymbol(),
                    dataMode->getNumberOfDataBitsPerSymbol(),
                    dataMode->getCodeRate());
            int completePsduOctets = (plan.getUncodedDataBits() - 16) / 8;
            require(vht->getVhtSigBLength() != 0);
            require(decodeVhtSuSigBLength(vht->getVhtSigBLength()) <=
                    B(completePsduOctets));
            require(vht->getShortGiNsymDisambiguation() ==
                    (vht->getShortGi() && receivedNsym % 10 == 9));
            require(vht->getLdpcExtraOfdmSymbol() ==
                    plan.getAdditionalCapacityApplied());
        }
        else
            plan = dataMode->computeEncodingPlan(vht->getLengthField());
    }
    else
        rejectUnsupportedHeader();

    require(plan.getPhyFormat() == dataMode->getPhyFormat());
    require(plan.getFecType() == dataMode->getFecType());
    require(plan.getNumberOfSymbols() == receivedNsym);
    return plan;
}
```

Add parity tests at this shared boundary for valid HT/VHT plans and for malformed non-integral duration, VHT-SIG-B Length, and Extra OFDM Symbol cases. The exact receiver may additionally validate MCS, NSS, bandwidth, reserved fields, and mapped-symbol count, but it must not reimplement the timing calculation.

### 9. Gate LDPC through typed peer capabilities

Adding bits to the management message is only the wire-format portion. Also add AP/STA frame construction, serialization, learned peer state, and a typed query boundary used by rate selection.

```cpp
class IIeee80211PeerCapabilities {
  public:
    virtual bool supportsHtLdpcRx(const MacAddress& peer) const = 0;
    virtual bool supportsVhtLdpcRx(const MacAddress& peer) const = 0;
};

LdpcSelection selectDataFec(const Frame& frame,
                           const vector<MacAddress>& intendedReceivers,
                           const optional<RxVector>& solicitingRxVector)
{
    bool localReady = ldpcImplemented && ldpcActivated;
    bool allReceiversReady = !intendedReceivers.empty()
        && allOf(intendedReceivers, [&](const auto& receiver) {
               return peerCapabilities->supportsLdpcRx(receiver);
           });

    bool controlPermitsLdpc =
        !(frame.isControl() && frame.initiatesTxop()) &&
        !(frame.isControlResponse() &&
          (!solicitingRxVector || solicitingRxVector->fecType != LDPC));

    if (forcedLdpc && !(localReady && allReceiversReady && controlPermitsLdpc))
        throw cRuntimeError("LDPC requested without local/peer capability");

    if (!(localReady && allReceiversReady && controlPermitsLdpc))
        return {BCC, ALLOWED};

    bool noLdpcPreferred = anyOf(intendedReceivers, [&](const auto& receiver) {
        return peerCapabilities->latestOperatingMode(receiver).noLdpc;
    });
    if (noLdpcPreferred && !forcedLdpc)
        return {BCC, RECOMMENDED};
    return {LDPC, noLdpcPreferred ? DISCOURAGED : ALLOWED};
}
```

Unknown group membership must use the conservative BCC path. The dynamic Operating Mode `No LDPC` bit is a normative `SHOULD NOT`, so represent it as a preference/policy input rather than an unconditional prohibition. Encode the Clause 10.15 and 10.6.6 control restrictions as named predicates with direct tests, not as scattered conditionals.

### 10. Isolate packet-level LDPC success modeling

Keep the existing NIST/Yans BCC equations unchanged. Route LDPC data through a paired C++/NED collaborator so error-model policy remains replaceable.

Add the complete implementation locally under the IEEE 802.11 packet-level error-model package:

- `IIeee80211FecSuccessModel.{h,ned}`: replaceable success-model contract.
- `Ieee80211LdpcPerSuccessModel.{h,cc,ned}`: concrete configured LDPC implementation.
- `Ieee80211LdpcPerTable.{h,cc}`: deterministic, immutable CSV value object with no OMNeT++ module behavior.
- A checked-in baseline CSV under the corresponding data/test-data subtree; the NED default points to that file, while simulations may override the path.

```cpp
class IIeee80211FecSuccessModel {
  public:
    virtual double computeDataSuccessRate(
        const IIeee80211DataMode& mode,
        const Ieee80211DataEncodingPlan& plan,
        double snrDb) const = 0;
};

double Ieee80211NistErrorModel::getDataSuccessRate(
        const Ieee80211Transmission& transmission, double snir) const
{
    const auto& dataMode = *transmission.getMode()->getDataMode();
    auto phyHeader = peekIeee80211PhyHeaderAtFront(transmission.getPacket());

    if (dataMode.getFecType() == BCC)
        return computeExistingBccSuccessRate(
            dataMode, phyHeader->getLengthField(), snir);

    if (snir < 0)
        throw cRuntimeError("Negative SNIR is invalid");
    auto plan = reconstructIeee80211ReceivedDataEncodingPlan(
        &dataMode, phyHeader, transmission.getDataDuration());
    double snrDb = snir == 0 ? -INFINITY : 10 * log10(snir);
    return ldpcSuccessModel->computeDataSuccessRate(dataMode, plan, snrDb);
}
```

Validate every CSV row against the exact structural key below. Runtime selection uses a separate calibrated-mode key: format, bandwidth, and per-stream MCS. Under the explicit ideal-separated-stream assumption, NSS does not select another curve; HT MCS 8–31 maps to MCS modulo 8, while VHT uses its received MCS unchanged. Do not perform nearest-neighbor matching or fallback across format, bandwidth, or per-stream MCS. SNR is the ordered coordinate within the selected curve, not part of either key. A missing calibrated-mode curve must produce a diagnostic error, never a BCC approximation.

The table value object owns all file parsing and interpolation. Use ordered standard containers so diagnostics, iteration, and tests are reproducible without relying on hash or input iteration order:

```cpp
enum class Ieee80211PhyFormat {
    HT,
    VHT_SU
};

struct Ieee80211LdpcPerCurveKey {
    Ieee80211PhyFormat phyFormat;
    int bandwidthMhz;
    int mcs;
    int numberOfSpatialStreams;
    int numberOfCodewords;
    int ldpcCodewordLength;
    int shortenedBits;
    int puncturedBits;
    int repeatedBits;

    auto asTuple() const {
        return std::tie(phyFormat, bandwidthMhz, mcs,
                        numberOfSpatialStreams, numberOfCodewords,
                        ldpcCodewordLength, shortenedBits,
                        puncturedBits, repeatedBits);
    }
    bool operator<(const Ieee80211LdpcPerCurveKey& other) const {
        return asTuple() < other.asTuple();
    }
};

struct Ieee80211LdpcPerPoint {
    double snrDb;
    double packetErrorRate;
};

using Ieee80211LdpcPerCurve = std::vector<Ieee80211LdpcPerPoint>;

class Ieee80211LdpcPerTable {
  protected:
    std::map<Ieee80211LdpcPerCurveKey, Ieee80211LdpcPerCurve> curves;
    using ModeKey = std::tuple<Ieee80211PhyFormat, int, int>;
    std::map<ModeKey, Ieee80211LdpcPerCurveKey> modeCurveKeys;

  public:
    void load(const char *tableFile);
    double getPacketErrorRate(const Ieee80211LdpcPerCurveKey& key,
                              double snrDb) const;
};
```

Build the structural validation key only from the resolved data mode and the locally computed or receiver-reconstructed encoding plan:

```cpp
Ieee80211LdpcPerCurveKey makeCurveKey(
        const IIeee80211DataMode& mode,
        const Ieee80211DataEncodingPlan& plan)
{
    require(plan.fecType == Ieee80211FecType::LDPC);
    require(allCodewordsUseSameLength(plan));

    return {
        getPhyFormat(mode),
        checkedIntegerMhz(mode.getBandwidth()),
        mode.getMcsIndex(),
        mode.getNumberOfSpatialStreams(),
        int(plan.codewords.size()),
        plan.codewords.front().codewordLength,
        sum(plan.codewords.shortenedBits),
        sum(plan.codewords.puncturedBits),
        sum(plan.codewords.repeatedBits)
    };
}
```

This key describes the actual encoded Data field. Do not add a separately configured code rate, payload length, or symbol count when it is derivable from the mode and plan. The table validates it, then selects the calibration with `(format, bandwidth, perStreamMcs)`, where `perStreamMcs = mcs % 8` for HT and `perStreamMcs = mcs` for VHT. SNR is the ordered coordinate inside the selected curve, not part of the key.

#### Canonical CSV contract

Use this exact header and column order:

```csv
phy_format,bandwidth_mhz,mcs,nss,number_of_codewords,ldpc_codeword_length,shortened_bits,punctured_bits,repeated_bits,snr_db,per
```

Field semantics are:

| Field | Contract |
|---|---|
| `phy_format` | Exact token `HT` or `VHT_SU`. |
| `bandwidth_mhz` | Positive integer channel width in MHz, legal for the format. |
| `mcs` | Non-negative HT/VHT MCS index, legal with the format and NSS. |
| `nss` | Positive number of spatial streams. |
| `number_of_codewords` | Positive `NCW` from the corresponding planner result. |
| `ldpc_codeword_length` | `LLDPC`, exactly 648, 1296, or 1944. |
| `shortened_bits` | Aggregate `Nshrt`, non-negative. |
| `punctured_bits` | Aggregate `Npunc`, non-negative. |
| `repeated_bits` | Aggregate `Nrep`, non-negative. |
| `snr_db` | Finite SNR sample in dB. |
| `per` | Data-field error probability, conditional on PHY-header success, with `0 < PER <= 1`. |

For example, a two-point synthetic curve for the first Annex I plan is:

```csv
# Produced by: <author/tool and version>
# Calibration: <channel model, decoder settings, trials, seed policy>
# PER estimator: (errors + 0.5) / (packets + 1)
phy_format,bandwidth_mhz,mcs,nss,number_of_codewords,ldpc_codeword_length,shortened_bits,punctured_bits,repeated_bits,snr_db,per
HT,20,4,1,1,1944,642,54,0,0.0,0.1
HT,20,4,1,1,1944,642,54,0,2.0,0.001
```

The CSV grammar is deliberately narrow: UTF-8/ASCII text, optional blank lines and lines beginning with `#`, comma-separated unquoted fields, ASCII whitespace trimmed around fields, and either LF or CRLF endings. Authors may use `#` comments to record provenance, generation commands, channel/calibration assumptions, decoder settings, trial counts, estimators, confidence information, or citations. Comments are opaque human-readable text: the loader ignores them and must not derive runtime semantics from them. A data row must have exactly 11 fields. Reject quoted fields, embedded commas, missing fields, trailing fields, partially parsed numbers, integer overflow, and unknown enum tokens with `file:line:column` diagnostics.

Rows must be in canonical lexicographic key order using `Ieee80211LdpcPerCurveKey::operator<`. Within one key, SNR must be strictly increasing and PER must be non-increasing. A key may occur in only one contiguous group and every curve must contain at least two points. These requirements make the file itself canonical; the in-memory `std::map` and per-key vectors preserve the same order.

#### Loader algorithm

```cpp
void Ieee80211LdpcPerTable::load(const char *tableFile)
{
    ByteVector rawCsv = readAllBytes(tableFile);
    CsvReader reader(rawCsv);
    requireExactHeader(reader.readHeader(), EXPECTED_COLUMNS);

    optional<Ieee80211LdpcPerCurveKey> previousKey;
    optional<Ieee80211LdpcPerPoint> previousPoint;
    map<ModeKey, Ieee80211LdpcPerCurveKey> loadedModeKeys;

    while (reader.hasDataRow()) {
        CsvRow row = reader.readStrictUnquotedRow(11);
        auto key = parseAndValidateCurveKey(row);
        auto point = parseAndValidatePoint(row);
        ModeKey modeKey = makeModeKey(
            key.phyFormat, key.bandwidthMhz,
            key.phyFormat == HT ? key.mcs % 8 : key.mcs);

        if (auto existing = loadedModeKeys.find(modeKey);
            existing != loadedModeKeys.end() && existing->second != key)
            reader.fail("multiple calibration signatures for one calibrated-mode key");
        loadedModeKeys.emplace(modeKey, key);

        if (previousKey) {
            if (key < *previousKey)
                reader.fail("curve keys are not in canonical order");

            if (key.asTuple() == previousKey->asTuple()) {
                if (!(point.snrDb > previousPoint->snrDb))
                    reader.fail("SNR must be strictly increasing");
                if (point.packetErrorRate > previousPoint->packetErrorRate)
                    reader.fail("PER must be non-increasing");
            }
            else {
                requireAtLeastTwoPoints(curves.at(*previousKey));
                previousPoint.reset();
            }
        }

        curves[key].push_back(point);
        previousKey = key;
        previousPoint = point;
    }

    if (previousKey)
        requireAtLeastTwoPoints(curves.at(*previousKey));
    if (curves.empty())
        throw cRuntimeError("LDPC PER table contains no curves");
    modeCurveKeys = std::move(loadedModeKeys);
}
```

`parseAndValidateCurveKey()` must also reject impossible basic combinations: illegal format/bandwidth/MCS/NSS, `NCW <= 0`, unsupported `LLDPC`, negative shortening/puncturing/repetition, simultaneous positive puncturing and repetition, or a structural signature that cannot be produced by the canonical LDPC planner. This final planner cross-check prevents a syntactically valid but unreachable curve from entering the table.

SNR in dB is already an affine base-10 logarithm of linear SNR, so this is log-log interpolation of PER versus linear SNR. Use `log10(PER)` explicitly and invert it with `10^x`:

```cpp
double Ieee80211LdpcPerTable::getPacketErrorRate(
        const Ieee80211LdpcPerCurveKey& key, double snrDb) const
{
    validateKey(key);
    ModeKey modeKey = makeModeKey(
        key.phyFormat, key.bandwidthMhz,
        key.phyFormat == HT ? key.mcs % 8 : key.mcs);
    auto selected = modeCurveKeys.find(modeKey);
    if (selected == modeCurveKeys.end())
        throw cRuntimeError("No LDPC PER curve for calibrated mode %s",
                            formatModeKey(modeKey).c_str());
    return interpolatePacketErrorRate(curves.at(selected->second), snrDb);
}

double interpolatePacketErrorRate(
        const Ieee80211LdpcPerCurve& curve, double snrDb)
{
    require(!std::isnan(snrDb)); // +/- infinity is handled by endpoint clamping
    require(curve.size() >= 2);

    // Clamp outside the measured domain; never extrapolate.
    if (snrDb <= curve.front().snrDb)
        return curve.front().packetErrorRate;
    if (snrDb >= curve.back().snrDb)
        return curve.back().packetErrorRate;

    auto upper = std::lower_bound(
        curve.begin(), curve.end(), snrDb,
        [](const Ieee80211LdpcPerPoint& point, double value) {
            return point.snrDb < value;
        });

    if (upper->snrDb == snrDb)
        return upper->packetErrorRate; // preserve exact samples exactly

    const auto& lower = *(upper - 1);
    double ratio = (snrDb - lower.snrDb) / (upper->snrDb - lower.snrDb);
    double log10Per = log10(lower.packetErrorRate)
                    + ratio * (log10(upper->packetErrorRate)
                             - log10(lower.packetErrorRate));
    return clamp(pow(10.0, log10Per), 0.0, 1.0);
}
```

For a midpoint SNR this produces the geometric mean:

```text
PER((SNR0_dB + SNR1_dB) / 2) = sqrt(PER0 * PER1)
```

Curve loading must enforce:

```text
SNR points are finite and strictly increasing
0 < PER <= 1 for every interpolation point
PER is non-increasing as SNR increases
all key dimensions are present and valid
duplicate (key, SNR) points and non-contiguous key groups are errors
row ordering and lookup ordering are deterministic
```

An exact zero cannot participate in logarithmic interpolation. Convert finite-trial zero-error measurements while generating the dataset, for example with `PER* = (errors + 0.5) / (packets + 1)`. Authors should record the estimator and production assumptions in leading `#` comments. Reject an unsmoothed zero in the runtime loader instead of silently switching to raw-PER interpolation. An exact PER of one is valid because `log(1) = 0`.

The concrete model loads the configured file once during local initialization and then performs only immutable lookups:

```cpp
void Ieee80211LdpcPerSuccessModel::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL)
        perTable.load(par("perTableFile"));
}

double Ieee80211LdpcPerSuccessModel::computeDataSuccessRate(
        const IIeee80211DataMode& mode,
        const Ieee80211DataEncodingPlan& plan,
        double snrDb) const
{
    auto key = makeCurveKey(mode, plan);
    double packetErrorRate = perTable.getPacketErrorRate(key, snrDb);
    return clamp(1.0 - packetErrorRate, 0.0, 1.0);
}
```

Declare only `perTableFile` in `Ieee80211LdpcPerSuccessModel.ned`; its default must name the checked-in baseline CSV. There is no automatic payload-length switch, payload-length scaling, nearest-MCS selection, or interpolation across calibrated-mode keys. Reuse across standard LDPC sizes and ideal-separated NSS is the explicit model assumption above, not an inferred nearest match. Adding other behavior later requires an explicit extension of the CSV/header contract, a mathematical definition, and validation against the calibration source.

The initial checked-in baseline is `Ieee80211LdpcPer1458B.csv`. It contains 32 structurally validated calibration curves: HT and VHT-SU, 20 MHz and 40 MHz, NSS 1, and MCS 0 through 7, all produced for a 1458-byte reference PSDU. The calibration assumes AWGN without fading. SNR is the post-reception signal-to-noise ratio over the occupied data and pilot tones, so the supplied SNR samples are bandwidth-independent and are reused for HT/VHT and 20/40 MHz; each format/bandwidth combination nevertheless has its own stored planner signature. Each SNR point used 40,000 simulated packets, and PER is conditional on successful PHY-header reception. A supplied zero-error result is stored as `(0 + 0.5) / (40000 + 1) = 0.5 / 40001`, keeping `log10(PER)` defined. The runtime deliberately reuses each curve for every standard LDPC payload/codeword size and ideal-separated NSS with the same format, bandwidth, and per-stream MCS. Missing format/bandwidth/per-stream-MCS calibrations still fail explicitly.

Header success remains on the existing BCC path. The exact bit-domain decoder and the packet-level curve model are separate fidelity paths. They use the same deterministic planner rules, but the receiver reconstructs its own plan from received signaling and timing instead of observing a sender plan.

### 11. Keep the exact HT/VHT bit path separate from legacy OFDM

The existing layered encoder/decoder is concretely tied to `Ieee80211OfdmMode`. Add HT/VHT-specific pipeline stages rather than adding amendment checks and downcasts to that legacy implementation.

```text
PSDU
  -> HT: prepend 16 zero SERVICE bits
     VHT: build SERVICE (including VHT-SIG-B CRC in B8-B15) and append PHY pad
  -> scramble with explicit deterministic seed
  -> compute a local immutable LDPC plan
  -> partition data among codewords
  -> shorten / encode / puncture-or-repeat
  -> concatenate codewords sequentially
  -> HT or VHT format-specific stream parser
  -> VHT segment parser for 160/80+80 MHz
  -> bypass frequency interleaver where the format specifies
  -> VHT LDPC tone mapping when applicable
  -> constellation mapper
```

For VHT SU, `Npld` includes the scrambled SERVICE, PSDU, and zero PHY pad bits; HT `Npld` includes only SERVICE and PSDU. The inverse pipeline must reconstruct the same plan from received signaling and Data duration before demapping and decoding, without consulting packet length or transmitter metadata. NSS>1 is modeled as ideal already-separated spatial streams. Unsupported production STBC and VHT MU combinations should fail at the pipeline boundary during the initial slice.

## Deterministic verification matrix

### LDPC encoder tests

Add `tests/unit/Ieee80211LdpcEncoder_1.test`:

```text
for N in {648, 1296, 1944}:
  for R in {1/2, 2/3, 3/4, 5/6}:
    code = annexFCode(N, R)
    information = fixedNonSymmetricPattern(K)
    encoded = coder(code).encode(information)

    assert encoded.length == N
    assert encoded[0:K] == information
    assert syndrome(H, encoded) == 0
```

Also cover exact matrix content, reference-vs-optimized encoding, malformed information lengths, and invalid code selections.

### LDPC decoder tests

Add a separate `tests/unit/Ieee80211LdpcDecoder_1.test` so decoder coverage is visible and can be run with its own focused filter. Exercise all 12 Annex F code variants with both `SUM_PRODUCT` and `NORMALIZED_MIN_SUM`; do not treat a noiseless encode/decode round trip as sufficient decoder coverage.

For every code/algorithm pair, include:

- noiseless soft-LLR recovery and the legacy hard-bit compatibility entry point;
- a fixed, weak wrong-sign LLR at an information-bit position and at a parity-bit position, with exact information recovery expected;
- at least one checked-in multi-error LLR vector that requires iterative message passing, independently cross-checked against a small reference decoder;
- repeat execution of each vector to prove identical decoded bits, convergence state, and iteration count;
- an explicit low-iteration-limit vector that does not converge, proving that the result has `converged=false`, reports the exhausted iteration count, and is not released by the caller as a valid payload.

Use fixed golden LLR vectors, with comments recording how they were produced; never generate test inputs from wall-clock time or an unrecorded random seed. Validate a candidate noisy vector before checking it in, then assert only its recorded outcome. Do not imply that either iterative algorithm corrects every error pattern.

```text
for N in {648, 1296, 1944}:
  for R in {1/2, 2/3, 3/4, 5/6}:
    code = annexFCode(N, R)
    information = fixedNonSymmetricPattern(code.K)
    encoded = referenceEncode(code, information)

    for algorithm in {SUM_PRODUCT, NORMALIZED_MIN_SUM}:
      decoder = coder(code, algorithm, maxIterations=fixtureLimit)

      successCases = {
        noiselessLlrs(encoded),
        weakWrongSignInformationBitFixture(code, encoded),
        weakWrongSignParityBitFixture(code, encoded),
        checkedInMultiErrorFixture(code, algorithm)
      }

      for llrs in successCases:
        first = decoder.decodeReliabilities(llrs)
        second = decoder.decodeReliabilities(llrs)

        assert first.converged
        assert first.informationBits == information
        assert 1 <= first.iterations <= fixtureLimit
        assert second == first

      hardResult = decoder.decode(encoded)
      assert hardResult == information

      failing = coder(code, algorithm, maxIterations=1)
          .decodeReliabilities(checkedInNeedsMoreThanOneIteration(code,
                                                                 algorithm))
      assert not failing.converged
      assert failing.iterations == 1
```

Add focused check-node tests below the complete-decoder tests:

- For sum-product, compare every outgoing message with an independent high-precision evaluation of `2*atanh(product(tanh(L/2)))`. Include zero/erasure, small magnitudes, mixed signs, two simultaneous zeros, and saturated inputs.
- For normalized min-sum, compare signs, first/second-minimum selection, normalization, and saturation against a simple test-only reference implementation, including tied minima and zero inputs.
- Verify that a successful full decode is reported only after the hard decision has a zero syndrome; use a fixture whose tentative decision still has a nonzero syndrome at the configured limit to cover failure.
- Reject `N-1` and `N+1` LLR vectors and every non-finite input, including `NaN` and positive/negative infinity. Verify clamping of finite inputs outside `[-maximumLlr, +maximumLlr]`.
- Verify algorithm configuration selection, invalid limits/factors, and the `sumProduct` default. Interleave calls for two code variants to check that no mutable decoder state leaks between calls.

Do not assert that sum-product is maximum-likelihood or that it wins for every individual loopy-graph input.

### IEEE 802.11 LDPC receive-mapping and decoding tests

Add `tests/unit/Ieee80211LdpcDataDecoder_1.test` for the inverse of the PPDU codeword mapping. This test sits above the core decoder and proves that the receiver constructs the right length-`N` LLR vector before invoking it:

```text
receivedLlrs = demappedReliabilitiesForTransmittedBits()

for each codewordPlan:
  codewordLlrs = vector(codewordPlan.N, 0.0)       // punctures are erasures
  copy transmitted observations into their planned codeword positions
  fill shortened positions with +MAX_LLR           // known zero bits
  add LLRs for repeated observations                // do not overwrite

  result = decoder.decodeReliabilities(codewordLlrs)
  require result.converged
  append result.informationBits excluding shortened bits

assert descramble(reassembledInformation) == expectedDataBits
```

Drive that path with both Annex I fixtures: the HT 20 MHz MCS 4 shortening/puncturing example and the HT 40 MHz MCS 1 shortening/repetition example. Run each fixture through both decoding algorithms and assert the exact recovered scrambled DATA bits, codeword boundaries, puncture locations (`LLR=0`), shortening locations (`LLR=+MAX_LLR`), and repeated-position LLR sums. Add a forced non-convergence fixture proving that receive failure propagates upward and no partial payload is delivered.

### Planner fixtures and boundaries

Add `tests/unit/Ieee80211LdpcPlanner_1.test`. Include these Annex I fixtures:

| Case | Expected plan |
|---|---|
| HT 20 MHz, MCS 4, 100-byte PSDU | `Npld=816`, `NCW=1`, `LLDPC=1944`, `NCBPS=208`, `Navbits=1248`, `Nshrt=642`, `Npunc=54`, `Nrep=0`, `NSYM=6` |
| HT 40 MHz, MCS 1, 140-byte PSDU, standard example with `STBC=1` (`mSTBC=2`) | `Npld=1136`, `NCW=2`, `LLDPC=1296`, `NCBPS=216`, `Navbits=2592`, `Nshrt=160`, `Npunc=0`, `Nrep=160`, `NSYM=12` |

The second case is a planner-only conformance fixture even while production STBC is rejected in the initial scope. Test values immediately below, at, and above 648, 1296, 1944, 2592 and each Table 19-16 threshold. For every plan assert:

```text
NCW >= 1
LLDPC in {648, 1296, 1944}
Nshrt >= 0, Npunc >= 0, Nrep >= 0
not (Npunc > 0 and Nrep > 0)
NSYM * NCBPS == Navbits
sum(codeword transmitted bits) == Navbits
LDPC tail bits == 0
```

### Header, mode, capability, and error-model tests

Add focused tests for:

- exact HT-SIG and VHT-SIG-A coding/extra-symbol bit positions and serializer round trips;
- invalid reserved fields and header/plan disagreement;
- the VHT header factory returning the correct concrete chunk type;
- distinct BCC/LDPC mode identities at equal MCS/bandwidth/NSS, with BCC as compatibility default;
- LDPC duration from planned `NSYM` and absence of BCC tails;
- local/peer capability matrix, multiple intended receivers, forced-illegal selection, and control responses;
- unchanged BCC NIST/Yans results;
- LDPC curve lookup, log-PER interpolation, monotonicity, and missing-curve failure.

Reuse the structure of `tests/unit/Ieee80211OnWireBitCompliance_1.test` and `tests/module/Ieee80211BitDomain.test`, but replace the latter's wall-clock-seeded helper for LDPC fixtures.

### PER curve interpolation tests

Add `tests/unit/Ieee80211LdpcPerTable_1.test` for the deterministic table value object, separate from packet reception decisions. Its valid fixture must use the exact CSV header specified above and include representative author comments that are ignored by the loader:

```text
curve = [(0 dB, 1e-1), (2 dB, 1e-3)]

assert interpolate(curve, -1 dB) == 1e-1       // lower clamp
assert interpolate(curve,  0 dB) == 1e-1       // exact sample
assert interpolate(curve,  1 dB) == 1e-2       // geometric midpoint
assert interpolate(curve,  2 dB) == 1e-3       // exact sample
assert interpolate(curve,  3 dB) == 1e-3       // upper clamp
```

Add a non-midpoint oracle using the explicit `log10(PER)` formula. Add isolated rejection fixtures for:

- a wrong or reordered CSV header;
- quoted fields, wrong field counts, trailing junk, and partially parsed numbers;
- unknown format tokens and illegal format/bandwidth/MCS/NSS combinations;
- invalid codeword length/counts, unreachable planner signatures, and simultaneous puncturing/repetition;
- zero/negative PER, PER greater than one, and non-finite SNR/PER;
- duplicate or decreasing SNR, increasing PER, a one-point curve, out-of-order keys, and non-contiguous key groups;
- an empty table and an exact missing-key lookup.

Verify that comments may appear before the header and between data groups without changing results. Also verify exact-sample preservation, both endpoint clamps including query `-INFINITY`/`+INFINITY`, geometric-midpoint interpolation, a non-midpoint base-10 logarithmic oracle, and `NaN` query rejection. Include a configured packet-level test proving that `Ieee80211LdpcPerSuccessModel` loads the configured CSV, derives the structural key from a receiver-reconstructed plan, reuses the NSS1 curve for legal ideal-separated NSS>1 modes (including HT MCS modulo 8), and fails on a missing format/bandwidth/per-stream-MCS calibration rather than falling back to BCC.

### Minimal deterministic end-to-end test

Use one fixed HT or VHT packet and fixed radio endpoints:

```text
case success:
  error endpoint = 0
  assert LDPC DATA selected and signaled
  assert DATA delivered and ACK received

case failure:
  error endpoint = 1
  assert DATA failure follows retry/drop policy
  assert ACK/retry counters and final delivery state
```

Do not infer monotonicity from one stochastic packet. Test pure success-model outputs directly; use fixed seeds only where an endpoint model cannot be made deterministic.

## Suggested implementation sequence and gates

1. Add exact-rate utilities, the reliability-aware FEC contract, Annex F tables, encoder, configurable sum-product/normalized-min-sum decoders, and their 12-variant unit tests.
2. Add the immutable-value HT/VHT-SU planner with Annex I, inverse receive reconstruction, and boundary tests.
3. Generalize HT/VHT mode/code identity and timing, keeping BCC selection and behavior unchanged.
4. Complete typed HT/VHT PHY headers, correct the VHT factory, and add bit-compliance tests.
5. Keep the transmit plan local, reconstruct the receive plan independently from on-air fields and timing, and add the replaceable packet-level LDPC success model, strict local CSV loader, and log-PER interpolation tests.
6. Add management capability representation, learned peer state, typed queries, selection gating, and its matrix tests.
7. Add the separate HT/VHT exact bit-domain pipeline and stage-by-stage Annex I fixtures.
8. Add the minimal deterministic ACK/retry scenario and only then consider a dedicated fingerprint case.

At every slice:

- build with `MODE=debug` and `-j$(nproc)`;
- run only explicit unit/module filters mapped to changed contracts;
- run architecture checks only for affected subtrees;
- compare BCC controls before and after any shared-path change;
- diagnose the first fingerprint divergence;
- do not update fingerprint CSV files without separate user approval.
