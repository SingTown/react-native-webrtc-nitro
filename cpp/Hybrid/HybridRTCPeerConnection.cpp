#include "HybridRTCPeerConnection.hpp"
#include "HybridRTCRtpTransceiver.hpp"

using namespace margelo::nitro::webrtc;

static auto getOrCreateTrack (
    const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                       std::string> &trackOrKind)
    -> std::shared_ptr<HybridMediaStreamTrack>
{
    if (std::holds_alternative<std::shared_ptr<HybridMediaStreamTrackSpec>> (
            trackOrKind))
    {
        auto mediaStreamTrackSpec
            = std::get<std::shared_ptr<HybridMediaStreamTrackSpec>> (
                trackOrKind);
        return std::dynamic_pointer_cast<HybridMediaStreamTrack> (
            mediaStreamTrackSpec);
    }
    else if (std::holds_alternative<std::string> (trackOrKind))
    {
        std::string kind = std::get<std::string> (trackOrKind);
        return std::make_shared<HybridMediaStreamTrack> (kind);
    }
    return nullptr;
}

static auto
getDirectionFromInit (const std::optional<RTCRtpTransceiverInit> &init)
    -> rtc::Description::Direction
{
    if (!init.has_value ())
    {
        return rtc::Description::Direction::Inactive;
    }
    if (!init->direction.has_value ())
    {
        return rtc::Description::Direction::Inactive;
    }
    switch (init->direction.value ())
    {
    case RTCRtpTransceiverDirection::SENDONLY:
        return rtc::Description::Direction::SendOnly;
    case RTCRtpTransceiverDirection::RECVONLY:
        return rtc::Description::Direction::RecvOnly;
    case RTCRtpTransceiverDirection::SENDRECV:
        return rtc::Description::Direction::SendRecv;
    case RTCRtpTransceiverDirection::INACTIVE:
        return rtc::Description::Direction::Inactive;
    default:
        throw std::runtime_error ("Unknown direction");
    }
}

static auto getAttributeTrackid (const std::string &attribute) -> std::string
{
    std::regex re (R"(msid:([^\s]+)(?:\s+([^\s]+))?)");
    std::smatch match;
    if (std::regex_match (attribute, match, re) && match.size () == 3)
    {
        return match[2];
    }
    return "";
}

auto getAttributeMsid (const std::string &attribute) -> std::string
{
    std::regex re (R"(msid:([^\s]+)(?:\s+([^\s]+))?)");
    std::smatch match;
    if (std::regex_match (attribute, match, re) && match.size () == 3)
    {
        return match[1];
    }
    return "";
}

static auto getMediaTrackid (const rtc::Description::Media &media)
    -> std::string
{
    for (std::string &attr : media.attributes ())
    {
        std::string trackid = getAttributeTrackid (attr);
        if (!trackid.empty ())
        {
            return trackid;
        }
    }
    return "";
}

auto getMediaMsids (const rtc::Description::Media &media)
    -> std::vector<std::string>
{
    std::vector<std::string> msids;
    for (std::string &attr : media.attributes ())
    {
        std::string msid = getAttributeMsid (attr);
        if (!msid.empty ())
        {
            msids.push_back (msid);
        }
    }
    return msids;
}

static auto getMsidsFromInit (const std::optional<RTCRtpTransceiverInit> &init)
    -> std::vector<std::string>
{
    if (!init.has_value ())
    {
        return {};
    }
    if (!init->streams.has_value ())
    {
        return {};
    }

    std::vector<std::string> streams;
    for (const auto &streamSpec : init->streams.value ())
    {
        auto stream
            = std::dynamic_pointer_cast<HybridMediaStream> (streamSpec);
        streams.push_back (stream->getId ());
    }
    return streams;
}

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
        if (auto *media = std::get_if<0> (&mediaVar))
        {
            if ((*media)->mid () == mid)
            {
                return *media;
            }
        }
    }
    return nullptr;
}

void HybridRTCPeerConnection::offerTransceivers ()
{
    for (size_t i = 0; i < rtcRtpTransceivers.size (); i++)
    {
        std::string mid = std::to_string (i);
        auto transceiver = rtcRtpTransceivers[i];
        auto media = transceiver->offerMedia (mid);
    }
}

void HybridRTCPeerConnection::answerTransceivers (
    const rtc::Description &remoteDescription)
{
    for (int i = 0; i < remoteDescription.mediaCount (); i++)
    {
        auto remoteMediaVar = remoteDescription.media (i);
        auto *remoteMedia = std::get_if<0> (&remoteMediaVar);
        if (!remoteMedia)
        {
            continue;
        }
        auto localDir = rtc::Description::Direction::Inactive;
        switch ((*remoteMedia)->direction ())
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
        for (auto &transceiver : rtcRtpTransceivers)
        {
            if (transceiver->mid.has_value ())
            {
                continue;
            }
            if (transceiver->direction != localDir)
            {
                continue;
            }
            if (transceiver->kind != (*remoteMedia)->type ())
            {
                continue;
            }

            auto localMedia = transceiver->answerMedia (**remoteMedia);
        }
    }
}

