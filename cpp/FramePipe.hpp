#pragma once
#include "FFmpeg.hpp"
#include <functional>

using FrameCallback
    = std::function<void (const std::string &pipeId, int subscriptionId,
                          const FFmpeg::Frame &frame)>;
using CleanupCallback = std::function<void (int subscriptionId)>;

auto subscribe (const std::vector<std::string> &pipeIds,
                const FrameCallback &onFrame,
                const CleanupCallback &onCleanup = {}) -> int;
// unsubscribe blocks until in-flight callbacks and cleanup finish. Do not call
// it from inside a FrameCallback.
void unsubscribe (int subscriptionId);
void publish (const std::string &pipeId, const FFmpeg::Frame &frame);
