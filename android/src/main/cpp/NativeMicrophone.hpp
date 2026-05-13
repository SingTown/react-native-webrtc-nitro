#pragma once

#include <atomic>
#include <mutex>
#include <oboe/Oboe.h>
#include <string>

class NativeMicrophone : public oboe::AudioStreamDataCallback,
                         public oboe::AudioStreamErrorCallback
{
  public:
    struct Tuning
    {
        float agcTargetRms = 6000.0f;
        float nearMaxGain = 6.0f;
        float farMaxGain = 16.0f;
        float noiseGateOpenRatio = 2.2f;
        float farThresholdRatio = 3.5f;
    };

    ~NativeMicrophone () override;

    auto start (const std::string &pipeId, const Tuning &tuning) -> bool;
    void stop ();

    auto onAudioReady (oboe::AudioStream *audioStream, void *audioData,
                       int32_t numFrames) -> oboe::DataCallbackResult override;

    auto onError (oboe::AudioStream *audioStream, oboe::Result error)
        -> bool override;

  private:
    auto openAndStartStreamLocked () -> bool;
    void scheduleRestart ();

    std::mutex mutex_;
    std::shared_ptr<oboe::AudioStream> stream_;
    std::string pipeId_;
    Tuning tuning_ {};
    float smoothedGain_ = 1.0f;
    float noiseFloor_ = 180.0f;
    bool noiseGateOpen_ = false;
    std::atomic<bool> restartInProgress_ { false };
};
