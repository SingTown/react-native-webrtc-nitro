#pragma once
#include "HybridMediaStream.hpp"
#include "HybridMediaStreamTrack.hpp"
#include "HybridRTCRtpReceiverSpec.hpp"
#include <rtc/rtc.hpp>

namespace margelo::nitro::webrtc
{
    class HybridRTCRtpReceiver : public HybridRTCRtpReceiverSpec
    {
      public:
        std::shared_ptr<HybridMediaStreamTrack> track;
        std::vector<std::shared_ptr<HybridMediaStream>> streams;

        HybridRTCRtpReceiver ()
            : HybridObject (TAG), HybridRTCRtpReceiverSpec ()
        {
        }

        void receive (const rtc::Description::Media &remoteMedia);
        std::variant<nitro::NullType,
                     std::shared_ptr<HybridMediaStreamTrackSpec>>
        getTrack () override;
    };
} // namespace margelo::nitro::webrtc
