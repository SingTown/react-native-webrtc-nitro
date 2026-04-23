#include "HybridMediaRecorder.hpp"
#include "FramePipe.hpp"
#include <atomic>
#include <filesystem>

using namespace margelo::nitro::webrtc;

auto HybridMediaRecorder::takePhoto (const std::string &file)
    -> std::shared_ptr<Promise<void>>
{
    if (mediaStream == nullptr)
    {
        throw std::runtime_error (
            "MediaStream is not set for HybridMediaRecorder");
    }

    if (std::filesystem::path (file).extension () != ".png")
    {
        throw std::invalid_argument ("Only .png format is supported");
    }

    auto tracks = mediaStream->getVideoTracks ();
    if (tracks.empty ())
    {
        throw std::runtime_error ("No video tracks available in MediaStream");
    }
    std::string srcPipeId = tracks[0]->get_srcPipeId ();
    return Promise<void>::async (
        [srcPipeId, file] () -> void
        {
            auto encoder = std::make_shared<FFmpeg::Encoder> (AV_CODEC_ID_PNG);
            auto photoSubscriptionId = std::make_shared<std::atomic<int>> (-1);
            FrameCallback callback =
                [encoder, file, photoSubscriptionId] (
                    const std::string &, int subscriptionId,
                    const FFmpeg::Frame &frame)
            {
                FILE *f = fopen (file.c_str (), "wb");
                if (!f)
                {
                    throw std::invalid_argument ("Failed to open file " + file
                                                 + " for writing");
                }
                encoder->send (frame);
                encoder->flush ();
                for (const FFmpeg::Packet &packet : encoder->receive ())
                {
                    fwrite (packet->data, 1, packet->size, f);
                }
                fclose (f);
                ::unsubscribe (subscriptionId);
                photoSubscriptionId->store (-1, std::memory_order_release);
            };

            photoSubscriptionId->store (
                subscribe ({ srcPipeId }, callback, nullptr),
                std::memory_order_release);

            // wait for the callback to be called
            while (photoSubscriptionId->load (std::memory_order_acquire) != -1)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }
        });
}

void HybridMediaRecorder::startRecording (const std::string &file)
{
    if (mediaStream == nullptr)
    {
        throw std::runtime_error (
            "MediaStream is not set for HybridMediaRecorder");
    }

    if (std::filesystem::path (file).extension () != ".mp4")
    {
        throw std::invalid_argument ("Only .mp4 format is supported c++");
    }
    if (recordingSubscriptionId != -1)
    {
        unsubscribe (recordingSubscriptionId);
        recordingSubscriptionId = -1;
    }
    stopPromise = nullptr;
    stopError = nullptr;
    stopCompleted = false;

    AVCodecID audioCodecId = AV_CODEC_ID_NONE;
    AVCodecID videoCodecId = AV_CODEC_ID_NONE;
    std::string audioPipeId = "";
    std::string videoPipeId = "";
    std::vector<std::string> pipeIds;
    if (!mediaStream->getAudioTracks ().empty ())
    {
        audioPipeId = mediaStream->getAudioTracks ()[0]->get_srcPipeId ();
        pipeIds.push_back (audioPipeId);
        audioCodecId = AV_CODEC_ID_AAC;
    }
    if (!mediaStream->getVideoTracks ().empty ())
    {
        videoPipeId = mediaStream->getVideoTracks ()[0]->get_srcPipeId ();
        pipeIds.push_back (videoPipeId);
        videoCodecId = AV_CODEC_ID_H264;
    }

    auto muxer
        = std::make_shared<FFmpeg::Muxer> (file, audioCodecId, videoCodecId);
    FrameCallback callback
        = [muxer, audioPipeId, videoPipeId] (const std::string &pipeId, int,
                                             const FFmpeg::Frame &frame)
    {
        if (pipeId == audioPipeId)
        {
            muxer->writeAudio (frame);
        }
        if (pipeId == videoPipeId)
        {
            muxer->writeVideo (frame);
        }
    };

    CleanupCallback cleanup = [this, muxer] (int)
    {
        try
        {
            muxer->stop ();
        }
        catch (...)
        {
            std::lock_guard lock (recordingMutex);
            stopError = std::current_exception ();
        }

        std::shared_ptr<Promise<void>> promise;
        std::exception_ptr error;
        {
            std::lock_guard lock (recordingMutex);
            stopCompleted = true;
            promise = stopPromise;
            error = stopError;
        }
        if (promise != nullptr)
        {
            if (error != nullptr)
            {
                promise->reject (error);
            }
            else
            {
                promise->resolve ();
            }
        }
    };

    recordingSubscriptionId = subscribe (pipeIds, callback, cleanup);
}

auto HybridMediaRecorder::stopRecording () -> std::shared_ptr<Promise<void>>
{
    if (recordingSubscriptionId == -1)
    {
        auto promise = Promise<void>::create ();
        promise->resolve ();
        return promise;
    }

    std::shared_ptr<Promise<void>> promise;
    bool completed = false;
    std::exception_ptr error;
    int subscriptionId = -1;
    {
        std::lock_guard lock (recordingMutex);
        if (stopPromise == nullptr)
        {
            stopPromise = Promise<void>::create ();
        }
        promise = stopPromise;
        completed = stopCompleted;
        error = stopError;
        subscriptionId = recordingSubscriptionId;
    }

    if (completed)
    {
        if (error != nullptr)
        {
            promise->reject (error);
        }
        else
        {
            promise->resolve ();
        }
        return promise;
    }

    unsubscribe (subscriptionId);

    {
        std::lock_guard lock (recordingMutex);
        recordingSubscriptionId = -1;
    }
    return promise;
}
