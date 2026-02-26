#include "NativeMicrophone.hpp"

#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
    constexpr int kTargetSampleRate = 48000;
    constexpr float kAttack = 0.12f;
    constexpr float kRelease = 0.06f;
    constexpr float kLimiterPeak = 26000.0f;
    constexpr float kFarRmsThreshold = 420.0f;
    constexpr float kFarMaxGain = 16.0f;
    constexpr float kNearMaxGain = 6.0f;
    constexpr float kNoiseGateRms = 120.0f;
    constexpr float kNoiseGateMaxGain = 8.0f;
}

NativeMicrophone::~NativeMicrophone () { stop (); }

auto NativeMicrophone::start (const std::string &pipeId) -> bool
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

    pipeId_ = pipeId;
    smoothedGain_ = 1.0f;

    result = stream_->requestStart ();
    if (result != oboe::Result::OK)
    {
        stream_->close ();
        stream_.reset ();
        pipeId_.clear ();
        return false;
    }

    return true;
}

void NativeMicrophone::stop ()
{
    std::shared_ptr<oboe::AudioStream> streamToClose;
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pipeId_.clear ();
        smoothedGain_ = 1.0f;
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
    float desiredGain = 6000.0f / std::max (rms, 1.0f);
    const float dynamicMaxGain
        = rms < kFarRmsThreshold ? kFarMaxGain : kNearMaxGain;
    desiredGain
        = std::clamp (desiredGain, 1.2f, dynamicMaxGain);
    if (rms < kNoiseGateRms)
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
    return false;
}
