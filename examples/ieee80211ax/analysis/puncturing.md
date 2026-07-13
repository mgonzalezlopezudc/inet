# HE preamble puncturing

![Puncturing comparison and runtime allocation](figures/puncturing/puncturing-frequency-allocation.png)

IEEE Std 802.11-2024 Table 27-1 identifies the 80 MHz puncturing forms, including puncturing the secondary 20 MHz channel (`80211ax-2024:chunk:10001`). Puncturing is a response to unavailable spectrum: it preserves use of the remaining subchannels but cannot increase clean-channel capacity.

Four conditions compare a clean 80 MHz channel, secondary-channel interference without puncturing, the same interference with a static `0100` mask, and runtime adaptation. The runtime case now resolves the HCF mask at scheduling time: it is unpunctured before 0.35 s, uses mask value 2 while the secondary-channel interferer is active, and returns to zero after 0.7 s.

Figure generation requires both masks 0 and 2 and aligned AP-radio RU offset, RU size, STA ID, and mask telemetry. Thus the frequency panel represents scheduled HE PPDUs, not a configuration string. The expected tradeoff is resilience under secondary-channel interference versus the loss of one 20 MHz subchannel; exact goodput depends on whether the offered load reaches either capacity ceiling.
