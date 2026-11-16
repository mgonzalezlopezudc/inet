// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/physicallayer/wireless/ieee802154/Ieee802154PhyService.h"
#include <cmath>
#include <set>
#include "inet/common/InitStages.h"
#include "inet/common/Simsignals.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee802154PhyService);
using Admission = Ieee802154PhyServiceAdmission;
using Status = Ieee802154PhyServiceStatus;
using Abort = Ieee802154PhyServiceAbortReason;
using State = Ieee802154PhyState;

Ieee802154PhyService::~Ieee802154PhyService()
{
    cancelAndDelete(operationTimer);
    if (pending)
        delete pending->packet;
}

int Ieee802154PhyService::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154PhyService::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        radio.reference(this, "radioModule", true);
        client.reference(this, "clientModule", true);
        operationTimer = new cMessage("operationTimer");
        channel.channel = par("channelNumber");
        threshold = W(par("ccaEdThreshold"));
        ccaDuration = SimTime(par("ccaDuration"));
        if (ccaDuration < SimTime(1, SIMTIME_US) || ccaDuration > SimTime(1, SIMTIME_S) || SimTime(ccaDuration.inUnit(SIMTIME_US), SIMTIME_US) != ccaDuration)
            throw cRuntimeError("phyCcaDuration must be a whole number of microseconds in 1..1000000");
        if (channel.channel < 11 || channel.channel > 26 || !std::isfinite(threshold.get<W>()) || threshold < W(0))
            throw cRuntimeError("Invalid selected O-QPSK channel/CCA threshold");
        radio->subscribe(transmissionStartedSignal, this);
        radio->subscribe(transmissionEndedSignal, this);
        radio->subscribe(receptionStartedSignal, this);
        radio->subscribe(receptionEndedSignal, this);
    }
    else if (stage == INITSTAGE_LAST) {
        auto node = findContainingNode(this);
        auto status = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        started = !status || status->getState() == NodeStatus::UP;
        setChannel(channel);
        radio->setRadioMode(IRadio::RADIO_MODE_OFF);
        auto medium = check_and_cast<cModule *>(const_cast<IRadioMedium *>(radio->getMedium()));
        medium->subscribe(IRadioMedium::signalArrivalStartedSignal, this);
    }
}

Ieee802154PhyTimingParameters Ieee802154PhyService::getTimingParameters() const
{
    // IEEE 802.15.4-2024, 13.1/13.3 and Table 12-1: selected 2450-MHz O-QPSK.
    return {SimTime(16, SIMTIME_US), SimTime(192, SIMTIME_US), SimTime(160, SIMTIME_US),
            SimTime(32, SIMTIME_US), bps(250000), B(127), B(2)};
}

Admission Ieee802154PhyService::admit(uint64_t id, bool reset)
{
    if (!id || id <= lastId)
        return Admission::INVALID_REQUEST;
    lastId = id;
    if (!started || !client || !radio)
        return Admission::UNAVAILABLE;
    if (resetId || (pending && !reset))
        return Admission::BUSY;
    return Admission::ACCEPTED;
}

void Ieee802154PhyService::setChannel(const Ieee802154PhyChannel& value)
{
    radio->setCenterFrequency(Hz(2405000000.0 + 5000000.0 * (value.channel - 11)));
    channel = value;
}

uint8_t Ieee802154PhyService::powerLevel(W power, double low, double high)
{
    if (power <= W(0))
        return 0;
    double dbm = 10 * std::log10(power.get<W>() * 1000);
    return std::lround(std::clamp((dbm - low) / (high - low), 0.0, 1.0) * 255);
}

Admission Ieee802154PhyService::submitTransmit(uint64_t id, Packet *packet, const Ieee802154PhyTransmitRequest& request)
{
    Enter_Method("submitTransmit");
    auto admission = admit(id);
    if (admission != Admission::ACCEPTED)
        return admission;
    if (!packet)
        return Admission::INVALID_REQUEST;
    take(packet);
    std::optional<Status> failure;
    if (state != State::TRANSMIT)
        failure = Status::WRONG_TRANSCEIVER_STATE;
    else if (packet->getDataLength() > B(127))
        failure = Status::FRAME_TOO_LONG;
    else if (packet->getDataLength() < B(2) || packet->getDataLength().get<b>() % 8 || (request.startAt && *request.startAt < simTime()))
        failure = Status::INVALID_PARAMETER;
    if (failure) {
        delete packet;
        Ieee802154PhyTransmitConfirm confirm(*failure);
        client->handleTransmitConfirm(id, confirm);
        return Admission::ACCEPTED;
    }
    int64_t power = txPower;
    if (request.destination) {
        if (request.destination->isBroadcast() || request.destination->isExtendedGroup())
            power = broadcastPower;
        else {
            power = unicastPower;
            for (const auto& peer : peerPowers)
                if (peer.address == *request.destination)
                    power = peer.powerDbm;
        }
    }
    packet->addTagIfAbsent<SignalPowerReq>()->setPower(W(std::pow(10.0, double(power) / 10) / 1000));
    pending = Pending{Kind::TRANSMIT, id};
    pending->packet = packet;
    pending->start = request.startAt.value_or(simTime());
    if (pending->start == simTime())
        handleMessage(operationTimer);
    else
        scheduleAt(pending->start, operationTimer);
    return Admission::ACCEPTED;
}

