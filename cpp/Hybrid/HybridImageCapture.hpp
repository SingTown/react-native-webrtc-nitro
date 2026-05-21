#pragma once
#include "HybridImageCaptureSpec.hpp"
#include "HybridMediaStreamTrack.hpp"
#include <NitroModules/Promise.hpp>

namespace margelo::nitro::webrtc
{
    class HybridImageCapture : public HybridImageCaptureSpec
    {
      private:
        std::shared_ptr<HybridMediaStreamTrack> track = nullptr;

      public:
        HybridImageCapture () : HybridObject (TAG), HybridImageCaptureSpec ()
        {
        }

        auto getTrack ()
            -> std::shared_ptr<HybridMediaStreamTrackSpec> override
        {
            return track;
        }

        void setTrack (
            const std::shared_ptr<HybridMediaStreamTrackSpec> &track) override
        {
            this->track
                = std::dynamic_pointer_cast<HybridMediaStreamTrack> (track);
        }

        auto takePhoto (const std::string &file)
            -> std::shared_ptr<Promise<void>> override;
    };
} // namespace margelo::nitro::webrtc
