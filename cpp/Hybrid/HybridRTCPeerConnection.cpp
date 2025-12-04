#include "HybridRTCPeerConnection.hpp"
#include "HybridRTCRtpTransceiver.hpp"

using namespace margelo::nitro::webrtc;

auto HybridRTCPeerConnection::getRemoteMediaFromMid (const std::string &mid)
    -> const rtc::Description::Media *
{
    auto description = peerConnection->remoteDescription ();
    if (!description)
    {
        return nullptr;
    }
    for (int i = 0; i < description->mediaCount (); i++)
    {
        auto mediaVar = description->media (i);
        if (std::holds_alternative<rtc::Description::Media *> (mediaVar))
        {
            auto *media = std::get<rtc::Description::Media *> (mediaVar);
            if (media->mid () == mid)
            {
                return media;
            }
        }
    }
    return nullptr;
}

void HybridRTCPeerConnection::setConfiguration (
    const std::optional<RTCConfiguration> &config)
{
    peerConnection->close ();
    rtc::Configuration c;
    // c.disableAutoNegotiation = true;

    if (config.has_value () && config->iceServers.has_value ())
    {
        for (const auto &jsServer : config.value ().iceServers.value ())
        {
            std::vector<std::string> urls;
            if (std::holds_alternative<std::vector<std::string>> (
                    jsServer.urls))
            {
                urls = std::get<std::vector<std::string>> (jsServer.urls);
            }
            else
            {
                urls = { std::get<std::string> (jsServer.urls) };
            }

            for (const auto &url : urls)
            {
                rtc::IceServer server (url);
                server.username = jsServer.username.value_or ("");
                server.password = jsServer.credential.value_or ("");
                c.iceServers.push_back (server);
            }
        }
    }

    peerConnection = std::make_shared<rtc::PeerConnection> (c);
    peerConnection->onStateChange (
        [this] (rtc::PeerConnection::State state)
        {
            if (this->connectionStateChangeHandler)
            {
                this->connectionStateChangeHandler.value () ({});
            }
        });
    peerConnection->onGatheringStateChange (
        [this] (rtc::PeerConnection::GatheringState state)
        {
            if (this->iceGatheringStateChangeHandler)
            {
                this->iceGatheringStateChangeHandler.value () ({});
            }
            if (state == rtc::PeerConnection::GatheringState::Complete)
            {
                if (this->iceCandidateHandler)
                {
                    RTCPeerConnectionIceEvent event{ nitro::null };
                    this->iceCandidateHandler.value () (event);
                }
            }
        });
    peerConnection->onLocalCandidate (
        [this] (rtc::Candidate candidate)
        {
            if (this->iceCandidateHandler)
            {
                RTCIceCandidate candidateObj{ candidate.candidate () };
                RTCPeerConnectionIceEvent event{ candidateObj };
                this->iceCandidateHandler.value () (event);
            }
        });
}

void HybridRTCPeerConnection::close () { peerConnection->close (); }

RTCPeerConnectionState HybridRTCPeerConnection::getConnectionState ()
{
    switch (peerConnection->state ())
    {
    case rtc::PeerConnection::State::New:
        return RTCPeerConnectionState::NEW;
    case rtc::PeerConnection::State::Connecting:
        return RTCPeerConnectionState::CONNECTING;
    case rtc::PeerConnection::State::Connected:
        return RTCPeerConnectionState::CONNECTED;
    case rtc::PeerConnection::State::Disconnected:
        return RTCPeerConnectionState::DISCONNECTED;
    case rtc::PeerConnection::State::Failed:
        return RTCPeerConnectionState::FAILED;
    case rtc::PeerConnection::State::Closed:
        return RTCPeerConnectionState::CLOSED;
    }
}

RTCIceGatheringState HybridRTCPeerConnection::getIceGatheringState ()
{
    switch (peerConnection->gatheringState ())
    {
    case rtc::PeerConnection::GatheringState::New:
        return RTCIceGatheringState::NEW;
    case rtc::PeerConnection::GatheringState::InProgress:
        return RTCIceGatheringState::GATHERING;
    case rtc::PeerConnection::GatheringState::Complete:
        return RTCIceGatheringState::COMPLETE;
    }
}

