#include "NativeMicrophone.hpp"

#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace
{
    constexpr int kTargetSampleRate = 48000;
    constexpr float kAttack = 0.12f;
    constexpr float kRelease = 0.06f;
    constexpr float kLimiterPeak = 26000.0f;
    constexpr float kNoiseGateMaxGain = 1.0f;
    constexpr float kAgcMinGain = 1.0f;
    constexpr float kInitialNoiseFloorRms = 180.0f;
    constexpr float kNoiseFloorMinRms = 80.0f;
    constexpr float kNoiseFloorMaxRms = 320.0f;
    constexpr float kNoiseFloorTrackRatio = 3.0f;
    constexpr float kNoiseFloorRise = 0.02f;
    constexpr float kNoiseFloorFall = 0.12f;
    constexpr float kNoiseGateCloseRatio = 1.8f;
    constexpr float kNoiseGateMinRms = 180.0f;
    constexpr float kNoiseGateMaxRms = 600.0f;
    constexpr float kFarThresholdMinRms = 320.0f;
    constexpr float kFarThresholdMaxRms = 1000.0f;
    constexpr float kAgcTargetRmsMin = 1000.0f;
    constexpr float kAgcTargetRmsMax = 12000.0f;
    constexpr float kNearMaxGainMin = 1.0f;
    constexpr float kNearMaxGainMax = 12.0f;
    constexpr float kFarMaxGainMax = 24.0f;
    constexpr float kNoiseGateOpenRatioMin = kNoiseGateCloseRatio + 0.2f;
    constexpr float kNoiseGateOpenRatioMax = 4.0f;
    constexpr float kFarThresholdRatioMin = 2.0f;
    constexpr float kFarThresholdRatioMax = 6.0f;

    auto clampFinite (float value, float fallback, float min, float max) -> float
    {
        if (!std::isfinite (value))
        {
            value = fallback;
        }
        return std::clamp (value, min, max);
    }

    auto sanitizeTuning (const NativeMicrophone::Tuning &tuning)
        -> NativeMicrophone::Tuning
    {
        NativeMicrophone::Tuning sanitized;
        sanitized.agcTargetRms
            = clampFinite (tuning.agcTargetRms, sanitized.agcTargetRms,
                           kAgcTargetRmsMin, kAgcTargetRmsMax);
        sanitized.nearMaxGain
            = clampFinite (tuning.nearMaxGain, sanitized.nearMaxGain,
                           kNearMaxGainMin, kNearMaxGainMax);
        sanitized.farMaxGain
            = clampFinite (tuning.farMaxGain, sanitized.farMaxGain,
                           sanitized.nearMaxGain, kFarMaxGainMax);
        sanitized.noiseGateOpenRatio
            = clampFinite (tuning.noiseGateOpenRatio,
                           sanitized.noiseGateOpenRatio,
                           kNoiseGateOpenRatioMin, kNoiseGateOpenRatioMax);
        sanitized.farThresholdRatio
            = clampFinite (tuning.farThresholdRatio, sanitized.farThresholdRatio,
                           kFarThresholdRatioMin, kFarThresholdRatioMax);
        return sanitized;
    }
}

NativeMicrophone::~NativeMicrophone () { stop (); }

auto NativeMicrophone::start (const std::string &pipeId, const Tuning &tuning)
    -> bool
{
    std::lock_guard<std::mutex> lock (mutex_);

    if (pipeId.empty ())
    {
        return false;
    }

    if (stream_ != nullptr)
    {
        stream_->requestStop ();
        stream_->close ();
        stream_.reset ();
    }

    pipeId_ = pipeId;
    tuning_ = sanitizeTuning (tuning);
    smoothedGain_ = 1.0f;
    noiseFloor_ = kInitialNoiseFloorRms;
    noiseGateOpen_ = false;
    restartInProgress_.store (false);
    return openAndStartStreamLocked ();
}

void NativeMicrophone::stop ()
{
    std::shared_ptr<oboe::AudioStream> streamToClose;
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pipeId_.clear ();
        smoothedGain_ = 1.0f;
        noiseFloor_ = kInitialNoiseFloorRms;
        noiseGateOpen_ = false;
        restartInProgress_.store (false);
        streamToClose = std::move (stream_);
    }

    if (streamToClose != nullptr)
    {
        streamToClose->requestStop ();
        streamToClose->close ();
    }
}