Admission Ieee802154PhyService::submitState(uint64_t id, State target)
{
    Enter_Method("submitState");
    auto admission = admit(id);
    if (admission != Admission::ACCEPTED)
        return admission;
    if (target != State::OFF && target != State::RECEIVE && target != State::TRANSMIT) {
        Ieee802154PhyStateConfirm confirm(Status::INVALID_PARAMETER);
        client->handleStateConfirm(id, confirm);
        return Admission::ACCEPTED;
    }
    if (target == state) {
        Ieee802154PhyStateConfirm confirm(Status::SUCCESS);
        confirm.state = state;
        confirm.readyAt = simTime();
        client->handleStateConfirm(id, confirm);
        return Admission::ACCEPTED;
    }
    pending = Pending{Kind::STATE, id};
    pending->target = target;
    state = State::OFF;
    radio->setRadioMode(IRadio::RADIO_MODE_OFF);
    if (!pending || pending->id != id)
        return Admission::ACCEPTED;
    if (target == State::OFF)
        handleMessage(operationTimer);
    else
        scheduleAfter(getTimingParameters().turnaroundTime, operationTimer);
    return Admission::ACCEPTED;
}

Admission Ieee802154PhyService::measure(uint64_t id, Kind kind)
{
    auto admission = admit(id);
    if (admission != Admission::ACCEPTED)
        return admission;
    if (state != State::RECEIVE) {
        if (kind == Kind::CCA) {
            Ieee802154PhyCcaConfirm confirm(Status::WRONG_TRANSCEIVER_STATE);
            client->handleCcaConfirm(id, confirm);
        }
        else {
            Ieee802154PhyEdConfirm confirm(Status::WRONG_TRANSCEIVER_STATE);
            client->handleEdConfirm(id, confirm);
        }
        return Admission::ACCEPTED;
    }
    pending = Pending{kind, id};
    pending->start = simTime();
    pending->receivedPpdu = radio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING;
    auto end = simTime() + (kind == Kind::CCA ? ccaDuration : std::min(ccaDuration, SimTime(128, SIMTIME_US)));
    observation.begin(radio.get(), simTime(), end);
    scheduleAt(end, operationTimer);
    return Admission::ACCEPTED;
}

Admission Ieee802154PhyService::submitCca(uint64_t id)
{
    Enter_Method("submitCca");
    return measure(id, Kind::CCA);
}

Admission Ieee802154PhyService::submitEd(uint64_t id)
{
    Enter_Method("submitEd");
    return measure(id, Kind::ED);
}

std::optional<Ieee802154PhyPibValue> Ieee802154PhyService::getAttribute(const std::string& name) const
{
    if (name == "phyCurrentChannelInfo") return channel;
    if (name == "phyCcaDuration") return ccaDuration;
    if (name == "phyCcaMode") return Ieee802154PhyCcaMode::MODE_1;
    if (name == "phyCcaEdThreshold") return threshold;
    if (name == "phyMaxPacketSize") return uint64_t(127);
    if (name == "phyMaxTxPower") return int64_t(20);
    if (name == "phyTxPower") return txPower;
    if (name == "phyBroadcastTxPower") return broadcastPower;
    if (name == "phyUnicastTxPower") return unicastPower;
    if (name == "phyPeersTxPower") return peerPowers;
    if (name == "phyRangingSupported") return false;
    if (name == "phyRxRmarkerOffset" || name == "phyTxRmarkerOffset") return uint64_t(0);
    return {};
}

