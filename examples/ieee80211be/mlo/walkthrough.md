# 802.11be Multi-Link Operation (MLO) Example

This example demonstrates the marquee feature of Wi-Fi 7: Multi-Link Operation (MLO). The network leverages the `Ieee80211MldMac` to seamlessly manage and distribute traffic across multiple independent wireless links simultaneously.

## Topology

The topology features a multi-link access point (`MldAp`) and a multi-link station (`MldHost`), connected to a wired backend server.
Instead of standard single-radio interfaces, these custom nodes employ an `MldInterface` compound module. This interface integrates a single upper MLD MAC (`Ieee80211MldMac`) and two lower independent 802.11 MAC/PHY instances (referred to as `link[0]` and `link[1]`).

## Configurations

The `omnetpp.ini` establishes a 5 GHz link (160 MHz width) and a 6 GHz link (320 MHz width) between the AP and the STA. A single, high-bandwidth UDP stream is sent from the server to the STA. The MLD MAC handles queueing the incoming packets and dynamically steering them to the lower links as transmission opportunities arise.

There are two primary configurations to evaluate different MLO paradigms:

1. **`Str` (Simultaneous Transmit and Receive):** In this mode, both the 5 GHz and 6 GHz links operate entirely independently. The device can transmit on the 6 GHz link while simultaneously receiving on the 5 GHz link without causing internal interference. This maximizes aggregated throughput.
2. **`Nstr` (Non-Simultaneous Transmit and Receive):** In this mode, the device's radios lack the isolation required for STR. Consequently, transmissions and receptions across the links must be carefully synchronized by the MLD MAC to prevent self-interference, leading to different channel access patterns and slightly constrained throughput compared to STR.

## Observations

When running the simulations, observe the packet flow in the GUI (or via vector/scalar statistics). You will notice that a single logical application flow is distributed across both the 5 GHz and 6 GHz radio channels, significantly boosting total data rate and reducing latency. Comparing the `Str` and `Nstr` configurations illustrates the scheduling constraints imposed by NSTR capabilities.
