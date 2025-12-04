#include "HybridRTCRtpTransceiver.hpp"
#include "FFmpeg.hpp"
#include "FramePipe.hpp"

using namespace margelo::nitro::webrtc;

void HybridRTCRtpTransceiver::senderOnOpen ()
{
    if (sender->track == nullptr)
    {
        return;
    }

    const size_t mtu = 1200;
    auto ssrcs = track->description ().getSSRCs ();
    if (ssrcs.size () != 1)
    {
        throw std::runtime_error ("Expected exactly one SSRC");
    }
    rtc::SSRC ssrc = ssrcs[0];

    AVCodecID avCodecId;
    auto separator = rtc::NalUnit::Separator::StartSequence;
    if (rtpMap->format == "H265")
    {
        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig> (
            ssrc, track->mid (), rtpMap->payloadType,
            rtc::H265RtpPacketizer::ClockRate);
        auto packetizer = std::make_shared<rtc::H265RtpPacketizer> (
            separator, rtpConfig, mtu);
        track->chainMediaHandler (packetizer);
        avCodecId = AV_CODEC_ID_H265;
    }
    else if (rtpMap->format == "H264")
    {
        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig> (
            ssrc, track->mid (), rtpMap->payloadType,
            rtc::H264RtpPacketizer::ClockRate);
        auto packetizer = std::make_shared<rtc::H264RtpPacketizer> (
            separator, rtpConfig, mtu);
        track->chainMediaHandler (packetizer);
        avCodecId = AV_CODEC_ID_H264;
    }
    else if (rtpMap->format == "opus")
    {
        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig> (
            ssrc, track->mid (), rtpMap->payloadType,
            rtc::OpusRtpPacketizer::DefaultClockRate);
        auto packetizer = std::make_shared<rtc::OpusRtpPacketizer> (rtpConfig);
        track->chainMediaHandler (packetizer);
        avCodecId = AV_CODEC_ID_OPUS;
    }
    else
    {
        throw std::runtime_error ("Unsupported codec: " + rtpMap->format);
    }

    std::string pipeId = sender->track->dstPipeId;
    auto encoder = std::make_shared<FFmpeg::Encoder> (avCodecId);
    int subscriptionId = subscribe (
        { pipeId },
        [encoder, this] (std::string, int, const FFmpeg::Frame &frame)
        {
            encoder->send (frame);
            auto packets = encoder->receive ();
            for (auto packet : packets)
            {
                if (!track->isOpen ())
                {
                    return;
                }
                track->sendFrame ((const rtc::byte *)packet->data,
                                  packet->size, packet->pts);
            }
        });
    track->onClosed ([subscriptionId] () { unsubscribe (subscriptionId); });
}

void HybridRTCRtpTransceiver::receiverOnOpen ()
{
    if (receiver->track == nullptr)
    {
        return;
    }

    AVCodecID avCodecId;
    auto separator = rtc::NalUnit::Separator::StartSequence;
    if (rtpMap->format == "H265")
    {
        auto depacketizer
            = std::make_shared<rtc::H265RtpDepacketizer> (separator);
        track->chainMediaHandler (depacketizer);
        avCodecId = AV_CODEC_ID_H265;
    }
    else if (rtpMap->format == "H264")
    {
        auto depacketizer
            = std::make_shared<rtc::H264RtpDepacketizer> (separator);
        track->chainMediaHandler (depacketizer);
        avCodecId = AV_CODEC_ID_H264;
    }
    else if (rtpMap->format == "opus")
    {
        auto depacketizer = std::make_shared<rtc::OpusRtpDepacketizer> ();
        track->chainMediaHandler (depacketizer);
        avCodecId = AV_CODEC_ID_OPUS;
    }
    else
    {
        throw std::runtime_error ("Unsupported codec: " + rtpMap->format);
    }

    auto decoder = std::make_shared<FFmpeg::Decoder> (avCodecId);
    std::string pipeId = receiver->track->srcPipeId;
    track->onFrame (
        [decoder, pipeId] (rtc::binary binary, rtc::FrameInfo info)
        {
            FFmpeg::Packet packet (binary.size ());
            memcpy (packet->data,
                    reinterpret_cast<const void *> (binary.data ()),
                    binary.size ());
            packet->pts = info.timestamp;
            packet->dts = info.timestamp;

            decoder->send (packet);
            auto frames = decoder->receive ();
            for (auto &frame : frames)
            {
                publish (pipeId, frame);
            }
        });
}

