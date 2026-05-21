#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

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

TEST (FramePipeTest, testUnsubscribeWaitsForDeferredCleanup)
{
    std::atomic<bool> callbackStarted{ false };
    std::atomic<bool> allowCallbackFinish{ false };
    std::atomic<bool> cleanedUp{ false };
    std::atomic<bool> unsubscribeReturned{ false };

    FrameCallback callback
        = [&callbackStarted, &allowCallbackFinish] (const std::string &, int,
                                                    const FFmpeg::Frame &)
    {
        callbackStarted = true;
        while (!allowCallbackFinish.load ())
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
    };
    CleanupCallback cleanup = [&cleanedUp] (int)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
        cleanedUp = true;
    };
    int subscriptionId = subscribe ({ "test_pipe" }, callback, cleanup);

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);
    std::thread publisher ([&frame] () { publish ("test_pipe", frame); });

    while (!callbackStarted.load ())
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    std::thread unsubscriber (
        [subscriptionId, &unsubscribeReturned] ()
        {
            unsubscribe (subscriptionId);
            unsubscribeReturned = true;
        });

    std::this_thread::sleep_for (std::chrono::milliseconds (10));
    ASSERT_FALSE (cleanedUp.load ());
    ASSERT_FALSE (unsubscribeReturned.load ());

    allowCallbackFinish = true;
    unsubscriber.join ();
    publisher.join ();

    ASSERT_TRUE (cleanedUp.load ());
    ASSERT_TRUE (unsubscribeReturned.load ());
}

TEST (FramePipeTest, testUnsubscribeAfterThrowingCallback)
{
    bool cleanedUp = false;
    FrameCallback callback
        = [] (const std::string &, int, const FFmpeg::Frame &)
    { throw std::runtime_error ("callback failed"); };
    CleanupCallback cleanup = [&cleanedUp] (int) { cleanedUp = true; };

    int subscriptionId = subscribe ({ "test_pipe" }, callback, cleanup);

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);
    ASSERT_THROW (publish ("test_pipe", frame), std::runtime_error);

    unsubscribe (subscriptionId);
    ASSERT_TRUE (cleanedUp);
}

TEST (FramePipeTest, testPublishExceptionReleasesUncalledCallbacks)
{
    auto cleanupCount = std::make_shared<std::atomic<int>> (0);
    FrameCallback callback
        = [] (const std::string &, int, const FFmpeg::Frame &)
    { throw std::runtime_error ("callback failed"); };
    CleanupCallback cleanup = [cleanupCount] (int)
    { cleanupCount->fetch_add (1, std::memory_order_acq_rel); };

    std::vector<int> subscriptionIds{
        subscribe ({ "test_pipe_throwing" }, callback, cleanup),
        subscribe ({ "test_pipe_throwing" }, callback, cleanup),
    };

    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, SAMPLE_RATE, CHANNELS, NB_SAMPLES);
    ASSERT_THROW (publish ("test_pipe_throwing", frame), std::runtime_error);

    std::atomic<bool> unsubscribeReturned{ false };
    std::thread unsubscriber (
        [subscriptionIds, &unsubscribeReturned] ()
        {
            for (int subscriptionId : subscriptionIds)
            {
                unsubscribe (subscriptionId);
            }
            unsubscribeReturned = true;
        });

    auto deadline
        = std::chrono::steady_clock::now () + std::chrono::milliseconds (500);
    while (!unsubscribeReturned.load ()
           && std::chrono::steady_clock::now () < deadline)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    if (!unsubscribeReturned.load ())
    {
        unsubscriber.detach ();
        FAIL () << "unsubscribe blocked after publish threw";
    }

    unsubscriber.join ();
    ASSERT_EQ (cleanupCount->load (), 2);
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
