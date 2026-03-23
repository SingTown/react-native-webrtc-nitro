#ifndef RTC_RTCP_JITTER_REQUESTER_H
#define RTC_RTCP_JITTER_REQUESTER_H

#include <queue>
#include <libavcodec/avcodec.h>
#include <rtc/rtc.hpp>

namespace rtc
{

    class RTC_CPP_EXPORT RtcpNackRequester final : public MediaHandler
    {
      public:
        RtcpNackRequester (SSRC ssrc, AVCodecID codec,
                           size_t jitterSize = 5,
                           size_t nackResendIntervalMs = 10,
                           size_t nackResendTimesMax = 10);
        SSRC ssrc;
        void incoming (message_vector &messages,
                       const message_callback &send) override;

      private:
        size_t jitterSize;
        size_t nackResendIntervalMs;
        size_t nackResendTimesMax;

        bool initialized = false;
        uint16_t expectedSeq;
        size_t nackResendTimes = 0;
        std::chrono::steady_clock::time_point nextNackTime
            = std::chrono::steady_clock::now ();

        AVCodecID codec;
        bool droppingUntilKeyframe = false;
        std::map<uint16_t, message_ptr> jitterBuffer;

        auto checkKeyframe (const std::byte *data, size_t size) -> bool;

        auto isSeqNewerOrEqual (uint16_t seq1, uint16_t seq2) -> bool;
        void clearBuffer ();
        auto nackMessage (uint16_t sequence) -> message_ptr;
    };

} // namespace rtc

#endif /* RTC_RTCP_JITTER_REQUESTER_H */