static std::shared_ptr<HybridMediaStreamTrack> getOrCreateTrack (
    const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                       std::string> &trackOrKind)
{
    if (std::holds_alternative<std::shared_ptr<HybridMediaStreamTrackSpec>> (
            trackOrKind))
    {
        auto mediaStreamTrackSpec
            = std::get<std::shared_ptr<HybridMediaStreamTrackSpec>> (
                trackOrKind);
        auto mediaStreamTrack
            = std::dynamic_pointer_cast<HybridMediaStreamTrack> (
                mediaStreamTrackSpec);
        return mediaStreamTrack;
    }
    else if (std::holds_alternative<std::string> (trackOrKind))
    {
        auto track = std::make_shared<HybridMediaStreamTrack> ();
        track->kind = std::get<std::string> (trackOrKind);
        return track;
    }
    return nullptr;
}

HybridRTCRtpTransceiver::HybridRTCRtpTransceiver (
    const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                       std::string> &trackOrKind,
    const std::optional<RTCRtpTransceiverInit> &init)
    : HybridObject (TAG), HybridRTCRtpTransceiverSpec ()
{
    sender = std::make_shared<HybridRTCRtpSender> ();
    receiver = std::make_shared<HybridRTCRtpReceiver> ();

    std::vector<std::shared_ptr<HybridMediaStream>> streams;
    if (init.has_value ())
    {
        if (init->direction.has_value ())
        {
            setDirection (init->direction.value ());
        }
        if (init->streams.has_value ())
        {
            for (const auto &streamSpec : init->streams.value ())
            {
                auto stream = std::dynamic_pointer_cast<HybridMediaStream> (
                    streamSpec);
                streams.push_back (stream);
            }
        }
    }

    auto mediaStreamTrack = getOrCreateTrack (trackOrKind);
    if (direction == rtc::Description::Direction::SendOnly
        || direction == rtc::Description::Direction::SendRecv)
    {
        sender->streams = streams;
        sender->track = mediaStreamTrack;
    }
    if (direction == rtc::Description::Direction::RecvOnly
        || direction == rtc::Description::Direction::SendRecv)
    {
        receiver->streams = streams;
        receiver->track = mediaStreamTrack;
    }
}

rtc::Description::Media
HybridRTCRtpTransceiver::offerMedia (const std::string &mid)
{
    this->mid = mid;
    uint32_t ssrc = random () % UINT32_MAX;
    std::string trackid = sender->track ? sender->track->id : "";
    std::string kind = getKind ();
    std::optional<rtc::Description::Media> media = std::nullopt;
    if (kind == "audio")
    {
        rtc::Description::Audio audio (mid, direction);
        audio.addOpusCodec (111);
        media = audio;
    }
    else if (kind == "video")
    {
        rtc::Description::Video video (mid, direction);
        video.addH265Codec (104, "level-id=93;"
                                 "profile-id=1;"
                                 "tier-flag=0;"
                                 "tx-mode=SRST");
        video.addH264Codec (96, "profile-level-id=42e01f;"
                                "packetization-mode=1;"
                                "level-asymmetry-allowed=1");
        media = video;
    }
    else
    {
        throw std::invalid_argument ("Unsupported media type: " + kind);
    }
    if (sender->streams.empty ())
    {
        media->addSSRC (ssrc, std::nullopt);
    }
    for (const auto &ms : sender->streams)
    {
        media->addSSRC (ssrc, std::nullopt, ms->id, trackid);
    }
    return media.value ();
}

