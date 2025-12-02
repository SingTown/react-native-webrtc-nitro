#include "FFmpeg.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace FFmpeg;

constexpr int SAMPLE_RATE = 48000;
constexpr int NB_SAMPLES = 960;

auto resampleAndFlush (Frame &inputFrame, AVSampleFormat outFormat,
                       int outSampleRate, int outChannels) -> Frame
{
    Resampler resampler;
    AudioFifo fifo;

    Frame frame1 = resampler.resample (inputFrame, outFormat, outSampleRate,
                                       outChannels);
    fifo.write (frame1);

    Frame frame2 = resampler.flush ();
    fifo.write (frame2);

    auto outFrame = fifo.read (-1);
    if (!outFrame.has_value ())
    {
        throw std::runtime_error ("No output frame from resampler");
    }
    return outFrame.value ();
}

TEST (ResamplerTest, testResampleFormat)
{
    Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1, NB_SAMPLES);
    inputFrame.fillNoise ();

    Frame outFrame
        = resampleAndFlush (inputFrame, AV_SAMPLE_FMT_S16, SAMPLE_RATE, 1);

    ASSERT_EQ (outFrame->format, AV_SAMPLE_FMT_S16);
    ASSERT_EQ (outFrame->nb_samples, NB_SAMPLES);

    auto *inData = (float *)inputFrame->data[0];
    auto *outData = (int16_t *)outFrame->data[0];
    for (int i = 0; i < NB_SAMPLES; ++i)
    {
        EXPECT_NEAR (outData[i] / 32767.0, inData[i], 0.01);
    }
}

TEST (ResamplerTest, testResamplePlanar)
{
    Frame inputFrame (AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, 2, NB_SAMPLES);
    inputFrame.fillNoise ();

    Frame outFrame
        = resampleAndFlush (inputFrame, AV_SAMPLE_FMT_S16P, SAMPLE_RATE, 2);
    ASSERT_EQ (outFrame->format, AV_SAMPLE_FMT_S16P);
    ASSERT_EQ (outFrame->nb_samples, NB_SAMPLES);

    auto *leftData = (float *)inputFrame->data[0];
    auto *rightData = (float *)inputFrame->data[1];
    auto *outLeft = (int16_t *)outFrame->data[0];
    auto *outRight = (int16_t *)outFrame->data[1];
    for (int i = 0; i < NB_SAMPLES; ++i)
    {
        EXPECT_NEAR (outLeft[i] / 32767.0, leftData[i], 0.01);
        EXPECT_NEAR (outRight[i] / 32767.0, rightData[i], 0.01);
    }
}

TEST (ResamplerTest, testResampleChannels12)
{
    Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1, NB_SAMPLES);
    inputFrame.fillNoise ();

    Frame outFrame
        = resampleAndFlush (inputFrame, AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 2);
    ASSERT_EQ (outFrame->format, AV_SAMPLE_FMT_FLT);
    ASSERT_EQ (outFrame->nb_samples, NB_SAMPLES);

    auto *inData = (float *)inputFrame->data[0];
    auto *outData = (float *)outFrame->data[0];
    for (int i = 0; i < NB_SAMPLES; ++i)
    {
        EXPECT_NEAR (outData[i * 2], inData[i] * 0.7071, 0.001);
        EXPECT_NEAR (outData[i * 2 + 1], inData[i] * 0.7071, 0.001);
    }
}

TEST (ResamplerTest, testResampleRate)
{
    constexpr int NB_SAMPLES_IN = 320;
    constexpr int NB_SAMPLES_OUT = 960;
    constexpr int SAMPLE_RATE_IN = 16000;
    constexpr int SAMPLE_RATE_OUT = 48000;

    Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE_IN, 1, NB_SAMPLES_IN);
    inputFrame.fillNoise ();
    auto *inData = (float *)inputFrame->data[0];

    Frame outFrame
        = resampleAndFlush (inputFrame, AV_SAMPLE_FMT_FLT, SAMPLE_RATE_OUT, 1);

    ASSERT_EQ (outFrame->nb_samples, NB_SAMPLES_OUT);
    ASSERT_EQ (outFrame->ch_layout.nb_channels, 1);
    ASSERT_EQ (outFrame->format, AV_SAMPLE_FMT_FLT);
    ASSERT_EQ (outFrame->sample_rate, SAMPLE_RATE_OUT);

    auto *outData = (float *)outFrame->data[0];
    for (int i = 0; i < inputFrame->nb_samples; i++)
    {
        float outSample = outData[i * 3];
        float inSample = inData[i];
        EXPECT_NEAR (outSample, inSample, 0.01);
    }
}

TEST (ResamplerTest, testPts)
{
    constexpr int NB_SAMPLES = 320;
    constexpr int SAMPLE_RATE_IN = 16000;
    constexpr int SAMPLE_RATE_OUT = 48000;

    Frame in1 (AV_SAMPLE_FMT_FLT, SAMPLE_RATE_IN, 1, NB_SAMPLES, 1);
    Frame in2 (AV_SAMPLE_FMT_FLT, SAMPLE_RATE_IN, 1, NB_SAMPLES,
               NB_SAMPLES + 1);
    in1.fillNoise ();
    in1.fillNoise ();

    Resampler resampler;
    std::optional<Frame> out1
        = resampler.resample (in1, AV_SAMPLE_FMT_FLT, SAMPLE_RATE_OUT, 1);
    std::optional<Frame> out2
        = resampler.resample (in2, AV_SAMPLE_FMT_FLT, SAMPLE_RATE_OUT, 1);
    ASSERT_EQ (out1.value ()->pts, 1);
    ASSERT_EQ (out2.value ()->pts, out1.value ()->nb_samples + 1);
}

TEST (ResamplerTest, testEmpty)
{
    constexpr int SAMPLE_RATE = 48000;
    Resampler resampler;
    Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1, 0, 1);
    resampler.resample (inputFrame, AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1);
    resampler.flush ();
}

TEST (ResamplerTest, testContinue)
{
    constexpr int AUDIO_SAMPLE_RATE = 48000;
    constexpr int NB_SAMPLES = 48000;
    constexpr int DELAY_MS = 1000;

    Resampler resampler;
    for (int i = 0; i < 10; ++i)
    {

        FFmpeg::Frame frame (AV_SAMPLE_FMT_FLT, AUDIO_SAMPLE_RATE, 2,
                             NB_SAMPLES);
        frame.fillNoise ();
        resampler.resample (frame, AV_SAMPLE_FMT_S16, AUDIO_SAMPLE_RATE, 2);
        std::this_thread::sleep_for (std::chrono::milliseconds (DELAY_MS));
    }
}
