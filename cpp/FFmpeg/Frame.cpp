#include "Frame.hpp"
#include <random>

using namespace FFmpeg;

Frame::Frame ()
{
    frame = std::shared_ptr<AVFrame> (av_frame_alloc (), [] (AVFrame *frame)
                                      { av_frame_free (&frame); });
    if (!frame)
    {
        throw std::runtime_error ("Could not allocate AVFrame");
    }
}

Frame::Frame (AVSampleFormat format, int sampleRate, int channels,
              int nb_samples, int64_t pts)
    : Frame ()
{
    AVRational time_base = { 1, sampleRate };
    if (pts == -1)
    {
        pts = currentPts (time_base);
    }

    frame->format = format;
    frame->time_base = time_base;
    frame->pts = pts;
    frame->sample_rate = sampleRate;
    frame->nb_samples = nb_samples;
    av_channel_layout_default (&frame->ch_layout, channels);
    if (frame->nb_samples > 0)
    {
        int ret = av_frame_get_buffer (frame.get (), FRAME_ALIGN);
        checkError (ret, "av_frame_get_buffer");
    }
}

Frame::Frame (AVPixelFormat format, int width, int height, int64_t pts)
    : Frame ()
{
    AVRational time_base = { 1, VIDEO_SAMPLE_RATE };
    if (pts == -1)
    {
        pts = currentPts (time_base);
    }

    frame->format = format;
    frame->time_base = (AVRational){ 1, VIDEO_SAMPLE_RATE };
    frame->pts = pts;
    frame->width = width;
    frame->height = height;

    int ret = av_frame_get_buffer (frame.get (), 1);
    checkError (ret, "av_frame_get_buffer");
}

auto Frame::isAudio () -> bool
{
    return frame->width == 0 && frame->height == 0 && frame->nb_samples > 0;
}

auto Frame::isVideo () -> bool
{
    return frame->width > 0 && frame->height > 0 && frame->nb_samples == 0;
}

void Frame::fillNoiseAudioFLT ()
{
    constexpr float MIN_NUMBER = -1.0F;
    constexpr float MAX_NUMBER = 1.0F;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_real_distribution<float> dist (MIN_NUMBER, MAX_NUMBER);

    int channels = frame->ch_layout.nb_channels;
    auto *inData = (float *)frame->data[0];
    for (int i = 0; i < frame->nb_samples * channels; ++i)
    {
        inData[i] = dist (rng);
    }
}

void Frame::fillNoiseAudioFLTP ()
{
    constexpr float MIN_NUMBER = -1.0F;
    constexpr float MAX_NUMBER = 1.0F;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_real_distribution<float> dist (MIN_NUMBER, MAX_NUMBER);

    int channels = frame->ch_layout.nb_channels;
    for (int ch = 0; ch < channels; ++ch)
    {
        auto *inData = (float *)frame->data[ch];
        for (int i = 0; i < frame->nb_samples; ++i)
        {
            inData[i] = dist (rng);
        }
    }
}

void Frame::fillNoiseAudioS16 ()
{
    constexpr int16_t MIN_NUMBER = -32768;
    constexpr int16_t MAX_NUMBER = 32767;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_int_distribution<int16_t> dist (MIN_NUMBER,
                                                        MAX_NUMBER);

    int channels = frame->ch_layout.nb_channels;
    auto *inData = (int16_t *)frame->data[0];
    for (int i = 0; i < frame->nb_samples * channels; ++i)
    {
        inData[i] = dist (rng);
    }
}

void Frame::fillNoiseAudioS16P ()
{
    constexpr int16_t MIN_NUMBER = -32768;
    constexpr int16_t MAX_NUMBER = 32767;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_int_distribution<int16_t> dist (MIN_NUMBER,
                                                        MAX_NUMBER);

    int channels = frame->ch_layout.nb_channels;
    for (int ch = 0; ch < channels; ++ch)
    {
        auto *inData = (int16_t *)frame->data[ch];
        for (int i = 0; i < frame->nb_samples; ++i)
        {
            inData[i] = dist (rng);
        }
    }
}

