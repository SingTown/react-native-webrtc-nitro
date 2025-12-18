#include "rtcpjitterbuffer.hpp"

namespace rtc
{

    RtcpJitterBuffer::RtcpJitterBuffer (size_t remainBufferSize)
        : remainBufferSize (remainBufferSize)
    {
    }

    void RtcpJitterBuffer::incoming (message_vector &messages,
                                     const message_callback &)
    {
        for (const auto &message : messages)
        {
            if (message->type != Message::Binary)
                continue;

            if (message->size () < sizeof (RtpHeader))
                continue;

            mBuffer.push (message);
        }

        message_vector result;

        while (mBuffer.size () > remainBufferSize)
        {
            result.push_back (mBuffer.top ());
            mBuffer.pop ();
        }
        messages.swap (result);
    }

} // namespace rtc
