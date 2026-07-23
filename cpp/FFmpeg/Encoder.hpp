#pragma once
#include "AudioFifo.hpp"
#include "Frame.hpp"
#include "Packet.hpp"
#include "Resampler.hpp"
#include "Scaler.hpp"

namespace FFmpeg
{
    class Encoder
    {
      private:
        std::recursive_mutex mutex;
        Scaler scaler;
        Resampler resampler;
        AudioFifo fifo;
        int64_t basePts = -1;
        std::vector<Packet> buffer;

        static constexpr int AAC_NB_SAMPLES = 1024;
        static constexpr int AAC_SAMPLE_RATE = 48000;
        static constexpr int AAC_BIT_RATE = 128000;

        static constexpr int OPUS_NB_SAMPLES = 960;
        static constexpr int OPUS_SAMPLE_RATE = 48000;
        static constexpr int OPUS_BIT_RATE = 64000;

        static constexpr int PNG_SAMPLE_RATE = 90000;

        static constexpr int H264_SAMPLE_RATE = 90000;
        static constexpr int H264_GOP_SIZE = 60;
        static constexpr int H264_BIT_RATE = 500000;
        static constexpr int H264_FPS = 20;

        static constexpr int H265_SAMPLE_RATE = 90000;
        static constexpr int H265_GOP_SIZE = 60;
        static constexpr int H265_BIT_RATE = 500000;
        static constexpr int H265_FPS = 20;

        void _init (const Frame &frame);
        void _fallback (const Frame &frame);
        void _receive ();

      public:
        const AVCodec *encoder = nullptr;
        AVCodecContext *ctx = nullptr;

        Encoder (AVCodecID codecId);
        ~Encoder ();

        void send (const Frame &frame);
        auto receive () -> std::vector<Packet>;
        void flush ();
    };
} // namespace FFmpeg
