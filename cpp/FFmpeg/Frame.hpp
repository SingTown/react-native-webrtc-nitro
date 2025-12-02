#pragma once
#include "Common.hpp"

namespace FFmpeg
{
    class Frame
    {
      private:
        std::shared_ptr<AVFrame> frame;
        static constexpr int VIDEO_SAMPLE_RATE = 90000;
        static constexpr int FRAME_ALIGN = 32;
        void fillNoiseAudioFLT ();
        void fillNoiseAudioFLTP ();
        void fillNoiseAudioS16 ();
        void fillNoiseAudioS16P ();
        void fillNoiseVideoRGB ();
        void fillNoiseVideoNV12 ();
        void fillNoiseVideoYUV420P ();

      public:
        Frame ();
        Frame (AVPixelFormat format, int width, int height, int64_t pts = -1);
        Frame (AVSampleFormat format, int sampleRate, int channels,
               int nb_samples, int64_t pts = -1);

        auto isAudio () -> bool;
        auto isVideo () -> bool;

        void fillNoise ();

        auto operator->() -> AVFrame * { return frame.get (); }

        auto operator->() const -> const AVFrame * { return frame.get (); }

        auto get () -> AVFrame * { return frame.get (); }

        [[nodiscard]] auto get () const -> const AVFrame *
        {
            return frame.get ();
        }
    };
} // namespace FFmpeg
