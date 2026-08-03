//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTGROUPIDMANAGEMENTFS_H
#define __INET_VHTGROUPIDMANAGEMENTFS_H

#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtGroupIdManager.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

namespace inet {
namespace ieee80211 {

/**
 * IEEE Std 802.11-2024, 11.39: individually addressed Group ID Management
 * followed by the normal ACK exchange. The AP-side state becomes ACTIVE only
 * after the ACK is accepted by this sequence.
 */
class INET_API VhtGroupIdManagementFs : public IFrameSequence
{
  protected:
    int step = 0;
    Ieee80211Mib *mib = nullptr;
    IVhtGroupIdManager *groupIdManager = nullptr;
    MacAddress peer;
    uint8_t groupId = 1;
    uint8_t userPosition = 0;
    uint64_t associationGeneration = 0;
    Hz channelWidth = Hz(0);

    Packet *buildActionFrame() const;
    bool isExpectedAck(Packet *packet, FrameSequenceContext *context) const;

  public:
    VhtGroupIdManagementFs(Ieee80211Mib *mib,
            IVhtGroupIdManager *groupIdManager, const MacAddress& peer,
            uint8_t groupId, uint8_t userPosition,
            uint64_t associationGeneration, Hz channelWidth);

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;
    virtual std::string getHistory() const override { return "VHT-Group-ID-Management (Action-ACK)"; }
};

} // namespace ieee80211
} // namespace inet

#endif
