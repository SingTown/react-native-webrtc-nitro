#include "FramePipe.hpp"
#include <algorithm>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

static std::shared_mutex mutex;
static int nextSubscriptionId = 1;

struct Subscription
{
    Subscription (std::vector<std::string> pipeIds, FrameCallback onFrame,
                  CleanupCallback onCleanup)
        : pipeIds (std::move (pipeIds)),
          onFrame (std::move (onFrame)),
          onCleanup (std::move (onCleanup))
    {
    }
    std::vector<std::string> pipeIds;
    FrameCallback onFrame;
    CleanupCallback onCleanup;
    std::atomic<int> inFlight { 0 };
    bool pendingCleanup = false;
};

std::unordered_map<int, Subscription> subscriptions;

auto subscribe (const std::vector<std::string> &pipeIds,
                const FrameCallback &onFrame, const CleanupCallback &onCleanup)
    -> int
{
    std::unique_lock lock (mutex);
    int subscriptionId = nextSubscriptionId++;
    subscriptions.try_emplace (subscriptionId, pipeIds, onFrame, onCleanup);
    return subscriptionId;
}

void unsubscribe (int subscriptionId)
{
    CleanupCallback cleanup;
    {
        std::unique_lock lock (mutex);
        auto it = subscriptions.find (subscriptionId);
        if (it != subscriptions.end ())
        {
            it->second.pendingCleanup = true;
            if (it->second.inFlight.load (std::memory_order_acquire) == 0)
            {
                cleanup = it->second.onCleanup;
                subscriptions.erase (it);
            }
        }
    }
    if (cleanup)
    {
        cleanup (subscriptionId);
    }
}

void publish (const std::string &pipeId, const FFmpeg::Frame &frame)
{
    if (pipeId.empty ())
    {
        return;
    }
    std::vector<std::pair<int, FrameCallback>> callbacks;
    {
        std::shared_lock lock (mutex);
        for (auto &subscription : subscriptions)
        {
            if (subscription.second.pendingCleanup)
            {
                continue;
            }
            if (!subscription.second.onFrame)
            {
                continue;
            }
            const auto &ids = subscription.second.pipeIds;
            if (std::find (ids.begin (), ids.end (), pipeId) != ids.end ())
            {
                subscription.second.inFlight.fetch_add (
                    1, std::memory_order_acq_rel);
                callbacks.emplace_back (subscription.first,
                                        subscription.second.onFrame);
            }
        }
    }

    for (const auto &callback : callbacks)
    {
        callback.second (pipeId, callback.first, frame);
        CleanupCallback cleanup;
        {
            std::unique_lock lock (mutex);
            auto it = subscriptions.find (callback.first);
            if (it != subscriptions.end ())
            {
                int remaining
                    = it->second.inFlight.fetch_sub (1,
                                                     std::memory_order_acq_rel)
                      - 1;
                if (remaining == 0 && it->second.pendingCleanup)
                {
                    cleanup = it->second.onCleanup;
                    subscriptions.erase (it);
                }
            }
        }
        if (cleanup)
        {
            cleanup (callback.first);
        }
    }
}
