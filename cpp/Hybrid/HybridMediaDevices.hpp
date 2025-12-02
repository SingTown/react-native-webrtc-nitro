#pragma once
#include "HybridMediaDevicesSpec.hpp"

namespace margelo::nitro::webrtc
{
    class HybridMediaDevices : public HybridMediaDevicesSpec
    {
      public:
        HybridMediaDevices () : HybridObject (TAG), HybridMediaDevicesSpec ()
        {
        }

        std::shared_ptr<Promise<std::shared_ptr<HybridMediaStreamSpec>>>
        getMockMedia (const MediaStreamConstraints &constraints) override;
    };
} // namespace margelo::nitro::webrtc