std::string HybridRTCPeerConnection::getLocalDescription ()
{
    return peerConnection->localDescription ()->generateSdp ();
}

std::string HybridRTCPeerConnection::getRemoteDescription ()
{
    return peerConnection->remoteDescription ()->generateSdp ();
}

std::shared_ptr<Promise<void>> HybridRTCPeerConnection::setLocalDescription (
    const std::optional<RTCSessionDescriptionInit> &description)
{
    if (!description.has_value ())
    {
        return Promise<void>::resolved ();
    }

    rtc::Description sdp (description->sdp.value_or (""));
    peerConnection->setLocalDescription (sdp.type ());
    return Promise<void>::resolved ();
}

std::shared_ptr<Promise<void>> HybridRTCPeerConnection::setRemoteDescription (
    const RTCSessionDescriptionInit &description)
{
    rtc::Description::Type remoteSdpType;
    switch (description.type)
    {
    case RTCSdpType::OFFER:
        remoteSdpType = rtc::Description::Type::Offer;
        break;
    case RTCSdpType::ANSWER:
        remoteSdpType = rtc::Description::Type::Answer;
        break;
    case RTCSdpType::PRANSWER:
        remoteSdpType = rtc::Description::Type::Pranswer;
        break;
    case RTCSdpType::ROLLBACK:
        remoteSdpType = rtc::Description::Type::Rollback;
        break;
    default:
        return Promise<void>::rejected (
            std::make_exception_ptr (std::runtime_error ("Invalid SDP type")));
    }

    rtc::Description remoteDescription (description.sdp.value_or (""),
                                        remoteSdpType);

    if (remoteSdpType == rtc::Description::Type::Offer)
    {
        for (int i = 0; i < remoteDescription.mediaCount (); i++)
        {
            auto remoteMediaVar = remoteDescription.media (i);
            if (!std::holds_alternative<rtc::Description::Media *> (
                    remoteMediaVar))
            {
                continue;
            }

            auto *remoteMedia
                = std::get<rtc::Description::Media *> (remoteMediaVar);
            auto localDir = rtc::Description::Direction::Inactive;
            switch (remoteMedia->direction ())
            {
            case rtc::Description::Direction::SendOnly:
                localDir = rtc::Description::Direction::RecvOnly;
                break;
            case rtc::Description::Direction::RecvOnly:
                localDir = rtc::Description::Direction::SendOnly;
                break;
            case rtc::Description::Direction::SendRecv:
                localDir = rtc::Description::Direction::SendRecv;
                break;
            default:
                continue;
            }

            // match transceiver
            for (auto &transceiver : transceivers)
            {
                if (transceiver->mid.has_value ())
                {
                    continue;
                }
                if (transceiver->direction != localDir)
                {
                    continue;
                }
                if (transceiver->getKind () != remoteMedia->type ())
                {
                    continue;
                }

                auto localMedia = transceiver->answerMedia (*remoteMedia);
                transceiver->track = peerConnection->addTrack (localMedia);
                transceiver->peerConnection = peerConnection;
            }
        }
    }

    peerConnection->setRemoteDescription (remoteDescription);
    for (const auto &transceiver : transceivers)
    {
        auto media = getRemoteMediaFromMid (transceiver->mid.value_or (""));
        if (!media)
        {
            continue;
        }
        transceiver->start (*media);
    }

    return Promise<void>::async (
        [this] () -> void
        {
            // call ontrack handler
            for (const auto &transceiver : transceivers)
            {
                if (!transceiver->receiver->track)
                {
                    continue;
                }

                if (trackHandler.has_value ())
                {
                    RTCTrackEvent event;
                    event.track = transceiver->receiver->track;
                    for (const auto &stream : transceiver->receiver->streams)
                    {
                        event.streams.push_back (stream);
                    }
                    trackHandler.value () (event);
                }
            }
        });
}

StateChangeHandler HybridRTCPeerConnection::getOnconnectionstatechange ()
{
    return connectionStateChangeHandler;
}

