#ifndef RTC_RTCP_JITTER_BUFFER_H
#define RTC_RTCP_JITTER_BUFFER_H

#include <queue>
#include <rtc/rtc.hpp>

namespace rtc
{

    class RTC_CPP_EXPORT RtcpJitterBuffer final : public MediaHandler
    {
      public:
        RtcpJitterBuffer (size_t remainBufferSize = 20);

        void incoming (message_vector &messages,
                       const message_callback &send) override;

      private:
        size_t remainBufferSize;

        struct MessageCompare
        {
            auto operator() (const message_ptr &a, const message_ptr &b) const
                -> bool
            {
                auto rtpA = reinterpret_cast<RtpHeader *> (a->data ());
                uint16_t seqNoA = rtpA->seqNumber ();
                auto rtpB = reinterpret_cast<RtpHeader *> (b->data ());
                uint16_t seqNoB = rtpB->seqNumber ();

                return (int16_t)(seqNoA - seqNoB) > 0;
            }
        };

        std::priority_queue<message_ptr, std::vector<message_ptr>,
                            MessageCompare>
            mBuffer;
    };

} // namespace rtc

#endif /* RTC_RTCP_JITTER_BUFFER_H */
