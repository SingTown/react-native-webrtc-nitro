#include "Muxer.hpp"

using namespace FFmpeg;

Muxer::Muxer (const std::string &path,
              AVCodecID audioCodecId = AV_CODEC_ID_NONE,
              AVCodecID videoCodecId = AV_CODEC_ID_NONE)
    : audioEncoder (audioCodecId), videoEncoder (videoCodecId)
{
    std::lock_guard lock (mutex);

    int ret = avformat_alloc_output_context2 (&fmt_ctx, nullptr, nullptr,
                                              path.c_str ());
    checkError (ret, "avformat_alloc_output_context2");

    ret = avio_open (&fmt_ctx->pb, path.c_str (), AVIO_FLAG_WRITE);
    checkError (ret, "avio_open");
}

Muxer::~Muxer ()
{
    if (avio_ctx != nullptr)
    {
        av_free (avio_ctx->buffer);
        av_free (avio_ctx);
    }
    if (fmt_ctx != nullptr)
    {
        avformat_free_context (fmt_ctx);
        fmt_ctx = nullptr;
    }
}

auto Muxer::hasAudio () -> bool { return audioEncoder.encoder != nullptr; }

auto Muxer::hasVideo () -> bool { return videoEncoder.encoder != nullptr; }

auto Muxer::isAudioOpened () -> bool { return audioStream != nullptr; }

auto Muxer::isVideoOpened () -> bool { return videoStream != nullptr; }

void Muxer::tryWriteHeader ()
{
    if (hasAudio () && !isAudioOpened ())
    {
        return;
    }
    if (hasVideo () && !isVideoOpened ())
    {
        return;
    }
    if (avformat_write_header (fmt_ctx, nullptr) < 0)
    {
        return;
    }
    hasWroteHeader = true;
}

void Muxer::tryWriteAudio ()
{
    if (audioStream == nullptr)
    {
        return;
    }
    if (!hasWroteHeader)
    {
        return;
    }
    std::vector<Packet> packets = audioEncoder.receive ();
    for (Packet &packet : packets)
    {
        packet->stream_index = audioStream->index;
        int ret = av_interleaved_write_frame (fmt_ctx, packet.get ());
        checkError (ret, "av_interleaved_write_frame");
    }
}

void Muxer::tryWriteVideo ()
{
    if (videoStream == nullptr)
    {
        return;
    }
    if (!hasWroteHeader)
    {
        return;
    }
    std::vector<Packet> packets = videoEncoder.receive ();
    for (Packet &packet : packets)
    {
        packet->stream_index = videoStream->index;
        int ret = av_interleaved_write_frame (fmt_ctx, packet.get ());
        checkError (ret, "av_interleaved_write_frame");
    }
}

void Muxer::writeAudio (const Frame &frame)
{
    std::lock_guard lock (mutex);

    audioEncoder.send (frame);

    if (audioStream == nullptr)
    {
        audioStream = avformat_new_stream (fmt_ctx, audioEncoder.encoder);
        if (audioStream == nullptr)
        {
            throw std::runtime_error ("Could not create audio stream");
        }
        int ret = avcodec_parameters_from_context (audioStream->codecpar,
                                                   audioEncoder.ctx);
        checkError (ret, "avcodec_parameters_from_context");
        tryWriteHeader ();
    }
    tryWriteAudio ();
}

void Muxer::writeVideo (const Frame &frame)
{
    std::lock_guard lock (mutex);

    videoEncoder.send (frame);

    if (videoStream == nullptr)
    {
        videoStream = avformat_new_stream (fmt_ctx, videoEncoder.encoder);
        if (videoStream == nullptr)
        {
            throw std::runtime_error ("Could not create video stream");
        }
        int ret = avcodec_parameters_from_context (videoStream->codecpar,
                                                   videoEncoder.ctx);
        checkError (ret, "avcodec_parameters_from_context");
        tryWriteHeader ();
    }
    tryWriteVideo ();
}

void Muxer::stop ()
{
    std::lock_guard lock (mutex);

    if (!hasWroteHeader)
    {
        return;
    }
    if (audioStream != nullptr)
    {
        audioEncoder.flush ();
        tryWriteAudio ();
    }
    if (videoStream != nullptr)
    {
        videoEncoder.flush ();
        tryWriteVideo ();
    }

    int ret = av_write_trailer (fmt_ctx);
    checkError (ret, "av_write_trailer");
    ret = avio_closep (&fmt_ctx->pb);
    checkError (ret, "avio_closep");
}
