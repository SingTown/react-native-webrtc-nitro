#include "FramePipe.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

static std::shared_mutex mutex;
static std::condition_variable_any cleanupCv;
static int nextSubscriptionId = 1;

struct Subscription
{
    Subscription (std::vector<std::string> pipeIds, FrameCallback onFrame,
                  CleanupCallback onCleanup)
        : pipeIds (std::move (pipeIds)), onFrame (std::move (onFrame)),
          onCleanup (std::move (onCleanup))
    {
    }

    std::vector<std::string> pipeIds;
    FrameCallback onFrame;
    CleanupCallback onCleanup;
    std::atomic<int> inFlight{ 0 };
    bool pendingCleanup = false;
};

std::unordered_map<int, Subscription> subscriptions;

static void eraseSubscription (int subscriptionId)
{
    std::unique_lock lock (mutex);
    subscriptions.erase (subscriptionId);
    cleanupCv.notify_all ();
}

static void runCleanupAndErase (int subscriptionId,
                                const CleanupCallback &cleanup)
{
    try
    {
        if (cleanup)
        {
            cleanup (subscriptionId);
        }
    }
    catch (...)
    {
        eraseSubscription (subscriptionId);
        throw;
    }
    eraseSubscription (subscriptionId);
}

static void finishFrameCallback (int subscriptionId)
{
    bool shouldNotify = false;
    {
        std::unique_lock lock (mutex);
        auto it = subscriptions.find (subscriptionId);
        if (it != subscriptions.end ())
        {
            int remaining
                = it->second.inFlight.fetch_sub (1, std::memory_order_acq_rel)
                  - 1;
            if (remaining == 0 && it->second.pendingCleanup)
            {
                shouldNotify = true;
            }
        }
    }
    if (shouldNotify)
    {
        cleanupCv.notify_all ();
    }
}

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
        if (it == subscriptions.end ())
        {
            return;
        }

        if (it->second.pendingCleanup)
        {
            cleanupCv.wait (lock,
                            [subscriptionId]
                            {
                                return subscriptions.find (subscriptionId)
                                       == subscriptions.end ();
                            });
            return;
        }

        it->second.pendingCleanup = true;
        cleanupCv.wait (lock,
                        [subscriptionId]
                        {
                            auto it = subscriptions.find (subscriptionId);
                            return it == subscriptions.end ()
                                   || it->second.inFlight.load (
                                          std::memory_order_acquire)
                                          == 0;
                        });

        it = subscriptions.find (subscriptionId);
        if (it == subscriptions.end ())
        {
            return;
        }
        cleanup = it->second.onCleanup;
    }

    runCleanupAndErase (subscriptionId, cleanup);
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

    for (std::size_t i = 0; i < callbacks.size (); ++i)
    {
        try
        {
            callbacks[i].second (pipeId, callbacks[i].first, frame);
        }
        catch (...)
        {
            for (std::size_t j = i; j < callbacks.size (); ++j)
            {
                finishFrameCallback (callbacks[j].first);
            }
            throw;
        }
        finishFrameCallback (callbacks[i].first);
    }
}
