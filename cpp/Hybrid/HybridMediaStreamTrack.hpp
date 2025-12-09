#pragma once
#include "FramePipe.hpp"
#include "HybridCameraSpec.hpp"
#include "HybridMediaStreamTrackSpec.hpp"
#include "HybridMicrophoneSpec.hpp"
#include "MockCamera.hpp"
#include "MockMicrophone.hpp"
#include <thread>

namespace margelo::nitro::webrtc
{
    auto uuidv4 () -> std::string;

    class HybridMediaStreamTrack : public HybridMediaStreamTrackSpec
    {
      private:
        const std::string kind;
        const std::string id;
        const std::string srcPipeId = uuidv4 ();
        const std::string dstPipeId = uuidv4 ();
        int subscriptionId = -1;
        std::thread mockThread;
        MediaStreamTrackState state = MediaStreamTrackState::LIVE;

        void enable ();
        void disable ();

      public:
        std::shared_ptr<MockMicrophone> mockMicrophone = nullptr;
        std::shared_ptr<MockCamera> mockCamera = nullptr;
        std::shared_ptr<HybridMicrophoneSpec> microphone = nullptr;
        std::shared_ptr<HybridCameraSpec> camera = nullptr;

        HybridMediaStreamTrack ()
            : HybridObject (TAG), HybridMediaStreamTrackSpec ()
        {
            throw std::runtime_error (
                "Uncaught TypeError: Illegal constructor");
        }

        HybridMediaStreamTrack (std::string kind)
            : HybridObject (TAG), HybridMediaStreamTrackSpec (),
              kind (std::move (kind)), id (uuidv4 ())
        {
            this->enable ();
        }

        HybridMediaStreamTrack (std::string kind, std::string id)
            : HybridObject (TAG), HybridMediaStreamTrackSpec (),
              kind (std::move (kind)), id (std::move (id))
        {
            this->enable ();
        }

        auto getId () -> std::string override { return id; };

        auto getKind () -> std::string override { return kind; };

        auto getReadyState () -> MediaStreamTrackState override
        {
            return state;
        };

        auto getEnabled () -> bool override;

        void setEnabled (bool enabled) override;

        auto get_srcPipeId () -> std::string override { return srcPipeId; };

        auto get_dstPipeId () -> std::string override { return dstPipeId; };

        void stop () override
        {
            state = MediaStreamTrackState::ENDED;
            if (mockMicrophone)
            {
                mockMicrophone->dispose ();
            }
            if (mockCamera)
            {
                mockCamera->dispose ();
            }
            if (microphone)
            {
                microphone->dispose();
            }
            if (camera)
            {
                camera->dispose();
            }
        };
    };
} // namespace margelo::nitro::webrtc
