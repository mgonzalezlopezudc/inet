#!/usr/bin/env bash
set -euo pipefail
peer_root=${1:?usage: run.sh /path/to/pinned/ns-3-checkout [output-directory]}
output_dir=${2:-/tmp/lowpan-peer}
script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
mkdir -p -- "$output_dir"
test "$(git -C "$peer_root" rev-parse HEAD)" = 5e35cfbc28bd43aa9ceb0d1edea44078d6e76830
c++ -std=c++20 -I"$peer_root/build/include" -I"$peer_root/src/sixlowpan/test" \
    "$script_dir/ns3-vector-peer.cc" "$peer_root/src/sixlowpan/test/mock-net-device.cc" \
    -L"$peer_root/build/lib" -Wl,-rpath,"$peer_root/build/lib" \
    -lns3-dev-sixlowpan-default -lns3-dev-internet-default \
    -lns3-dev-network-default -lns3-dev-core-default -o "$output_dir/peer"
"$output_dir/peer" decode "$script_dir/inet-encoded.hex" > "$output_dir/decoded.hex"
cmp "$script_dir/inet-original.hex" "$output_dir/decoded.hex"
"$output_dir/peer" encode "$script_dir/inet-original.hex" > "$output_dir/encoded.hex"
cmp "$script_dir/ns3-encoded.hex" "$output_dir/encoded.hex"
echo 'Pinned peer encode/decode vectors match; run LowpanPeerReceive.test for INET receive proof.'
