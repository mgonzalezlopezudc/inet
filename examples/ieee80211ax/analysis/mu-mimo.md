# MU-MIMO plots

The left panel is a measured PPDU-by-STA allocation matrix whose cells contain
the allocated NSS. Empty cells mean that the STA was not part of that PPDU;
multiple filled cells in one column demonstrate simultaneous spatial service.
The synchronized `heStreamStartIndex` signal verifies non-overlapping stream
ranges. The companion panel shows measured aggregate goodput. A wider or
higher-dimensional MU-MIMO setup should serve more users concurrently and
increase aggregate delivery.
