#pragma once
#include "Frame.hpp"

namespace FFmpeg
{
    class AudioFilter
    {
      private:
        std::recursive_mutex mutex;
        std::string filterSpec;
        AVFilterGraph *graph = nullptr;
        AVFilterContext *source = nullptr;
        AVFilterContext *sink = nullptr;

        void init (const Frame &frame);
        auto receive () -> std::vector<Frame>;

      public:
        explicit AudioFilter (std::string filterSpec
                              = "highpass=f=200,lowpass=f=3000,afftdn,"
                                "dynaudnorm=f=20:g=5:m=5:p=0.90:t=0.02");
        ~AudioFilter ();

        auto filter (const Frame &frame) -> std::vector<Frame>;
        auto flush () -> std::vector<Frame>;
    };
} // namespace FFmpeg
