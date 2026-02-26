#pragma once

#include <mutex>
#include <oboe/Oboe.h>
#include <string>

class NativeMicrophone : public oboe::AudioStreamDataCallback,
                         public oboe::AudioStreamErrorCallback
{
  public:
    ~NativeMicrophone () override;

    auto start (const std::string &pipeId) -> bool;
    void stop ();

    auto onAudioReady (oboe::AudioStream *audioStream, void *audioData,
                       int32_t numFrames) -> oboe::DataCallbackResult override;

    auto onError (oboe::AudioStream *audioStream, oboe::Result error)
        -> bool override;

  private:
    std::mutex mutex_;
    std::shared_ptr<oboe::AudioStream> stream_;
    std::string pipeId_;
    float smoothedGain_ = 1.0f;
};
