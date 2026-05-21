#include "HybridImageCapture.hpp"
#include "FramePipe.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <memory>
#include <stdexcept>

using namespace margelo::nitro::webrtc;

namespace
{
    constexpr auto PHOTO_FRAME_TIMEOUT = std::chrono::seconds (10);

    void writePng (const std::string &file, const FFmpeg::Frame &frame)
    {
        auto fileCloser = [] (FILE *f)
        {
            if (f != nullptr)
            {
                fclose (f);
            }
        };
        std::unique_ptr<FILE, decltype (fileCloser)> output (
            fopen (file.c_str (), "wb"), fileCloser);
        if (!output)
        {
            throw std::invalid_argument ("Failed to open file " + file
                                         + " for writing");
        }

        FFmpeg::Encoder encoder (AV_CODEC_ID_PNG);
        encoder.send (frame);
        encoder.flush ();
        for (const FFmpeg::Packet &packet : encoder.receive ())
        {
            size_t written
                = fwrite (packet->data, 1, packet->size, output.get ());
            if (written != static_cast<size_t> (packet->size))
            {
                throw std::runtime_error ("Failed to write file " + file);
            }
        }
    }
} // namespace

auto HybridImageCapture::takePhoto (const std::string &file)
    -> std::shared_ptr<Promise<void>>
{
    if (track == nullptr)
    {
        throw std::runtime_error (
            "MediaStreamTrack is not set for HybridImageCapture");
    }

    if (track->getKind () != "video")
    {
        throw std::runtime_error (
            "ImageCapture requires a video MediaStreamTrack");
    }

    if (track->getReadyState () == MediaStreamTrackState::ENDED)
    {
        throw std::runtime_error (
            "ImageCapture requires a live MediaStreamTrack");
    }

    if (std::filesystem::path (file).extension () != ".png")
    {
        throw std::invalid_argument ("Only .png format is supported");
    }

    std::string srcPipeId = track->get_srcPipeId ();
    return Promise<void>::async (
        [srcPipeId, file] () -> void
        {
            auto framePromise
                = std::make_shared<std::promise<FFmpeg::Frame>> ();
            auto frameFuture = framePromise->get_future ();
            FrameCallback callback
                = [framePromise] (const std::string &, int,
                                  const FFmpeg::Frame &frame)
            {
                try
                {
                    framePromise->set_value (frame);
                }
                catch (const std::future_error &)
                {
                }
            };

            int subscriptionId = -1;
            try
            {
                subscriptionId = subscribe ({ srcPipeId }, callback, nullptr);

                if (frameFuture.wait_for (PHOTO_FRAME_TIMEOUT)
                    != std::future_status::ready)
                {
                    throw std::runtime_error (
                        "Timed out waiting for a video frame");
                }

                FFmpeg::Frame frame = frameFuture.get ();

                ::unsubscribe (subscriptionId);
                subscriptionId = -1;
                writePng (file, frame);
            }
            catch (...)
            {
                if (subscriptionId != -1)
                {
                    ::unsubscribe (subscriptionId);
                }
                throw;
            }
        });
}
