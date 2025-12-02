#include "Resampler.hpp"

using namespace FFmpeg;

Resampler::~Resampler ()
{
    if (swr_ctx != nullptr)
    {
        swr_free (&swr_ctx);
        swr_ctx = nullptr;
    }
}

auto Resampler::resample (const Frame &frame, AVSampleFormat outFormat,
                          int outSampleRate, int outChannels) -> Frame
{
    std::lock_guard lock (mutex);
    if (swr_ctx == nullptr)
    {
        // init
        auto inFormat = (AVSampleFormat)frame->format;
        int inSampleRate = frame->sample_rate;

        this->outFormat = outFormat;
        this->outChannels = outChannels;
        this->outSampleRate = outSampleRate;
        this->pts = frame->pts;

        AVChannelLayout outLayout;
        av_channel_layout_default (&outLayout, outChannels);
        int ret = swr_alloc_set_opts2 (&swr_ctx, &outLayout, outFormat,
                                       outSampleRate, &frame->ch_layout,
                                       inFormat, inSampleRate, 0, nullptr);
        checkError (ret, "swr_alloc_set_opts2");

        ret = swr_init (swr_ctx);
        checkError (ret, "swr_init");
    }
    int in_nb_samples = frame->nb_samples;
    int outSamples = swr_get_out_samples (swr_ctx, in_nb_samples);
    Frame dst (outFormat, outSampleRate, outChannels, outSamples, pts);

    int ret = swr_convert (swr_ctx, dst->data, dst->nb_samples, frame->data,
                           in_nb_samples);
    checkError (ret, "swr_convert");
    dst->nb_samples = ret;
    dst->pts = pts;
    pts += ret;
    return dst;
}

auto Resampler::flush () -> Frame
{
    std::lock_guard lock (mutex);
    if (swr_ctx == nullptr)
    {
        throw std::runtime_error ("Resampler not initialized");
    }
    int outSamples = swr_get_out_samples (swr_ctx, 0);
    Frame dst (outFormat, outSampleRate, outChannels, outSamples, pts);

    int ret = swr_convert (swr_ctx, dst->data, dst->nb_samples, nullptr, 0);
    checkError (ret, "swr_convert");
    dst->nb_samples = ret;
    dst->pts = pts;
    pts += ret;
    return dst;
}
