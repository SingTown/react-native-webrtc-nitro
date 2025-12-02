#pragma once
#include "HybridMediaStreamSpec.hpp"
#include "HybridMediaStreamTrack.hpp"

namespace margelo::nitro::webrtc
{

    class HybridMediaStream : public HybridMediaStreamSpec
    {
      public:
        std::vector<std::shared_ptr<HybridMediaStreamTrack>> tracks;

        HybridMediaStream () : HybridObject (TAG), HybridMediaStreamSpec () {}

        std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>>
        getTracks () override;
        void addTrack (
            const std::shared_ptr<HybridMediaStreamTrackSpec> &track) override;
        void removeTrack (
            const std::shared_ptr<HybridMediaStreamTrackSpec> &track) override;
        std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>>
        getAudioTracks () override;
        std::vector<std::shared_ptr<HybridMediaStreamTrackSpec>>
        getVideoTracks () override;
    };
} // namespace margelo::nitro::webrtc
