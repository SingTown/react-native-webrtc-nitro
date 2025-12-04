#include "Encoder.hpp"

using namespace FFmpeg;

Encoder::Encoder (AVCodecID codecId)
{
    std::lock_guard lock (mutex);
    if (codecId == AV_CODEC_ID_NONE)
    {
        return;
    }
    encoder = avcodec_find_encoder (codecId);
    if (encoder == nullptr)
    {
        throw std::runtime_error ("Could not find encoder");
    }
}

Encoder::~Encoder ()
{
    if (ctx != nullptr)
    {
        avcodec_free_context (&ctx);
        ctx = nullptr;
    }
}

void Encoder::_init (const Frame &frame)
{
    ctx = avcodec_alloc_context3 (encoder);
    if (ctx == nullptr)
    {
        throw std::runtime_error ("Could not allocate AVCodecContext");
    }
    this->basePts = frame->pts;
    if (encoder->id == AV_CODEC_ID_H264)
    {
        ctx->codec_id = AV_CODEC_ID_H264;
        ctx->width = frame->width;
        ctx->height = frame->height;
        ctx->time_base = (AVRational){ 1, H264_SAMPLE_RATE };
        ctx->framerate = (AVRational){ H264_FPS, 1 };
        ctx->bit_rate = H264_BIT_RATE;
        ctx->gop_size = H264_GOP_SIZE;
        ctx->max_b_frames = 0;
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        ctx->profile = FF_PROFILE_H264_CONSTRAINED_BASELINE;
        ctx->color_range = AVCOL_RANGE_MPEG;
        ctx->color_primaries = AVCOL_PRI_BT709;
        ctx->color_trc = AVCOL_TRC_BT709;
        ctx->colorspace = AVCOL_SPC_BT709;
    }
    else if (encoder->id == AV_CODEC_ID_H265)
    {
        ctx->codec_id = AV_CODEC_ID_H265;
        ctx->width = frame->width;
        ctx->height = frame->height;
        ctx->time_base = (AVRational){ 1, H265_SAMPLE_RATE };
        ctx->framerate = (AVRational){ H265_FPS, 1 };
        ctx->bit_rate = H265_BIT_RATE;
        ctx->gop_size = H265_GOP_SIZE;
        ctx->max_b_frames = 0;
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        ctx->profile = FF_PROFILE_HEVC_MAIN;
        ctx->color_range = AVCOL_RANGE_MPEG;
        ctx->color_primaries = AVCOL_PRI_BT709;
        ctx->color_trc = AVCOL_TRC_BT709;
        ctx->colorspace = AVCOL_SPC_BT709;
    }
    else if (encoder->id == AV_CODEC_ID_OPUS)
    {
        ctx->codec_id = AV_CODEC_ID_OPUS;
        ctx->time_base = (AVRational){ 1, OPUS_SAMPLE_RATE };
        ctx->sample_rate = OPUS_SAMPLE_RATE;
        ctx->bit_rate = OPUS_BIT_RATE;
        ctx->sample_fmt = AV_SAMPLE_FMT_FLT;
        av_channel_layout_default (&ctx->ch_layout, 2);
    }
    else if (encoder->id == AV_CODEC_ID_AAC)
    {
        ctx->codec_id = AV_CODEC_ID_AAC;
        ctx->time_base = (AVRational){ 1, AAC_SAMPLE_RATE };
        ctx->sample_rate = AAC_SAMPLE_RATE;
        ctx->bit_rate = AAC_BIT_RATE;
        ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        av_channel_layout_default (&ctx->ch_layout, 2);
    }
    else if (encoder->id == AV_CODEC_ID_PNG)
    {
        ctx->codec_id = AV_CODEC_ID_PNG;
        ctx->time_base = (AVRational){ 1, PNG_SAMPLE_RATE };
        ctx->width = frame->width;
        ctx->height = frame->height;
        ctx->pix_fmt = AV_PIX_FMT_RGBA;
    }
    else
    {
        throw std::runtime_error ("Unsupported encoder"
                                  + std::to_string (encoder->id));
    }
    ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    int ret = avcodec_open2 (ctx, encoder, nullptr);
    checkError (ret, "avcodec_open2");
}

void Encoder::_receive ()
{
    while (true)
    {
        Packet packet;
        int ret = avcodec_receive_packet (ctx, packet.get ());
        if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        checkError (ret, "avcodec_receive_packet");
        buffer.push_back (packet);
    }
}

void Encoder::send (const Frame &frame)
{
    std::lock_guard lock (mutex);
    if (ctx == nullptr)
    {
        _init (frame);
    }

    std::vector<Frame> frames;

    if (encoder->id == AV_CODEC_ID_OPUS)
    {
        Frame resampled_frame = resampler.resample (frame, AV_SAMPLE_FMT_FLT,
                                                    AAC_SAMPLE_RATE, 2);
        fifo.write (resampled_frame);

        while (std::optional<Frame> frame = fifo.read (OPUS_NB_SAMPLES))
        {
            frames.push_back (frame.value ());
        }
    }
    else if (encoder->id == AV_CODEC_ID_AAC)
    {
        Frame resampled_frame = resampler.resample (frame, AV_SAMPLE_FMT_FLTP,
                                                    OPUS_SAMPLE_RATE, 2);
        fifo.write (resampled_frame);
        while (std::optional<Frame> frame = fifo.read (AAC_NB_SAMPLES))
        {
            frames.push_back (frame.value ());
        }
    }
    else if (encoder->id == AV_CODEC_ID_H264
             || encoder->id == AV_CODEC_ID_H265)
    {
        frames.push_back (scaler.scale (frame, AV_PIX_FMT_YUV420P,
                                        frame->width, frame->height));
    }
    else if (encoder->id == AV_CODEC_ID_PNG)
    {
        frames.push_back (scaler.scale (frame, AV_PIX_FMT_RGBA, frame->width,
                                        frame->height));
    }
    else
    {
        throw std::runtime_error ("Unsupported encoder"
                                  + std::to_string (encoder->id));
    }

    for (Frame &frame : frames)
    {
        frame->pts -= this->basePts;
        int ret = avcodec_send_frame (ctx, frame.get ());
        checkError (ret, "avcodec_send_frame");
        _receive ();
    }
}

auto Encoder::receive () -> std::vector<Packet>
{
    std::lock_guard lock (mutex);
    if (ctx == nullptr)
    {
        return {};
    }

    std::vector<Packet> result = std::move (buffer);
    buffer.clear ();
    return result;
}

void Encoder::flush ()
{
    std::lock_guard lock (mutex);
    if (ctx == nullptr)
    {
        return;
    }

    int ret = avcodec_send_frame (ctx, nullptr);
    checkError (ret, "avcodec_send_frame");

    _receive ();
}
