#include "HybridMediaRecorder.hpp"
#include "FramePipe.hpp"
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
        [this, srcPipeId, file] () -> void
        {
            auto encoder = std::make_shared<FFmpeg::Encoder> (AV_CODEC_ID_PNG);
            FrameCallback callback =
                [this, encoder, file] (const std::string &, int subscriptionId,
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
                this->subscriptionId = -1;
            };

            this->subscriptionId
                = subscribe ({ srcPipeId }, callback, nullptr);

            // wait for the callback to be called
            while (this->subscriptionId != -1)
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

    CleanupCallback cleanup = [muxer] (int) { muxer->stop (); };

    subscriptionId = subscribe (pipeIds, callback, cleanup);
}

void HybridMediaRecorder::stopRecording ()
{
    unsubscribe (subscriptionId);
    subscriptionId = -1;
}