rtc::Description::Media HybridRTCRtpTransceiver::answerMedia (
    const rtc::Description::Media &remoteMedia)
{
    std::string kind = getKind ();
    auto mid = remoteMedia.mid ();
    this->mid = mid;
    uint32_t ssrc = random () % UINT32_MAX;
    std::string trackid = sender->track ? sender->track->id : "";

    std::optional<rtc::Description::Media> media = std::nullopt;
    if (kind == "audio")
    {
        rtc::Description::Audio audio (mid, direction);
        media = audio;
    }
    else if (kind == "video")
    {
        rtc::Description::Video video (mid, direction);
        media = video;
    }
    else
    {
        throw std::invalid_argument ("Unsupported media type: " + kind);
    }

    for (auto pt : remoteMedia.payloadTypes ())
    {
        auto rtpMap = remoteMedia.rtpMap (pt);
        if (rtpMap->format == "H264" || rtpMap->format == "H265"
            || rtpMap->format == "opus")
        {
            media->addRtpMap (*rtpMap);
            break;
        }
    }

    if (sender->streams.empty ())
    {
        media->addSSRC (ssrc, std::nullopt);
    }
    for (const auto &ms : sender->streams)
    {
        media->addSSRC (ssrc, std::nullopt, ms->id, trackid);
    }
    return media.value ();
}

void
HybridRTCRtpTransceiver::start (const rtc::Description::Media &remoteMedia)
{
    receiver->receive (remoteMedia);

    auto localMedia = track->description ();
    for (auto remotePt : remoteMedia.payloadTypes ())
    {
        if (localMedia.hasPayloadType (remotePt))
        {
            rtpMap = *remoteMedia.rtpMap (remotePt);
            break;
        }
    }
    if (!rtpMap.has_value ())
    {
        return;
    }

    track->onOpen (
        [this] ()
        {
            senderOnOpen ();

            receiverOnOpen ();
        });
}

std::string HybridRTCRtpTransceiver::getKind ()
{
    if (sender->track != nullptr)
    {
        return sender->track->kind;
    }
    if (receiver->track != nullptr)
    {
        return receiver->track->kind;
    }
    throw std::runtime_error ("No track available to determine kind");
}

RTCRtpTransceiverDirection HybridRTCRtpTransceiver::getDirection ()
{
    switch (direction)
    {
    case rtc::Description::Direction::SendOnly:
        return RTCRtpTransceiverDirection::SENDONLY;
    case rtc::Description::Direction::RecvOnly:
        return RTCRtpTransceiverDirection::RECVONLY;
    case rtc::Description::Direction::SendRecv:
        return RTCRtpTransceiverDirection::SENDRECV;
    case rtc::Description::Direction::Inactive:
        return RTCRtpTransceiverDirection::INACTIVE;
    default:
        throw std::runtime_error ("Unknown direction");
    }
}

void
HybridRTCRtpTransceiver::setDirection (RTCRtpTransceiverDirection direction)
{
    switch (direction)
    {
    case RTCRtpTransceiverDirection::SENDONLY:
        this->direction = rtc::Description::Direction::SendOnly;
        break;
    case RTCRtpTransceiverDirection::RECVONLY:
        this->direction = rtc::Description::Direction::RecvOnly;
        break;
    case RTCRtpTransceiverDirection::SENDRECV:
        this->direction = rtc::Description::Direction::SendRecv;
        break;
    case RTCRtpTransceiverDirection::INACTIVE:
        this->direction = rtc::Description::Direction::Inactive;
        break;
    default:
        throw std::runtime_error ("Unknown direction");
    }
}

std::variant<margelo::nitro::NullType, std::string>
HybridRTCRtpTransceiver::getMid ()
{
    if (mid.has_value ())
    {
        return mid.value ();
    }
    else
    {
        return nitro::null;
    }
}

std::shared_ptr<HybridRTCRtpSenderSpec> HybridRTCRtpTransceiver::getSender ()
{
    return sender;
}

std::shared_ptr<HybridRTCRtpReceiverSpec>
HybridRTCRtpTransceiver::getReceiver ()
{
    return receiver;
}
