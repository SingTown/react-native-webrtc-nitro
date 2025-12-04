#pragma once
#include "HybridMediaStream.hpp"
#include "HybridMediaStreamTrack.hpp"
#include "HybridRTCRtpSenderSpec.hpp"

namespace margelo::nitro::webrtc
{
    class HybridRTCRtpSender : public HybridRTCRtpSenderSpec
    {
      public:
        std::shared_ptr<HybridMediaStreamTrack> track;
        std::vector<std::shared_ptr<HybridMediaStream>> streams;

        HybridRTCRtpSender () : HybridObject (TAG), HybridRTCRtpSenderSpec ()
        {
        }

        std::variant<nitro::NullType,
                     std::shared_ptr<HybridMediaStreamTrackSpec>>
        getTrack () override;
    };
} // namespace margelo::nitro::webrtc
