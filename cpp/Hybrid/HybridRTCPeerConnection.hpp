#pragma once
#include "HybridRTCPeerConnectionSpec.hpp"
#include "HybridRTCRtpTransceiver.hpp"

namespace margelo::nitro::webrtc
{
    using StateChangeHandler
        = std::optional<std::function<void (const Event &event)>>;
    using IceCandidateHandler = std::optional<
        std::function<void (const RTCPeerConnectionIceEvent &event)>>;
    using TrackHandler
        = std::optional<std::function<void (const RTCTrackEvent &event)>>;

    class HybridRTCPeerConnection : public HybridRTCPeerConnectionSpec
    {
      private:
        std::shared_ptr<rtc::PeerConnection> peerConnection;
        std::vector<std::shared_ptr<HybridRTCRtpTransceiver>> transceivers;
        StateChangeHandler connectionStateChangeHandler;
        StateChangeHandler iceGatheringStateChangeHandler;
        IceCandidateHandler iceCandidateHandler;
        TrackHandler trackHandler;

      public:
        HybridRTCPeerConnection ()
            : HybridObject (TAG), HybridRTCPeerConnectionSpec ()
        {
            peerConnection = std::make_shared<rtc::PeerConnection> ();
        }

        auto getRemoteMediaFromMid (const std::string &mid)
            -> const rtc::Description::Media *;

        RTCPeerConnectionState getConnectionState () override;
        RTCIceGatheringState getIceGatheringState () override;
        std::string getLocalDescription () override;
        std::string getRemoteDescription () override;
        std::shared_ptr<Promise<void>> setLocalDescription (
            const std::optional<RTCSessionDescriptionInit> &description)
            override;
        std::shared_ptr<Promise<void>> setRemoteDescription (
            const RTCSessionDescriptionInit &description) override;

        StateChangeHandler getOnconnectionstatechange () override;
        void setOnconnectionstatechange (
            const StateChangeHandler &handler) override;
        StateChangeHandler getOnicegatheringstatechange () override;
        void setOnicegatheringstatechange (
            const StateChangeHandler &handler) override;
        IceCandidateHandler getOnicecandidate () override;
        void setOnicecandidate (const IceCandidateHandler &handler) override;
        std::shared_ptr<Promise<void>> addIceCandidate (
            const std::optional<
                std::variant<nitro::NullType, RTCIceCandidateInit>> &candidate)
            override;

        TrackHandler getOntrack () override;
        void setOntrack (const TrackHandler &handler) override;

        std::shared_ptr<HybridRTCRtpTransceiverSpec> addTransceiver (
            const std::variant<std::shared_ptr<HybridMediaStreamTrackSpec>,
                               std::string> &trackOrKind,
            const std::optional<RTCRtpTransceiverInit> &init) override;
        std::vector<std::shared_ptr<HybridRTCRtpTransceiverSpec>>
        getTransceivers () override;
        void setConfiguration (
            const std::optional<RTCConfiguration> &config) override;
        void close () override;

        std::shared_ptr<Promise<RTCSessionDescriptionInit>>
        createOffer () override;
        std::shared_ptr<Promise<RTCSessionDescriptionInit>>
        createAnswer () override;
    };
} // namespace margelo::nitro::webrtc