Status Ieee802154PhyService::setAttribute(const Ieee802154PhySetRequest& request)
{
    const auto& name = request.attribute;
    if (name == "phyMaxTxPower" || name == "phyRangingSupported")
        return Status::READ_ONLY;
    if (!getAttribute(name))
        return Status::UNSUPPORTED_ATTRIBUTE;
    if (name == "phyCurrentChannelInfo") {
        auto value = std::get_if<Ieee802154PhyChannel>(&request.value);
        if (!value) return Status::INVALID_PARAMETER;
        if (value->phy != Ieee802154PhyChannel::Phy::O_QPSK || value->band != Ieee802154PhyChannel::Band::MHZ_2450 || value->channel < 11 || value->channel > 26)
            return Status::UNSUPPORTED_CONFIGURATION;
        setChannel(*value);
    }
    else if (name == "phyCcaDuration") {
        auto value = std::get_if<simtime_t>(&request.value);
        if (!value || *value < SimTime(1, SIMTIME_US) || *value > SimTime(1, SIMTIME_S) || SimTime(value->inUnit(SIMTIME_US), SIMTIME_US) != *value)
            return Status::INVALID_PARAMETER;
        ccaDuration = *value;
    }
    else if (name == "phyCcaEdThreshold") {
        auto value = std::get_if<W>(&request.value);
        if (!value || !std::isfinite(value->get<W>()) || *value < W(0)) return Status::INVALID_PARAMETER;
        threshold = *value;
    }
    else if (name == "phyPeersTxPower") {
        auto value = std::get_if<std::vector<Ieee802154PhyPeerPower>>(&request.value);
        if (!value) return Status::INVALID_PARAMETER;
        std::set<Ieee802154Address> keys;
        for (const auto& peer : *value)
            if (peer.address.isNone() || peer.address.isBroadcast() || peer.address.isExtendedGroup() || peer.address.isShortUnallocated() ||
                    peer.powerDbm < -100 || peer.powerDbm > 20 || !keys.insert(peer.address).second)
                return Status::INVALID_PARAMETER;
        peerPowers = *value;
    }
    else if (name == "phyTxPower" || name == "phyBroadcastTxPower" || name == "phyUnicastTxPower") {
        auto value = std::get_if<int64_t>(&request.value);
        if (!value || *value < -100 || *value > 20) return Status::INVALID_PARAMETER;
        if (name == "phyTxPower") {
            if (*value < broadcastPower || *value < unicastPower) return Status::INVALID_PARAMETER;
            txPower = *value;
        }
        else {
            if (*value > txPower) return Status::INVALID_PARAMETER;
            (name == "phyBroadcastTxPower" ? broadcastPower : unicastPower) = *value;
        }
    }
    else {
        auto current = *getAttribute(name);
        if (request.value.index() != current.index()) return Status::INVALID_PARAMETER;
        if (name == "phyCcaMode" && std::get<Ieee802154PhyCcaMode>(request.value) == Ieee802154PhyCcaMode::MODE_1) return Status::SUCCESS;
        if (name == "phyMaxPacketSize" && std::get<uint64_t>(request.value) == 127) return Status::SUCCESS;
        if ((name == "phyRxRmarkerOffset" || name == "phyTxRmarkerOffset") && std::get<uint64_t>(request.value) == 0) return Status::SUCCESS;
        return Status::UNSUPPORTED_CONFIGURATION;
    }
    return Status::SUCCESS;
}

Admission Ieee802154PhyService::submitGet(uint64_t id, const Ieee802154PhyGetRequest& request)
{
    Enter_Method("submitGet");
    auto admission = admit(id);
    if (admission == Admission::ACCEPTED) {
        auto value = getAttribute(request.attribute);
        Ieee802154PhyGetConfirm confirm(request.attribute, value ? Status::SUCCESS : Status::UNSUPPORTED_ATTRIBUTE);
        confirm.value = value;
        client->handleGetConfirm(id, confirm);
    }
    return admission;
}

Admission Ieee802154PhyService::submitSet(uint64_t id, const Ieee802154PhySetRequest& request)
{
    Enter_Method("submitSet");
    auto admission = admit(id);
    if (admission != Admission::ACCEPTED)
        return admission;
    if (radio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING)
        return Admission::BUSY;
    Ieee802154PhySetConfirm confirm(request.attribute, setAttribute(request));
    client->handleSetConfirm(id, confirm);
    return Admission::ACCEPTED;
}

void Ieee802154PhyService::abort(Abort reason)
{
    reportAbort(retirePending(),reason);
}

std::optional<Ieee802154PhyService::Pending> Ieee802154PhyService::retirePending()
{
    cancelEvent(operationTimer);
    observation.clear();
    auto operation = pending;
    pending.reset();
    if (operation) {
        delete operation->packet;
        operation->packet = nullptr;
    }
    return operation;
}

