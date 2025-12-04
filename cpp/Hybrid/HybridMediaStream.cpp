#include "HybridMediaStream.hpp"

using namespace margelo::nitro::webrtc;

std::string HybridMediaStream::getId () { return id; }

std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>>
HybridMediaStream::getTracks ()
{
    std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>> trackSpecs;
    for (const auto &track : tracks)
    {
        trackSpecs.push_back (track);
    }
    return trackSpecs;
}

void HybridMediaStream::addTrack (
    const std::shared_ptr<HybridMediaStreamTrackSpec> &track)
{
    auto concreteTrack
        = std::dynamic_pointer_cast<HybridMediaStreamTrack> (track);
    if (concreteTrack)
    {
        tracks.push_back (concreteTrack);
    }
}

void HybridMediaStream::removeTrack (
    const std::shared_ptr<HybridMediaStreamTrackSpec> &track)
{
    auto concreteTrack
        = std::dynamic_pointer_cast<HybridMediaStreamTrack> (track);
    if (concreteTrack)
    {
        tracks.erase (
            std::remove (tracks.begin (), tracks.end (), concreteTrack),
            tracks.end ());
    }
}

std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>>
HybridMediaStream::getAudioTracks ()
{
    std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>> audioTracks;
    for (const auto &track : tracks)
    {
        if (track->getKind () == "audio")
        {
            audioTracks.push_back (track);
        }
    }
    return audioTracks;
}

std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>>
HybridMediaStream::getVideoTracks ()
{
    std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>> videoTracks;
    for (const auto &track : tracks)
    {
        if (track->getKind () == "video")
        {
            videoTracks.push_back (track);
        }
    }
    return videoTracks;
}