auto NativeMicrophone::onAudioReady (oboe::AudioStream *audioStream,
                                     void *audioData, int32_t numFrames)
    -> oboe::DataCallbackResult
{
    std::string pipe;
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pipe = pipeId_;
    }

    if (pipe.empty () || audioData == nullptr || numFrames <= 0)
    {
        return oboe::DataCallbackResult::Continue;
    }

    const int channelCount = audioStream->getChannelCount ();
    const int sampleRate = audioStream->getSampleRate ();
    const int sampleCount = numFrames * channelCount;
    const auto *input = static_cast<const int16_t *> (audioData);
    float sumSquares = 0.0f;
    for (int i = 0; i < sampleCount; ++i)
    {
        const auto s = static_cast<float> (input[i]);
        sumSquares += s * s;
    }
    const float rms
        = std::sqrt (sumSquares / static_cast<float> (sampleCount));
    if (rms < noiseFloor_ * kNoiseFloorTrackRatio)
    {
        const float tracking = rms > noiseFloor_ ? kNoiseFloorRise : kNoiseFloorFall;
        noiseFloor_ += (rms - noiseFloor_) * tracking;
        noiseFloor_
            = std::clamp (noiseFloor_, kNoiseFloorMinRms, kNoiseFloorMaxRms);
    }

    const float noiseGateOpenRms
        = std::clamp (noiseFloor_ * tuning_.noiseGateOpenRatio,
                      kNoiseGateMinRms, kNoiseGateMaxRms);
    const float noiseGateCloseRms
        = std::clamp (noiseFloor_ * kNoiseGateCloseRatio, kNoiseGateMinRms,
                      kNoiseGateMaxRms);
    if (noiseGateOpen_)
    {
        if (rms < noiseGateCloseRms)
        {
            noiseGateOpen_ = false;
        }
    }
    else if (rms > noiseGateOpenRms)
    {
        noiseGateOpen_ = true;
    }

    const float farRmsThreshold
        = std::clamp (noiseFloor_ * tuning_.farThresholdRatio,
                      kFarThresholdMinRms, kFarThresholdMaxRms);
    float desiredGain = tuning_.agcTargetRms / std::max (rms, 1.0f);
    const float dynamicMaxGain
        = rms < farRmsThreshold ? tuning_.farMaxGain : tuning_.nearMaxGain;
    desiredGain = std::clamp (desiredGain, kAgcMinGain, dynamicMaxGain);
    if (!noiseGateOpen_)
    {
        desiredGain = std::min (desiredGain, kNoiseGateMaxGain);
    }

    const float smoothing
        = desiredGain > smoothedGain_ ? kAttack : kRelease;
    smoothedGain_ += (desiredGain - smoothedGain_) * smoothing;

    float estimatedPeak = 0.0f;
    for (int i = 0; i < sampleCount; ++i)
    {
        const float v
            = std::abs (static_cast<float> (input[i])) * smoothedGain_;
        if (v > estimatedPeak)
        {
            estimatedPeak = v;
        }
    }
    float limiterGain = 1.0f;
    if (estimatedPeak > kLimiterPeak)
    {
        limiterGain = kLimiterPeak / estimatedPeak;
    }
    const float finalGain = smoothedGain_ * limiterGain;

    auto frame = FFmpeg::Frame (AV_SAMPLE_FMT_S16, sampleRate, channelCount,
                                numFrames);
    auto *output = reinterpret_cast<int16_t *> (frame->data[0]);
    for (int i = 0; i < sampleCount; ++i)
    {
        const int scaled = static_cast<int> (input[i] * finalGain);
        if (scaled > 32767)
        {
            output[i] = 32767;
        }
        else if (scaled < -32768)
        {
            output[i] = -32768;
        }
        else
        {
            output[i] = static_cast<int16_t> (scaled);
        }
    }
    publish (pipe, frame);

    return oboe::DataCallbackResult::Continue;
}

auto NativeMicrophone::onError (oboe::AudioStream *, oboe::Result) -> bool
{
    scheduleRestart ();
    return true;
}

auto NativeMicrophone::openAndStartStreamLocked () -> bool
{
    oboe::AudioStreamBuilder builder;
    builder.setDirection (oboe::Direction::Input)
        ->setPerformanceMode (oboe::PerformanceMode::LowLatency)
        ->setSharingMode (oboe::SharingMode::Shared)
        ->setInputPreset (oboe::InputPreset::VoiceCommunication)
        ->setFormat (oboe::AudioFormat::I16)
        ->setChannelCount (oboe::ChannelCount::Mono)
        ->setSampleRate (kTargetSampleRate)
        ->setDataCallback (this)
        ->setErrorCallback (this);

    oboe::Result result = builder.openStream (stream_);
    if (result != oboe::Result::OK || stream_ == nullptr)
    {
        return false;
    }

    result = stream_->requestStart ();
    if (result != oboe::Result::OK)
    {
        stream_->close ();
        stream_.reset ();
        return false;
    }

    return true;
}

void NativeMicrophone::scheduleRestart ()
{
    bool expected = false;
    if (!restartInProgress_.compare_exchange_strong (expected, true))
    {
        return;
    }

    std::string pipeSnapshot;
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pipeSnapshot = pipeId_;
    }
    if (pipeSnapshot.empty ())
    {
        restartInProgress_.store (false);
        return;
    }

    std::thread ([this, pipeSnapshot] {
        std::this_thread::sleep_for (std::chrono::milliseconds (180));
        for (int attempt = 1; attempt <= 3; ++attempt)
        {
            std::shared_ptr<oboe::AudioStream> streamToClose;
            {
                std::lock_guard<std::mutex> lock (mutex_);
                if (pipeId_.empty () || pipeId_ != pipeSnapshot)
                {
                    restartInProgress_.store (false);
                    return;
                }
                streamToClose = std::move (stream_);
            }
            if (streamToClose != nullptr)
            {
                streamToClose->requestStop ();
                streamToClose->close ();
            }

            bool started = false;
            {
                std::lock_guard<std::mutex> lock (mutex_);
                if (pipeId_.empty () || pipeId_ != pipeSnapshot)
                {
                    restartInProgress_.store (false);
                    return;
                }
                started = openAndStartStreamLocked ();
            }
            if (started)
            {
                restartInProgress_.store (false);
                return;
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (220));
        }
        restartInProgress_.store (false);
    }).detach ();
}
