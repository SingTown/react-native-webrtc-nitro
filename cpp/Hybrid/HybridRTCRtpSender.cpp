#include "HybridRTCRtpSender.hpp"

using namespace margelo::nitro::webrtc;

std::variant<margelo::nitro::NullType,
             std::shared_ptr<HybridMediaStreamTrackSpec>>
HybridRTCRtpSender::getTrack ()
{
    if (track)
    {
        return track;
    }
    else
    {
        return nitro::null;
    }
}
