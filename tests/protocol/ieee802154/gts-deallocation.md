# IEEE 802.15.4 GTS deallocation

Plan: [B3-04](../../../plan/pending/802154-superframe-gts-plan.md#b3-04--ieee802154-add-gts-removal-and-cfp-repacking). Logical test: `Ieee802154GtsDeallocation`.

The native fixture associates two devices with a PAN coordinator on a
one-hop scalar medium. In a BO 3/SO 1 superframe, it allocates three two-slot
grants in order: device 1 transmit, device 2 transmit, device 1 receive. It
removes the middle grant either through device 2's real CAP command/ACK or
through an explicit coordinator request. Typed and raw frames run separately.

```text
Before:  CAP | device 1 RX | device 2 TX | device 1 TX
After:   expanded CAP     | device 1 RX | device 1 TX
```

The device request retires local use immediately and completes from its ACK.
The coordinator sends a GTS indication for that received request, removes any
old positive advertisement for the removed key, and advertises the surviving
relocation. An explicit coordinator removal instead confirms locally and
advertises a zero-start descriptor for the removed grant. Its original length
and direction remain visible for four actual superframes. Both paths retain
the surviving grant and expand the advertised CAP.

From the checkout root, run the owning checks with a fresh debug library:

```sh
make MODE=debug -j4
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154(GtsState|GtsWire|ExchangeTiming)_1[.]test$'
opp_run_module_tests --load . -p inet -m debug --no-build -f 'Ieee802154(GtsServiceContract|GtsAdapter)_1[.]test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154(GtsDeallocation|GtsAllocation|GtsData|Reassociation|Lifecycle)[.]test$'
```

| Scenario | Stimulus and required observation |
| --- | --- |
| 0–1 | Remove the middle grant from the device or coordinator. Check confirmation/indication distinctions, expanded CAP, relocated descriptor and rejection of later DATA on the removed grant. |
| 2–4 | Lose the command or its first ACK, or change the wire request to a mismatching length. The lost command leaves the coordinator grant intact; a valid mismatching command is ACKed and ignored. |
| 5–6 | Reset during the coordinator confirmation or an outstanding device request. The original terminal precedes RESET confirmation and is not repeated. |
| 7 | Drop the first relocation beacon for the surviving device. Real DATA retries retain one DSN and use only the actual relocated slot; delivery follows a later qualifying beacon. |
| 8 | Rebind the old member and reuse its short address. The retained zero-start wire record yields DENIED for that short address; a later allocation succeeds only after retention expires. |
| 9 | Grow ordinary beacon content and change SO. Automatic removal makes the affected DATA invalid while a surviving reservation carries actual acknowledged CFP DATA. |
| 10 | Observe the coordinator's zero-start record in four actual beacon RF publications and its absence in the fifth. |
| 11 | Exercise unsupported coordinator allocation/targetless removal and invalid targets, then prove a surviving grant still carries DATA. |

The fixture records native MAC PCAPs on the coordinator and removed device for
independent command and descriptor decoding. Those captures represent MAC
events; radio transmission signals provide the separate CFP timing evidence.
The fresh debug acceptance passed three unit fixtures, two module fixtures and
all 248 protocol cases, including all 24 cases in this fixture and 224 existing
controls. There were no protocol errors, skips or failures. Independent
Scapy 2.8.0 MAC/FCS inspection and a raw GTS List parser verified both
representations: device removal sent a type-zero, length-two request without a
removal descriptor; coordinator removal advertised short address 3 with
start zero and length two. Both retained the surviving short-address-2 receive
grant at slot 12 with Final CAP Slot 11. Mermaid rendering is **NOT_RUN**.
