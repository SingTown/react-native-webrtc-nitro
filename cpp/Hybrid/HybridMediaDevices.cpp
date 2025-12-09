#include "HybridMediaDevices.hpp"
#include "MockCamera.hpp"
#include "MockMicrophone.hpp"
#include <NitroModules/HybridObjectRegistry.hpp>
#include <NitroModules/Promise.hpp>

using namespace margelo::nitro::webrtc;

auto
HybridMediaDevices::getMockMedia (const MediaStreamConstraints &constraints)
    -> std::shared_ptr<Promise<std::shared_ptr<HybridMediaStreamSpec>>>
{
    auto hybridMediaStreams = std::make_shared<HybridMediaStream> ();
    if (constraints.audio.value_or (false))
    {
        auto hybridAudioTrack
            = std::make_shared<HybridMediaStreamTrack> ("audio");
        hybridAudioTrack->mockMicrophone = std::make_shared<MockMicrophone> (
            hybridAudioTrack->get_srcPipeId ());
        hybridMediaStreams->addTrack (hybridAudioTrack);
    }
    if (constraints.video.value_or (false))
    {
        auto hybridVideoTrack
            = std::make_shared<HybridMediaStreamTrack> ("video");
        hybridVideoTrack->mockCamera = std::make_shared<MockCamera> (
            hybridVideoTrack->get_srcPipeId ());
        hybridMediaStreams->addTrack (hybridVideoTrack);
    }
    return Promise<std::shared_ptr<HybridMediaStreamSpec>>::resolved (
        hybridMediaStreams);
};

auto
HybridMediaDevices::getUserMedia (const MediaStreamConstraints &constraints)
    -> std::shared_ptr<Promise<std::shared_ptr<HybridMediaStreamSpec>>>
{
    bool audio = constraints.audio.value_or (false);
    bool video = constraints.video.value_or (false);

    auto hybridMediaStreams = std::make_shared<HybridMediaStream> ();

    if (audio)
    {
        auto microphoneObj
            = HybridObjectRegistry::createHybridObject ("Microphone");
        auto microphone
            = std::dynamic_pointer_cast<HybridMicrophoneSpec> (microphoneObj);
        if (!microphone)
        {
            throw std::runtime_error (
                "Could not cast to HybridMicrophoneSpec");
        }

        auto hybridAudioTrack
            = std::make_shared<HybridMediaStreamTrack> ("audio");
        hybridAudioTrack->microphone = microphone;
        hybridMediaStreams->addTrack (hybridAudioTrack);

        microphone->open (hybridAudioTrack->get_srcPipeId ())->await ().get ();
    }

    if (video)
    {
        auto cameraObj = HybridObjectRegistry::createHybridObject ("Camera");
        auto camera = std::dynamic_pointer_cast<HybridCameraSpec> (cameraObj);
        if (!camera)
        {
            throw std::runtime_error ("Could not cast to HybridCameraSpec");
        }

        auto hybridVideoTrack
            = std::make_shared<HybridMediaStreamTrack> ("video");
        hybridVideoTrack->camera = camera;
        hybridMediaStreams->addTrack (hybridVideoTrack);

        camera->open (hybridVideoTrack->get_srcPipeId ())->await ().get ();
    }

    return Promise<std::shared_ptr<HybridMediaStreamSpec>>::resolved (
        hybridMediaStreams);
};
