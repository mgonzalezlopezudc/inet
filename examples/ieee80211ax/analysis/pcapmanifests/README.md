# Packet-capture manifest history

`analyze_pcap_types.py --generate` publishes one immutable
`<YYYYMMDDTHHMMSSZ>.json` manifest here before atomically updating the
compatibility mirror at `../pcap_statistics_manifest.json`.

Do not edit or replace a session manifest. Generate a new session ID when the
captures, scalar metadata, source state, binary, or analysis tools change.
