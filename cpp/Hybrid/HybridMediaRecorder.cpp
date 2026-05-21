#include "HybridMediaRecorder.hpp"
#include "FramePipe.hpp"
#include <filesystem>

using namespace margelo::nitro::webrtc;

void HybridMediaRecorder::startRecording (const std::string &file)
{
    if (subscriptionId != -1)
    {
        throw std::runtime_error ("MediaRecorder is already recording");
    }

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
