//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.queue;

import inet.queueing.queue.DropTailQueue;

//
// Per-STA, per-AC queue bank for IEEE 802.11 HE OFDMA scheduling.
// Contains exactly 4 FIFO queues for QoS traffic, one per access category:
// - AC_BK (Background): TID 1, 2
// - AC_BE (Best Effort): TID 0, 3  
// - AC_VI (Video): TID 4, 5
// - AC_VO (Voice): TID 6, 7
//
// Each queue:
// - Preserves FIFO ordering
// - Tracks original enqueue timestamp for HoL delay calculation
// - Maintains packet and byte counts
// - Supports configurable capacity
//
module StationQueueBank
{
    parameters:
        @class(StationQueueBank);
        string staAddress;  // MAC address of the associated STA
        int packetCapacity = default(100);  // Per-AC queue capacity in packets
        @display("i=block/queue");

    submodules:
        // Background traffic queue (TID 1, 2)
        queueBK: DropTailQueue {
            parameters:
                packetCapacity = parent.packetCapacity;
                @display("p=100,100;i=block/queue");
        }

        // Best effort queue (TID 0, 3)
        queueBE: DropTailQueue {
            parameters:
                packetCapacity = parent.packetCapacity;
                @display("p=100,200;i=block/queue");
        }

        // Video queue (TID 4, 5)
        queueVI: DropTailQueue {
            parameters:
                packetCapacity = parent.packetCapacity;
                @display("p=100,300;i=block/queue");
        }

        // Voice queue (TID 6, 7)
        queueVO: DropTailQueue {
            parameters:
                packetCapacity = parent.packetCapacity;
                @display("p=100,400;i=block/queue");
        }
}
