# Native MCPS request boundaries

> **Kind:** procedure · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [data-ack.md](data-ack.md)

`Ieee802154Admission.test` uses two production interfaces on PAN 0x1234 with
management disabled, typed frames and raw bytes. The receiver is a static PAN
coordinator so source-only frames exercise their real acceptance predicate.
An independent nine-row layout table gives complete header/FCS lengths 9, 15,
9, 15, 11, 17, 17, 23 and 25 bytes: source omitted/short/EUI, destination
omitted/short/EUI, same-PAN compression and separate source/destination PANs.

For each layout, a 128-byte complete MPDU must fail FRAME_TOO_LONG before
transmission and without consuming a DSN. Its 127-byte neighbor traverses the
real radio path and delivers unchanged bytes. A lower-frame subscriber checks
actual MPDU size/version; provider confirmations and receiver indications are
independent observations. An empty MSDU is valid and produces the nine-byte
destination-only MPDU. A three-bit MSDU is invalid: Table 8-30's MSDU is a string
of octets, so silently padding it into a byte is not an admissible conversion.

Additional cases cover absent source and destination, unallocated destination,
short-source allocation sentinels, invalid source mode, invalid/unsupported
security level, null packet, zero/reused/decreasing IDs and BUSY ownership.
A counted original packet must be destroyed before every terminal confirmation;
refusals retain caller ownership and emit no confirmation. GET observes the
authoritative DSN after every attempt, including rollover on accepted frames.
Each run expects 27 terminal results, ten real transmissions and ten deliveries.

The three-bit case reproduced acceptance by the initial production MAC. The
bounded repair belongs to submitData, after ownership transfer and before byte
conversion: non-octet MSDUs complete INVALID_PARAMETER and consume no DSN. The
same validation applies to direct and indirect admission. Existing malformed
frame reception remains a separate decoder/filtering responsibility. This
source/control-path contract was validated before the repair; the direct test
checks the actual provider and its failure callback, not a mirrored helper.
