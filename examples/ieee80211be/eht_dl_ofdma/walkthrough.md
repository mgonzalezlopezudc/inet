# 802.11be Downlink OFDMA Example

This example highlights the improvements to Downlink Orthogonal Frequency Division Multiple Access (DL OFDMA) introduced in Wi-Fi 7 (802.11be). By aggregating multiple users in the frequency domain into Multiple Resource Units (MRUs), the AP can serve multiple stations simultaneously within a single TXOP.

## Topology

The network consists of a single Access Point (`ap`), connected to a backend server, and four wireless stations (`host[0]` through `host[3]`). The AP operates on a wide 320 MHz channel in the 6 GHz band, standard for EHT operations.

The server generates aggressive UDP traffic targeting all four hosts.

## Configurations

The `omnetpp.ini` defines the EHT MAC and Scheduler parameters. The `EhtDlSchedulerEqualSizedMRUs` assigns available tones to the target stations as equal-sized Multiple Resource Units (MRUs), which are an evolution of 802.11ax's RUs that allow for greater flexibility and gap spanning.

The simulation explores two scenarios:

1. **`EqualSizedMRUs`:** In a clean 320 MHz channel, the AP divides the spectrum into four roughly equal MRUs, scheduling all four stations simultaneously in every Downlink MU PPDU.
2. **`PuncturedMRUs`:** This configuration introduces preamble puncturing (`hePreamblePuncturing`). When interference (simulated by the puncturing pattern) occupies a portion of the channel, the AP punctures the affected 20 MHz subchannels. Due to the flexible nature of 802.11be MRUs, the AP can still aggregate the remaining contiguous and non-contiguous subchannels and schedule them among the stations efficiently, a major improvement over strict 802.11ax puncturing rules.

## Observations

When running the configurations, observe the MU PPDU transmissions. Under `PuncturedMRUs`, check the PHY headers and the scheduler's allocations to see how it spans the gap created by the punctured subchannels, continuing to serve the stations using the remaining bandwidth. You can see this visually if you enable `displayHeMuSignalPhyFields` in the visualizer.
