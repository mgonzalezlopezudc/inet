## Contents

* Select the capture point
* Add a recorder with command-line overrides
* Select the recorded representation
* Bound capture cost
* Validate the artifact

## Select the capture point

Inspect the instantiated NED model and effective INI configuration before adding a recorder. Determine:

* The node path and whether it exposes `numPcapRecorders`.
* The interface or protocol layer whose packet signal answers the question.
* Whether sender, receiver, AP/router, or several observation points are required.

`moduleNamePatterns` is relative to the node containing the recorder. Use `eth[0]`, `wlan[0]`, or `ppp[0]`, not a full network path. A custom node may not inherit recorder support; inspect its NED ancestry instead of forcing an override.

Ingress/egress meaning depends on the packet signal observed at that module. Inspect the recorder subscription or module source when direction is not explicit in the capture.

## Add a recorder with command-line overrides

Store the known-good command from `inet-simulation-run` in a Bash array named `INET_RUN`, then append narrow recorder overrides:

```sh
NODE='*.host[0]'
PCAP="logs/${CONFIG}-${RUN}-host0-eth0.pcapng"

"${INET_RUN[@]}" \
  -c "$CONFIG" \
  -r "$RUN" \
  "--${NODE}.numPcapRecorders=1" \
  "--${NODE}.pcapRecorder[0].pcapFile=\"$PCAP\"" \
  "--${NODE}.pcapRecorder[0].fileFormat=\"pcapng\"" \
  "--${NODE}.pcapRecorder[0].timePrecision=9" \
  "--${NODE}.pcapRecorder[0].moduleNamePatterns=\"eth[0]\"" \
  "--${NODE}.pcapRecorder[0].alwaysFlush=true" \
  "--${NODE}.pcapRecorder[0].verbose=false"
```

Use unique filenames that identify configuration, run, node, interface, and recorder. `alwaysFlush=true` is useful for diagnostic runs that may abort; disable it for long performance-sensitive runs only when losing the last buffered packets is acceptable.

Use several recorders and separate files when capture points must remain distinguishable.

## Select the recorded representation

`moduleNamePatterns` selects where packets are observed. `dumpProtocols` selects which protocol representation is written. For example:

```sh
"--${NODE}.pcapRecorder[0].dumpProtocols=\"ipv4 ipv6\""
```

For native Wi-Fi analysis, select the installed model's IEEE 802.11 MAC representation. If INET cannot determine or convert the link type, inspect the capture module, `dumpProtocols`, packet protocol tags, and available recorder helpers.

When exact computed checksum or FCS fields matter, add supported `checksumMode="computed"` or `fcsMode="computed"` overrides and record that they may change model behavior. Do not change these modes silently.

## Bound capture cost

Prefer narrowing the observation point or time interval. When needed:

* Set `snaplen` only if truncation will not remove required headers or payload.
* Use an INET `packetFilter` expression to reduce recording at the source.
* Do not use Wireshark display-filter syntax in `packetFilter`.
* Record broadly and filter afterward when the correct INET expression is uncertain.

## Validate the artifact

Before reasoning from the capture, confirm that it is nonempty and decodes at the intended link layer:

```sh
test -s "$PCAP"
capinfos "$PCAP"
tshark -n -r "$PCAP" -c 10
```

A successful simulation does not prove that the recorder matched a module or observed packets.