void Ieee802154PhyService::reportAbort(const std::optional<Pending>& operation, Abort reason)
{
    if (!operation || !client)
        return;
    if (operation->kind == Kind::TRANSMIT) client->handleTransmitConfirm(operation->id, Ieee802154PhyTransmitConfirm(reason));
    else if (operation->kind == Kind::CCA) client->handleCcaConfirm(operation->id, Ieee802154PhyCcaConfirm(reason));
    else if (operation->kind == Kind::ED) client->handleEdConfirm(operation->id, Ieee802154PhyEdConfirm(reason));
    else client->handleStateConfirm(operation->id, Ieee802154PhyStateConfirm(reason));
}

Admission Ieee802154PhyService::submitReset(uint64_t id, bool defaults)
{
    Enter_Method("submitReset");
    auto admission = admit(id, true);
    if (admission != Admission::ACCEPTED)
        return admission;
    resetId = id;
    resetDefaults = defaults;
    receivedPowers.clear();
    state = State::OFF;
    radio->setRadioMode(IRadio::RADIO_MODE_OFF);
    abort(Abort::RESET);
    if (!resetId) // Stop may have completed reset from an interrupted-operation callback.
        return Admission::ACCEPTED;
    if (defaults) {
        auto defaultChannel = channel;
        defaultChannel.channel = par("channelNumber");
        setChannel(defaultChannel);
        threshold = W(par("ccaEdThreshold"));
        ccaDuration = SimTime(par("ccaDuration"));
        txPower = broadcastPower = unicastPower = 0;
        peerPowers.clear();
    }
    resetId.reset();
    if (client)
        client->handleResetConfirm(id, Ieee802154PhyResetConfirm(defaults, Status::SUCCESS));
    return Admission::ACCEPTED;
}

Ieee802154PhyServiceCancelResult Ieee802154PhyService::cancel(uint64_t id)
{
    Enter_Method("cancel");
    if ((resetId && *resetId == id) || (pending && pending->id == id && pending->transmissionId != -1))
        return Ieee802154PhyServiceCancelResult::NOT_CANCELLABLE;
    if (!pending || pending->id != id)
        return Ieee802154PhyServiceCancelResult::NOT_PENDING;
    if (pending->kind == Kind::STATE) {
        state = State::OFF;
        radio->setRadioMode(IRadio::RADIO_MODE_OFF);
    }
    abort(Abort::CANCELLED);
    return Ieee802154PhyServiceCancelResult::CANCELLED;
}

void Ieee802154PhyService::stop()
{
    started = false;
    receivedPowers.clear();
    state = State::OFF;
    auto operation = retirePending();
    auto stoppedReset = resetId;
    auto stoppedDefaults = resetDefaults;
    resetId.reset();
    if (client)
        client->handleAvailabilityIndication({false,state});
    if (radio)
        radio->setRadioMode(IRadio::RADIO_MODE_OFF);
    reportAbort(operation,Abort::STOPPED);
    if (stoppedReset && client)
        client->handleResetConfirm(*stoppedReset, Ieee802154PhyResetConfirm(stoppedDefaults, Abort::STOPPED));
}

bool Ieee802154PhyService::handleOperationStage(LifecycleOperation *operation, IDoneCallback *)
{
    Enter_Method("handleOperationStage");
    if (dynamic_cast<ModuleStartOperation *>(operation)) {
        if (operation->getCurrentStage() == ModuleStartOperation::STAGE_LOCAL) {
            started = true;
            if (client)
                client->handleAvailabilityIndication({started,state});
        }
    }
    else if (dynamic_cast<ModuleStopOperation *>(operation)) {
        if (operation->getCurrentStage() == ModuleStopOperation::STAGE_LOCAL)
            stop();
    }
    else if (dynamic_cast<ModuleCrashOperation *>(operation)) {
        if (operation->getCurrentStage() == ModuleCrashOperation::STAGE_CRASH)
            stop();
    }
    else
        throw cRuntimeError("Unsupported PHY lifecycle operation");
    return true;
}

