#pragma once
#include "HybridRTCPeerConnectionSpec.hpp"
#include "HybridRTCRtpReceiver.hpp"
#include "HybridRTCRtpSender.hpp"
#include "HybridRTCRtpTransceiverSpec.hpp"
#include <rtc/rtc.hpp>

namespace margelo::nitro::webrtc
{
    class HybridRTCRtpTransceiver : public HybridRTCRtpTransceiverSpec
    {
      public:
        std::shared_ptr<rtc::PeerConnection> peerConnection;
        std::shared_ptr<rtc::Track> track;
        std::optional<rtc::Description::Media::RtpMap> rtpMap;

        std::optional<std::string> mid = std::nullopt;
        rtc::Description::Direction direction
            = rtc::Description::Direction::Inactive;
        std::shared_ptr<HybridRTCRtpSender> sender;
        std::shared_ptr<HybridRTCRtpReceiver> receiver;

        HybridRTCRtpTransceiver ()
            : HybridObject (TAG), HybridRTCRtpTransceiverSpec ()
        {
        }

        HybridRTCRtpTransceiver (
            const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                               std::string> &trackOrKind,
            const std::optional<RTCRtpTransceiverInit> &init);

        rtc::Description::Media offerMedia (const std::string &mid);
        rtc::Description::Media
        answerMedia (const rtc::Description::Media &remoteMedia);
        void start (const rtc::Description::Media &remoteMedia);
        std::string getKind ();
        void senderOnOpen ();
        void receiverOnOpen ();

        std::variant<nitro::NullType, std::string> getMid () override;
        std::shared_ptr<HybridRTCRtpSenderSpec> getSender () override;
        std::shared_ptr<HybridRTCRtpReceiverSpec> getReceiver () override;

        RTCRtpTransceiverDirection getDirection () override;
        void setDirection (RTCRtpTransceiverDirection direction) override;
    };
} // namespace margelo::nitro::webrtc
