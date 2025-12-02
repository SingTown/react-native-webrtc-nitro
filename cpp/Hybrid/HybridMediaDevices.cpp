#include "HybridMediaDevices.hpp"
#include "HybridMediaStream.hpp"
#include "HybridMediaStreamTrack.hpp"

using namespace margelo::nitro::webrtc;

std::shared_ptr<Promise<std::shared_ptr<HybridMediaStreamSpec>>>
HybridMediaDevices::getMockMedia (const MediaStreamConstraints &constraints)
{
    auto stream = std::make_shared<HybridMediaStream> ();
    if (constraints.audio.value_or (false))
    {
        auto audioTrack = std::make_shared<HybridMediaStreamTrack> ();
        audioTrack->kind = "audio";
        audioTrack->addMocker ();
        stream->addTrack (audioTrack);
    }
    if (constraints.video.value_or (false))
    {
        auto videoTrack = std::make_shared<HybridMediaStreamTrack> ();
        videoTrack->kind = "video";
        videoTrack->addMocker ();
        stream->addTrack (videoTrack);
    }
    return Promise<std::shared_ptr<HybridMediaStreamSpec>>::resolved (stream);
}