void Ieee802154PhyService::handleMessage(cMessage *message)
{
    if (message == operationTimer) {
        if (!pending)
            return;
        if (!client) {
            abort(Abort::STOPPED);
            return;
        }
        if (pending->kind == Kind::TRANSMIT) {
            auto packet = pending->packet;
            pending->packet = nullptr;
            auto representation = packet->dup();
            delete packet;
            radio->transmit(representation);
        }
        else if (pending->kind == Kind::STATE) {
            auto operation = *pending;
            pending.reset();
            state = operation.target;
            radio->setRadioMode(state == State::OFF ? IRadio::RADIO_MODE_OFF : state == State::RECEIVE ? IRadio::RADIO_MODE_RECEIVER : IRadio::RADIO_MODE_TRANSMITTER);
            if (client) {
                Ieee802154PhyStateConfirm confirm(Status::SUCCESS);
                confirm.state = operation.target;
                confirm.readyAt = simTime();
                client->handleStateConfirm(operation.id, confirm);
            }
        }
        else {
            auto operation = *pending;
            auto energy = observation.finish();
            pending.reset();
            if (!client)
                return;
            if (operation.kind == Kind::CCA) {
                Ieee802154PhyCcaConfirm confirm(Status::SUCCESS);
                confirm.outcome = operation.receivedPpdu || energy.peakPower > threshold ? Ieee802154PhyCcaOutcome::BUSY : Ieee802154PhyCcaOutcome::IDLE;
                confirm.timing = Ieee802154PhyMeasurementTiming{operation.start, simTime()};
                client->handleCcaConfirm(operation.id, confirm);
            }
            else {
                Ieee802154PhyEdConfirm confirm(Status::SUCCESS);
                confirm.level = powerLevel(energy.meanPower, -75, -35);
                confirm.meanPower = energy.meanPower;
                confirm.timing = Ieee802154PhyMeasurementTiming{operation.start, simTime()};
                client->handleEdConfirm(operation.id, confirm);
            }
        }
        return;
    }
    auto packet = check_and_cast<Packet *>(message);
    auto receivedPower = receivedPowers.find(packet->getId());
    std::optional<W> meanPower;
    std::optional<int> transmissionId;
    if (receivedPower != receivedPowers.end()) {
        meanPower = receivedPower->second.power;
        transmissionId = receivedPower->second.transmissionId;
        receivedPowers.erase(receivedPower);
    }
    if (!packet->arrivedOn("lowerLayerIn") || state != State::RECEIVE || !started || !client) {
        delete packet;
        return;
    }
    auto signalTime = packet->getTag<SignalTimeInd>();
    Ieee802154PhyReceiveIndication indication;
    indication.channel = channel;
    indication.timing = {signalTime->getStartTime(), signalTime->getStartTime() + SimTime(192, SIMTIME_US), signalTime->getEndTime(), signalTime->getEndTime()};
    indication.fcsLength = B(2);
    indication.transmissionId = transmissionId;
    if (meanPower) {
        indication.meanPower = *meanPower;
        indication.lqi = powerLevel(*meanPower, -100, -20);
    }
    client->handleReceiveIndication(packet, indication);
}

void Ieee802154PhyService::receiveSignal(cComponent *, simsignal_t id, cObject *object, cObject *details)
{
    Enter_Method_Silent();
    if (id == IRadioMedium::signalArrivalStartedSignal) {
        observation.include(check_and_cast<const IReception *>(object));
        return;
    }
    if (id == receptionStartedSignal) {
        if (pending && pending->kind == Kind::CCA && simTime() < pending->start + ccaDuration)
            pending->receivedPpdu = true;
        auto reception = check_and_cast<const IReception *>(object);
        Ieee802154PhyReceiveStart start{reception->getTransmission()->getId(), reception->getStartTime(), reception->getEndTime()};
        if (started && client)
            client->handleReceiveStarted(start);
        return;
    }
    if (id == receptionEndedSignal) {
        auto reception = check_and_cast<const IReception *>(object);
        auto packet = check_and_cast<Packet *>(details);
        auto scalar = check_and_cast<const ScalarReceptionAnalogModel *>(reception->getAnalogModel());
        receivedPowers.emplace(packet->getId(), ReceivedQuality{scalar->getPower(), reception->getTransmission()->getId()});
        return;
    }
    auto transmission = check_and_cast<const ITransmission *>(object);
    if (!pending || pending->kind != Kind::TRANSMIT)
        return;
    if (id == transmissionStartedSignal) {
        pending->transmissionId = transmission->getId();
        pending->timing = {transmission->getStartTime(), transmission->getDataStartTime(), transmission->getDataEndTime(), transmission->getEndTime()};
        auto requestId = pending->id;
        auto timing = pending->timing;
        if (client)
            client->handleTransmitStarted(requestId, {timing.ppduStart, timing.psduStart, timing.ppduEnd});
    }
    else if (id == transmissionEndedSignal && pending->transmissionId == transmission->getId()) {
        auto operation = *pending;
        pending.reset();
        if (client) {
            Ieee802154PhyTransmitConfirm confirm(Status::SUCCESS);
            confirm.timing = operation.timing;
            client->handleTransmitConfirm(operation.id, confirm);
        }
    }
}

} // namespace physicallayer
} // namespace inet