void HybridRTCPeerConnection::setOnconnectionstatechange (
    const StateChangeHandler &handler)
{
    connectionStateChangeHandler = handler;
}

StateChangeHandler HybridRTCPeerConnection::getOnicegatheringstatechange ()
{
    return iceGatheringStateChangeHandler;
}

void HybridRTCPeerConnection::setOnicegatheringstatechange (
    const StateChangeHandler &handler)
{
    iceGatheringStateChangeHandler = handler;
}

IceCandidateHandler HybridRTCPeerConnection::getOnicecandidate ()
{
    return iceCandidateHandler;
}

std::shared_ptr<Promise<void>> HybridRTCPeerConnection::addIceCandidate (
    const std::optional<std::variant<nitro::NullType, RTCIceCandidateInit>>
        &candidate)
{
    if (!candidate.has_value ())
    {
        return Promise<void>::resolved ();
    }
    if (std::holds_alternative<nitro::NullType> (candidate.value ()))
    {
        return Promise<void>::resolved ();
    }
    auto candidateInit = std::get<RTCIceCandidateInit> (candidate.value ());

    if (candidateInit.candidate.value_or ("").length () == 0)
    {
        return Promise<void>::resolved ();
    }

    std::string candidateStr = candidateInit.candidate.value ();
    std::string midStr = "";
    if (candidateInit.sdpMid.has_value ())
    {
        if (std::holds_alternative<std::string> (
                candidateInit.sdpMid.value ()))
        {
            midStr = std::get<std::string> (candidateInit.sdpMid.value ());
        }
    }
    rtc::Candidate cand (candidateStr, midStr);
    peerConnection->addRemoteCandidate (cand);
    return Promise<void>::resolved ();
}

void
HybridRTCPeerConnection::setOnicecandidate (const IceCandidateHandler &handler)
{
    iceCandidateHandler = handler;
}

TrackHandler HybridRTCPeerConnection::getOntrack () { return trackHandler; }

void HybridRTCPeerConnection::setOntrack (const TrackHandler &handler)
{
    trackHandler = handler;
}

std::vector<std::shared_ptr<HybridRTCRtpTransceiverSpec>>
HybridRTCPeerConnection::getTransceivers ()
{
    std::vector<std::shared_ptr<HybridRTCRtpTransceiverSpec>> specs;
    for (const auto &transceiver : transceivers)
    {
        specs.push_back (transceiver);
    }
    return specs;
}

std::shared_ptr<HybridRTCRtpTransceiverSpec>
HybridRTCPeerConnection::addTransceiver (
    const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                       std::string> &trackOrKind,
    const std::optional<RTCRtpTransceiverInit> &init)
{
    std::string mid = std::to_string (transceivers.size ());
    auto rtpTransceiver
        = std::make_shared<HybridRTCRtpTransceiver> (trackOrKind, init);

    transceivers.push_back (rtpTransceiver);
    return rtpTransceiver;
}

std::shared_ptr<Promise<RTCSessionDescriptionInit>>
HybridRTCPeerConnection::createOffer ()
{
    for (int i = 0; i < transceivers.size (); i++)
    {
        std::string mid = std::to_string (i);
        auto transceiver = transceivers[i];
        auto media = transceiver->offerMedia (mid);
        transceiver->track = peerConnection->addTrack (media);
        transceiver->peerConnection = peerConnection;
    }

    rtc::Description sdp = peerConnection->createOffer ();

    RTCSessionDescriptionInit description;
    description.type = RTCSdpType::OFFER;
    description.sdp = sdp.generateSdp ();

    return Promise<RTCSessionDescriptionInit>::resolved (
        std::move (description));
}

std::shared_ptr<Promise<RTCSessionDescriptionInit>>
HybridRTCPeerConnection::createAnswer ()
{
    rtc::Description sdp = peerConnection->createAnswer ();

    RTCSessionDescriptionInit description;
    description.type = RTCSdpType::ANSWER;
    description.sdp = sdp.generateSdp ();

    return Promise<RTCSessionDescriptionInit>::resolved (
        std::move (description));
}
