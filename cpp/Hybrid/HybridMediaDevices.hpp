#pragma once
#include "HybridMediaDevicesSpec.hpp"
#include "HybridMediaStream.hpp"
#include "HybridMediaStreamTrack.hpp"

namespace margelo::nitro::webrtc
{
    class HybridMediaDevices : public HybridMediaDevicesSpec
    {
      public:
        HybridMediaDevices () : HybridObject (TAG), HybridMediaDevicesSpec ()
        {
        }

        auto getMockMedia (const MediaStreamConstraints &constraints)
            -> std::shared_ptr<
                Promise<std::shared_ptr<HybridMediaStreamSpec>>> override;

        auto getUserMedia (const MediaStreamConstraints &constraints)
            -> std::shared_ptr<
                Promise<std::shared_ptr<HybridMediaStreamSpec>>> override;
    };
} // namespace margelo::nitro::webrtc
