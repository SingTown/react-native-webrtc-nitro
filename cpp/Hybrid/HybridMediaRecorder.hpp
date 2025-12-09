#pragma once
#include "HybridMediaRecorderSpec.hpp"
#include "HybridMediaStream.hpp"
#include <NitroModules/Promise.hpp>

namespace margelo::nitro::webrtc
{
    class HybridMediaRecorder : public HybridMediaRecorderSpec
    {
      private:
        std::shared_ptr<HybridMediaStream> mediaStream = nullptr;
        int subscriptionId = -1;

      public:
        HybridMediaRecorder () : HybridObject (TAG), HybridMediaRecorderSpec ()
        {
        }

        auto getStream () -> std::shared_ptr<HybridMediaStreamSpec> override
        {
            return mediaStream;
        }

        void setStream (
            const std::shared_ptr<HybridMediaStreamSpec> &mediaStream) override
        {
            this->mediaStream
                = std::dynamic_pointer_cast<HybridMediaStream> (mediaStream);
        }

        auto takePhoto (const std::string &file)
            -> std::shared_ptr<Promise<void>> override;
        void startRecording (const std::string &file) override;
        void stopRecording () override;
    };
} // namespace margelo::nitro::webrtc