void HybridRTCPeerConnection::startTransceivers ()
{
    for (const auto &transceiver : rtcRtpTransceivers)
    {
        auto media = getRemoteMediaFromMid (transceiver->mid.value_or (""));
        if (!media)
        {
            continue;
        }

        // create track
        std::string trackid = getMediaTrackid (*media);
        if (trackid.empty ())
        {
            trackid = uuidv4 ();
        }
        auto receiveMediaStreamTrack
            = std::make_shared<HybridMediaStreamTrack> (media->type (),
                                                        trackid);
        transceiver->hybridRtcRtpReceiver->mediaStreamTrack
            = receiveMediaStreamTrack;

        // create receive streams
        auto msids = getMediaMsids (*media);
        if (msids.empty ())
        {
            msids.push_back (uuidv4 ());
        }
        for (const std::string &msid : msids)
        {
            if (receiveMediaStreamMap[msid] == nullptr)
            {
                receiveMediaStreamMap[msid]
                    = std::make_shared<HybridMediaStream> (msid);
            }

            receiveMediaStreamMap[msid]->addTrack (receiveMediaStreamTrack);
        }

        transceiver->start (*media);

        if (trackHandler)
        {
            if (transceiver->direction
                == rtc::Description::Direction::SendOnly)
            {
                continue;
            }

            if (transceiver->direction
                == rtc::Description::Direction::Inactive)
            {
                continue;
            }

            RTCTrackEvent event;
            event.track = receiveMediaStreamTrack;
            event.streams.reserve (msids.size ());
            for (const auto &msid : msids)
            {
                event.streams.push_back (receiveMediaStreamMap[msid]);
            }
            trackHandler.value () (event);
        }
    }
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
        [this] (rtc::PeerConnection::State)
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
        [this] (const rtc::Candidate &candidate)
        {
            if (this->iceCandidateHandler)
            {
                RTCIceCandidate candidateObj{ candidate.candidate (),
                                              candidate.mid () };
                RTCPeerConnectionIceEvent event{ candidateObj };
                this->iceCandidateHandler.value () (event);
            }
        });
}

auto HybridRTCPeerConnection::setLocalDescription (
    const std::optional<RTCSessionDescriptionInit> &description)
    -> std::shared_ptr<Promise<void>>
{
    if (!description.has_value ())
    {
        return Promise<void>::resolved ();
    }

    rtc::Description sdp (description->sdp.value_or (""));
    peerConnection->setLocalDescription (sdp.type ());
    return Promise<void>::resolved ();
}

auto HybridRTCPeerConnection::setRemoteDescription (
    const RTCSessionDescriptionInit &description)
    -> std::shared_ptr<Promise<void>>
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
        answerTransceivers (remoteDescription);
    }

    peerConnection->setRemoteDescription (remoteDescription);

    startTransceivers ();

    return Promise<void>::resolved ();
}

auto HybridRTCPeerConnection::addIceCandidate (
    const std::optional<std::variant<nitro::NullType, RTCIceCandidateInit>>
        &candidate) -> std::shared_ptr<Promise<void>>
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

auto HybridRTCPeerConnection::getTransceivers ()
    -> std::vector<std::shared_ptr<HybridRTCRtpTransceiverSpec>>
{
    std::vector<std::shared_ptr<HybridRTCRtpTransceiverSpec>>
        hybridRtcRtpTransceiverSpec;
    hybridRtcRtpTransceiverSpec.reserve (rtcRtpTransceivers.size ());
    for (const auto &transceiver : rtcRtpTransceivers)
    {
        hybridRtcRtpTransceiverSpec.push_back (transceiver);
    }
    return hybridRtcRtpTransceiverSpec;
}

auto HybridRTCPeerConnection::addTransceiver (
    const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                       std::string> &trackOrKind,
    const std::optional<RTCRtpTransceiverInit> &init)
    -> std::shared_ptr<HybridRTCRtpTransceiverSpec>
{
    std::shared_ptr<HybridMediaStreamTrack> mediaStreamTrack
        = getOrCreateTrack (trackOrKind);
    rtc::Description::Direction direction = getDirectionFromInit (init);
    std::vector<std::string> msids = getMsidsFromInit (init);

    auto rtcRtpTransceiver = std::make_shared<HybridRTCRtpTransceiver> (
        peerConnection, mediaStreamTrack, direction, msids);

    rtcRtpTransceivers.push_back (rtcRtpTransceiver);
    return rtcRtpTransceiver;
}

auto HybridRTCPeerConnection::createOffer ()
    -> std::shared_ptr<Promise<RTCSessionDescriptionInit>>
{
    offerTransceivers ();

    rtc::Description sdp = peerConnection->createOffer ();

    RTCSessionDescriptionInit description;
    description.type = RTCSdpType::OFFER;
    description.sdp = sdp.generateSdp ();

    return Promise<RTCSessionDescriptionInit>::resolved (
        std::move (description));
}

auto HybridRTCPeerConnection::createAnswer ()
    -> std::shared_ptr<Promise<RTCSessionDescriptionInit>>
{
    rtc::Description sdp = peerConnection->createAnswer ();

    RTCSessionDescriptionInit description;
    description.type = RTCSdpType::ANSWER;
    description.sdp = sdp.generateSdp ();

    return Promise<RTCSessionDescriptionInit>::resolved (
        std::move (description));
}
