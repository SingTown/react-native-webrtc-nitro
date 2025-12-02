#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include <gtest/gtest.h>

constexpr int SAMPLE_RATE = 48000;
constexpr int NB_SAMPLES = 960;
constexpr int CHANNELS = 2;

TEST (FramePipeTest, testCallback)
{
    bool called = false;
    int subscriptionIdSet = -1;
    FrameCallback callback
        = [&called, &subscriptionIdSet] (const std::string &pipeId,
                                         int subscriptionId,
                                         const FFmpeg::Frame &)
    {
        ASSERT_EQ (pipeId, std::string ("test_pipe"));
        subscriptionIdSet = subscriptionId;
        called = true;
    };
    int subscriptionId = subscribe ({ "test_pipe" }, callback);

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);

    ASSERT_FALSE (called);
    publish ("test_pipe", frame);
    ASSERT_TRUE (called);
    ASSERT_EQ (subscriptionIdSet, subscriptionId);

    unsubscribe (subscriptionId);
}

TEST (FramePipeTest, testCallbackNotMatch)
{
    bool called = false;
    FrameCallback callback
        = [&called] (const std::string &, int, const FFmpeg::Frame &)
    { called = true; };
    int subscriptionId = subscribe ({ "test_pipe" }, callback);

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);

    ASSERT_FALSE (called);
    publish ("test_pipe2", frame);
    ASSERT_FALSE (called);

    unsubscribe (subscriptionId);
}

TEST (FramePipeTest, testCleanup)
{
    bool cleanedUp = false;
    int subscriptionIdSet = -1;
    FrameCallback callback
        = [] (const std::string &, int, const FFmpeg::Frame &frame) {};
    CleanupCallback cleanup
        = [&cleanedUp, &subscriptionIdSet] (int subscriptionId)
    {
        cleanedUp = true;
        subscriptionIdSet = subscriptionId;
    };
    int subscriptionId = subscribe ({ "test_pipe" }, callback, cleanup);

    ASSERT_FALSE (cleanedUp);
    unsubscribe (subscriptionId);
    ASSERT_TRUE (cleanedUp);
    ASSERT_EQ (subscriptionIdSet, subscriptionId);
}

TEST (FramePipeTest, testUnsubscribe)
{
    int count = 0;
    FrameCallback callback = [&count] (const std::string &pipeId, int subId,
                                       const FFmpeg::Frame &) { count += 1; };

    int subscriptionId = subscribe ({ "test_pipe" }, callback);

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);
    publish ("test_pipe", frame);
    publish ("test_pipe", frame);
    publish ("test_pipe", frame);
    publish ("test_pipe", frame);
    unsubscribe (subscriptionId);
    ASSERT_EQ (count, 4);
}

TEST (FramePipeTest, testUnsubscribeInCallback)
{
    int count = 0;
    FrameCallback callback =
        [&count] (const std::string &pipeId, int subId, const FFmpeg::Frame &)
    {
        count += 1;
        unsubscribe (subId);
    };

    int subscriptionId = subscribe ({ "test_pipe" }, callback);

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);
    publish ("test_pipe", frame);
    publish ("test_pipe", frame);
    publish ("test_pipe", frame);
    publish ("test_pipe", frame);
    unsubscribe (subscriptionId);
    ASSERT_EQ (count, 1);
}
