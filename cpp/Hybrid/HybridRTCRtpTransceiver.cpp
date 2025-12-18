#include "HybridRTCRtpTransceiver.hpp"
#include "rtcpjitterbuffer.hpp"

using namespace margelo::nitro::webrtc;

auto HybridRTCRtpTransceiver::offerMedia (const std::string &mid)
    -> rtc::Description::Media
{
    this->mid = mid;
    uint32_t ssrc = random () % UINT32_MAX;
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

    if (sendMsids.empty ())
    {
        media->addSSRC (ssrc, std::nullopt);
    }
    for (const auto &msid : sendMsids)
    {
        std::string trackid
            = hybridRtcRtpSender->mediaStreamTrack
                  ? hybridRtcRtpSender->mediaStreamTrack->getId ()
                  : "";
        media->addSSRC (ssrc, std::nullopt, msid, trackid);
    }
    track = peerConnection->addTrack (media.value ());
    return media.value ();
}

auto HybridRTCRtpTransceiver::answerMedia (
    const rtc::Description::Media &remoteMedia) -> rtc::Description::Media
{
    auto mid = remoteMedia.mid ();
    this->mid = mid;
    uint32_t ssrc = random () % UINT32_MAX;

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

    if (sendMsids.empty ())
    {
        media->addSSRC (ssrc, std::nullopt);
    }
    for (const auto &msid : sendMsids)
    {
        std::string trackid
            = hybridRtcRtpSender->mediaStreamTrack
                  ? hybridRtcRtpSender->mediaStreamTrack->getId ()
                  : "";
        media->addSSRC (ssrc, std::nullopt, msid, trackid);
    }
    track = peerConnection->addTrack (media.value ());
    return media.value ();
}

void HybridRTCRtpTransceiver::senderOnOpen ()
{
    if (hybridRtcRtpSender->mediaStreamTrack == nullptr)
    {
        return;
    }

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
        // create RTP configuration
        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig> (
            ssrc, track->mid (), rtpMap->payloadType,
            rtc::H265RtpPacketizer::ClockRate);
        // create packetizer
        auto packetizer
            = std::make_shared<rtc::H265RtpPacketizer> (separator, rtpConfig);

        // add RTCP SR handler
        auto srReporter = make_shared<rtc::RtcpSrReporter> (rtpConfig);
        packetizer->addToChain (srReporter);

        // add RTCP NACK handler
        auto nackResponder = make_shared<rtc::RtcpNackResponder> ();
        packetizer->addToChain (nackResponder);

        // set handler
        track->setMediaHandler (packetizer);

        avCodecId = AV_CODEC_ID_H265;
    }
    else if (rtpMap->format == "H264")
    {
        // create RTP configuration
        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig> (
            ssrc, track->mid (), rtpMap->payloadType,
            rtc::H264RtpPacketizer::ClockRate);

        // create packetizer
        auto packetizer
            = std::make_shared<rtc::H264RtpPacketizer> (separator, rtpConfig);

        // add RTCP SR handler
        auto srReporter = make_shared<rtc::RtcpSrReporter> (rtpConfig);
        packetizer->addToChain (srReporter);

        // add RTCP NACK handler
        auto nackResponder = make_shared<rtc::RtcpNackResponder> ();
        packetizer->addToChain (nackResponder);

        // set handler
        track->setMediaHandler (packetizer);

        avCodecId = AV_CODEC_ID_H264;
    }
    else if (rtpMap->format == "opus")
    {
        // create RTP configuration
        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig> (
            ssrc, track->mid (), rtpMap->payloadType,
            rtc::OpusRtpPacketizer::DefaultClockRate);

        // create packetizer
        auto packetizer = std::make_shared<rtc::OpusRtpPacketizer> (rtpConfig);

        // add RTCP SR handler
        auto srReporter = make_shared<rtc::RtcpSrReporter> (rtpConfig);
        packetizer->addToChain (srReporter);

        // add RTCP NACK handler
        auto nackResponder = make_shared<rtc::RtcpNackResponder> ();
        packetizer->addToChain (nackResponder);

        // set handler
        track->setMediaHandler (packetizer);

        avCodecId = AV_CODEC_ID_OPUS;
    }
    else
    {
        throw std::runtime_error ("Unsupported codec: " + rtpMap->format);
    }

    std::string pipeId
        = hybridRtcRtpSender->mediaStreamTrack->get_dstPipeId ();
    auto encoder = std::make_shared<FFmpeg::Encoder> (avCodecId);
    int subscriptionId = subscribe (
        { pipeId },
        [encoder, this] (const std::string &, int, const FFmpeg::Frame &frame)
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
    if (hybridRtcRtpReceiver->mediaStreamTrack == nullptr)
    {
        return;
    }

    AVCodecID avCodecId;
    auto separator = rtc::NalUnit::Separator::StartSequence;

    auto jitterbuffer = std::make_shared<rtc::RtcpJitterBuffer> ();
    auto rtcpSession = std::make_shared<rtc::RtcpReceivingSession> ();

    if (rtpMap->format == "H265")
    {
        auto depacketizer
            = std::make_shared<rtc::H265RtpDepacketizer> (separator);
        depacketizer->addToChain (jitterbuffer);
        depacketizer->addToChain (rtcpSession);

        track->chainMediaHandler (depacketizer);
        avCodecId = AV_CODEC_ID_H265;
    }
    else if (rtpMap->format == "H264")
    {
        auto depacketizer
            = std::make_shared<rtc::H264RtpDepacketizer> (separator);
        depacketizer->addToChain (jitterbuffer);
        depacketizer->addToChain (rtcpSession);

        track->chainMediaHandler (depacketizer);
        avCodecId = AV_CODEC_ID_H264;
    }
    else if (rtpMap->format == "opus")
    {
        auto depacketizer = std::make_shared<rtc::OpusRtpDepacketizer> ();
        depacketizer->addToChain (jitterbuffer);
        depacketizer->addToChain (rtcpSession);

        track->chainMediaHandler (depacketizer);
        avCodecId = AV_CODEC_ID_OPUS;
    }
    else
    {
        throw std::runtime_error ("Unsupported codec: " + rtpMap->format);
    }

    auto decoder = std::make_shared<FFmpeg::Decoder> (avCodecId);
    std::string pipeId
        = hybridRtcRtpReceiver->mediaStreamTrack->get_srcPipeId ();
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

void
HybridRTCRtpTransceiver::start (const rtc::Description::Media &remoteMedia)
{
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
