// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSERVICETAG_H
#define __INET_IEEE802154MACSERVICETAG_H

#include "inet/common/TagBase.h"
#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"

namespace inet {

/** Local gate facade; these values never become wire fields. */
class INET_API Ieee802154MacDataReq : public TagBase
{
  public:
    Ieee802154MacDataRequest request;
    virtual Ieee802154MacDataReq *dup() const override { return new Ieee802154MacDataReq(*this); }
    virtual std::string str() const override { return "destination=" + request.destinationAddress.str(); }
};

class INET_API Ieee802154MacDataInd : public TagBase
{
  public:
    Ieee802154MacDataIndication indication;
    virtual Ieee802154MacDataInd *dup() const override { return new Ieee802154MacDataInd(*this); }
    virtual std::string str() const override { return "source=" + indication.sourceAddress.str(); }
};

/** Confirmation or refusal, distinguished explicitly; packet has no MSDU bytes. */
class INET_API Ieee802154MacDataResultInd : public TagBase
{
  public:
    Ieee802154MacServiceRequestId requestId = 0;
    uint8_t msduHandle = 0;
    Ieee802154MacServiceAdmission admission = Ieee802154MacServiceAdmission::ACCEPTED;
    std::optional<Ieee802154MacDataConfirm> confirm;
    virtual Ieee802154MacDataResultInd *dup() const override { return new Ieee802154MacDataResultInd(*this); }
};

class INET_API Ieee802154MacPromiscuousInd : public TagBase
{
  public:
    Ieee802154MacPromiscuousIndication indication;
    virtual Ieee802154MacPromiscuousInd *dup() const override { return new Ieee802154MacPromiscuousInd(*this); }
};

class INET_API Ieee802154MacCommStatusInd : public TagBase
{
  public:
    std::optional<Ieee802154MacCommStatusIndication> indication;
    virtual Ieee802154MacCommStatusInd *dup() const override { return new Ieee802154MacCommStatusInd(*this); }
};

} // namespace inet
#endif
