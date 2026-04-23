#pragma once
#include "FramePipe.hpp"
#include "HybridMediaRecorderSpec.hpp"
#include "HybridMediaStream.hpp"
#include <NitroModules/Promise.hpp>
#include <exception>
#include <mutex>

namespace margelo::nitro::webrtc
{
    class HybridMediaRecorder : public HybridMediaRecorderSpec
    {
      private:
        std::shared_ptr<HybridMediaStream> mediaStream = nullptr;
        std::mutex recordingMutex;
        int recordingSubscriptionId = -1;
        std::shared_ptr<Promise<void>> stopPromise = nullptr;
        std::exception_ptr stopError = nullptr;
        bool stopCompleted = false;

      public:
        HybridMediaRecorder () : HybridObject (TAG), HybridMediaRecorderSpec ()
        {
        }
        ~HybridMediaRecorder () override
        {
            if (recordingSubscriptionId != -1)
            {
                unsubscribe (recordingSubscriptionId);
                recordingSubscriptionId = -1;
            }
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
        auto stopRecording () -> std::shared_ptr<Promise<void>> override;
    };
} // namespace margelo::nitro::webrtc
