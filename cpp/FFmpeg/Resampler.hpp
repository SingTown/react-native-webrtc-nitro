#pragma once
#include "Frame.hpp"

namespace FFmpeg
{
    class Resampler
    {
      private:
        std::recursive_mutex mutex;
        SwrContext *swr_ctx = nullptr;

        AVSampleFormat outFormat = AV_SAMPLE_FMT_NONE;
        int outChannels = 0;
        int outSampleRate = 0;
        int64_t pts = -1;

      public:
        ~Resampler ();

        auto resample (const Frame &frame, AVSampleFormat outFormat,
                       int outSampleRate, int outChannels) -> Frame;
        auto flush () -> Frame;
    };
} // namespace FFmpeg