void Frame::fillNoiseVideoRGB ()
{
    constexpr uint8_t MIN_NUMBER = 0;
    constexpr uint8_t MAX_NUMBER = 255;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_int_distribution<uint8_t> dist (MIN_NUMBER,
                                                        MAX_NUMBER);

    int planeCount = av_pix_fmt_count_planes ((AVPixelFormat)frame->format);
    for (int plane = 0; plane < planeCount; ++plane)
    {
        uint8_t *data = frame->data[plane];
        int linesize = frame->linesize[plane];
        for (int y = 0; y < frame->height; ++y)
        {
            uint8_t *line = data + y * linesize;
            for (int x = 0; x < linesize; ++x)
            {
                line[x] = dist (rng);
            }
        }
    }
}

void Frame::fillNoiseVideoNV12 ()
{
    constexpr uint8_t MIN_NUMBER = 0;
    constexpr uint8_t MAX_NUMBER = 255;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_int_distribution<uint8_t> dist (MIN_NUMBER,
                                                        MAX_NUMBER);
    // Y plane
    {
        uint8_t *data = frame->data[0];
        int linesize = frame->linesize[0];
        for (int y = 0; y < frame->height; ++y)
        {
            uint8_t *line = data + y * linesize;
            for (int x = 0; x < linesize; ++x)
            {
                line[x] = dist (rng);
            }
        }
    }
    // UV plane
    {
        uint8_t *data = frame->data[1];
        int linesize = frame->linesize[1];
        for (int y = 0; y < frame->height / 2; ++y)
        {
            uint8_t *line = data + y * linesize;
            for (int x = 0; x < linesize; ++x)
            {
                line[x] = dist (rng);
            }
        }
    }
}

void Frame::fillNoiseVideoYUV420P ()
{
    constexpr uint8_t MIN_NUMBER = 0;
    constexpr uint8_t MAX_NUMBER = 255;
    std::mt19937 rng (std::random_device{}());
    static std::uniform_int_distribution<uint8_t> dist (MIN_NUMBER,
                                                        MAX_NUMBER);
    // Y plane
    {
        uint8_t *data = frame->data[0];
        int linesize = frame->linesize[0];
        for (int y = 0; y < frame->height; ++y)
        {
            uint8_t *line = data + y * linesize;
            for (int x = 0; x < linesize; ++x)
            {
                line[x] = dist (rng);
            }
        }
    }
    // U plane
    {
        uint8_t *data = frame->data[1];
        int linesize = frame->linesize[1];
        for (int y = 0; y < frame->height / 2; ++y)
        {
            uint8_t *line = data + y * linesize;
            for (int x = 0; x < linesize; ++x)
            {
                line[x] = dist (rng);
            }
        }
    }
    // V plane
    {
        uint8_t *data = frame->data[2];
        int linesize = frame->linesize[2];
        for (int y = 0; y < frame->height / 2; ++y)
        {
            uint8_t *line = data + y * linesize;
            for (int x = 0; x < linesize; ++x)
            {
                line[x] = dist (rng);
            }
        }
    }
}

void Frame::fillNoise ()
{
    if (isAudio ())
    {
        switch (frame->format)
        {
        case AV_SAMPLE_FMT_FLT:
            fillNoiseAudioFLT ();
            return;
        case AV_SAMPLE_FMT_FLTP:
            fillNoiseAudioFLTP ();
            return;
        case AV_SAMPLE_FMT_S16:
            fillNoiseAudioS16 ();
            return;
        case AV_SAMPLE_FMT_S16P:
            fillNoiseAudioS16P ();
            return;
        default:
            break;
        }
    }
    else if (isVideo ())
    {
        switch (frame->format)
        {
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_ABGR:
        case AV_PIX_FMT_BGRA:
            fillNoiseVideoRGB ();
            return;
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
            fillNoiseVideoNV12 ();
            return;
        case AV_PIX_FMT_YUV420P:
            fillNoiseVideoYUV420P ();
            return;
        default:
            break;
        }
    }
    throw std::runtime_error ("Unsupported format in fillNoise");
}
