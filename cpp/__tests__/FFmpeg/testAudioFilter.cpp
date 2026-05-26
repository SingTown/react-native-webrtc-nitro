#include "FFmpeg.hpp"
#include <gtest/gtest.h>

using namespace FFmpeg;

TEST (AudioFilterTest, testDefaultFilter)
{
    constexpr int sampleRate = 48000;
    constexpr int channels = 1;
    constexpr int nbSamples = 960;

    AudioFilter filter;
    Frame input (AV_SAMPLE_FMT_FLTP, sampleRate, channels, nbSamples);
    input.fillNoise ();

    std::vector<Frame> output = filter.filter (input);
    std::vector<Frame> flushed = filter.flush ();
    output.insert (output.end (), flushed.begin (), flushed.end ());

    ASSERT_FALSE (output.empty ());
    int totalSamples = 0;
    for (Frame &frame : output)
    {
        ASSERT_TRUE (frame.isAudio ());
        EXPECT_EQ (frame->sample_rate, sampleRate);
        EXPECT_EQ (frame->ch_layout.nb_channels, channels);
        EXPECT_EQ (frame->format, AV_SAMPLE_FMT_DBLP);
        totalSamples += frame->nb_samples;
    }
    EXPECT_GT (totalSamples, 0);
}

TEST (AudioFilterTest, testDefaultFilterAutoConvertsS16Input)
{
    constexpr int sampleRate = 48000;
    constexpr int channels = 1;
    constexpr int nbSamples = 960;

    AudioFilter filter;
    Frame input (AV_SAMPLE_FMT_S16, sampleRate, channels, nbSamples);
    input.fillNoise ();

    std::vector<Frame> output = filter.filter (input);
    std::vector<Frame> flushed = filter.flush ();
    output.insert (output.end (), flushed.begin (), flushed.end ());

    ASSERT_FALSE (output.empty ());
    for (Frame &frame : output)
    {
        ASSERT_TRUE (frame.isAudio ());
        EXPECT_EQ (frame->sample_rate, sampleRate);
        EXPECT_EQ (frame->ch_layout.nb_channels, channels);
        EXPECT_EQ (frame->format, AV_SAMPLE_FMT_DBLP);
    }
}
