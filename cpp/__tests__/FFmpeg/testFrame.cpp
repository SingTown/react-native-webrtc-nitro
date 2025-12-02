#include "FFmpeg.hpp"
#include <gtest/gtest.h>

using namespace FFmpeg;

constexpr int AUDIO_SAMPLE_RATE = 48000;
constexpr int NB_SAMPLES = 960;
constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;

TEST (FrameTest, testAudioFrame)
{
    const AVRational timebase = { 1, AUDIO_SAMPLE_RATE };

    int64_t ptsStart = currentPts (timebase);
    Frame frame (AV_SAMPLE_FMT_FLT, AUDIO_SAMPLE_RATE, 2, NB_SAMPLES);
    int64_t ptsEnd = currentPts (timebase);

    ASSERT_EQ (frame->format, AV_SAMPLE_FMT_FLT);
    ASSERT_EQ (frame->sample_rate, AUDIO_SAMPLE_RATE);
    ASSERT_EQ (frame->ch_layout.nb_channels, 2);
    ASSERT_EQ (frame->nb_samples, NB_SAMPLES);
    ASSERT_GE (frame->pts, ptsStart);
    ASSERT_LE (frame->pts, ptsEnd);
}

TEST (FrameTest, testVideoFrame)
{
    constexpr int VIDEO_SAMPLE_RATE = 90000;
    const AVRational timebase = { 1, VIDEO_SAMPLE_RATE };

    int64_t ptsStart = currentPts (timebase);
    Frame frame (AV_PIX_FMT_YUV420P, WIDTH, HEIGHT);
    int64_t ptsEnd = currentPts (timebase);

    ASSERT_EQ (frame->format, AV_PIX_FMT_YUV420P);
    ASSERT_EQ (frame->width, WIDTH);
    ASSERT_EQ (frame->height, HEIGHT);
    ASSERT_GE (frame->pts, ptsStart);
    ASSERT_LE (frame->pts, ptsEnd);
}

TEST (NoiseTest, testAudioS16)
{
    Frame frame (AV_SAMPLE_FMT_S16, AUDIO_SAMPLE_RATE, 2, NB_SAMPLES);
    frame.fillNoise ();
}

TEST (NoiseTest, testAudioS16P)
{
    Frame frame (AV_SAMPLE_FMT_S16P, AUDIO_SAMPLE_RATE, 2, NB_SAMPLES);
    frame.fillNoise ();
}

TEST (NoiseTest, testAudioFLT)
{
    Frame frame (AV_SAMPLE_FMT_FLT, AUDIO_SAMPLE_RATE, 2, NB_SAMPLES);
    frame.fillNoise ();
}

TEST (NoiseTest, testAudioFLTP)
{
    Frame frame (AV_SAMPLE_FMT_FLTP, AUDIO_SAMPLE_RATE, 2, NB_SAMPLES);
    frame.fillNoise ();
}

TEST (NoiseTest, testVideoRGB24)
{
    Frame frame (AV_PIX_FMT_RGB24, WIDTH, HEIGHT);
    frame.fillNoise ();
}

TEST (NoiseTest, testVideoRGBA)
{
    Frame frame (AV_PIX_FMT_RGBA, WIDTH, HEIGHT);
    frame.fillNoise ();
}

TEST (NoiseTest, testVideoARGB)
{
    Frame frame (AV_PIX_FMT_ARGB, WIDTH, HEIGHT);
    frame.fillNoise ();
}

TEST (NoiseTest, testVideoNV12)
{
    Frame frame (AV_PIX_FMT_NV12, WIDTH, HEIGHT);
    frame.fillNoise ();
}

TEST (NoiseTest, testVideoYUV420P)
{
    Frame frame (AV_PIX_FMT_YUV420P, WIDTH, HEIGHT);
    frame.fillNoise ();
}
