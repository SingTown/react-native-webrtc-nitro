#include "rtcpnackrequester.hpp"

namespace rtc
{
    RtcpNackRequester::RtcpNackRequester (SSRC ssrc, size_t jitterSize,
                                          size_t nackResendIntervalMs,
                                          size_t nackResendTimesMax)
        : ssrc (ssrc), jitterSize (jitterSize),
          nackResendIntervalMs (nackResendIntervalMs),
          nackResendTimesMax (nackResendTimesMax)
    {
    }

    void RtcpNackRequester::incoming (message_vector &messages,
                                      const message_callback &send)
    {

        message_vector result;
        for (const auto &message : messages)
        {
            if (message->type != Message::Binary)
            {
                result.push_back (message);
                continue;
            }

            if (message->size () < sizeof (RtpHeader))
            {
                result.push_back (message);
                continue;
            }

            auto rtp = reinterpret_cast<RtpHeader *> (message->data ());
            uint16_t seqNo = rtp->seqNumber ();

            if (!initialized)
            {
                expectedSeq = seqNo;
                initialized = true;
            }
            if (isSeqNewerOrEqual (seqNo, expectedSeq))
            {
                jitterBuffer[seqNo] = message;
            }
        }

        while (jitterBuffer.size () > jitterSize)
        {
            bool alreadyReceived = jitterBuffer.count (expectedSeq) > 0;
            if (alreadyReceived)
            {
                auto packet = jitterBuffer[expectedSeq];
                result.push_back (packet);
                jitterBuffer.erase (expectedSeq);
                expectedSeq++;
                nackResendTimes = 0;
                continue;
            }
            else
            {
                if (nackResendTimes >= nackResendTimesMax)
                {
                    clearBuffer ();
                    break;
                }

                auto now = std::chrono::steady_clock::now ();
                if (now > nextNackTime)
                {
                    nextNackTime
                        = now
                          + std::chrono::milliseconds (nackResendIntervalMs);
                    send (nackMessage (expectedSeq));
                    nackResendTimes++;
                }

                break;
            }
        }
        messages.swap (result);
    }

    auto RtcpNackRequester::isSeqNewerOrEqual (uint16_t seq1, uint16_t seq2)
        -> bool
    {
        return (int16_t)(seq1 - seq2) >= 0;
    }

    void RtcpNackRequester::clearBuffer ()
    {
        initialized = false;
        jitterBuffer.clear ();
        nackResendTimes = 0;
        nextNackTime = std::chrono::steady_clock::now ();
    }

    auto RtcpNackRequester::nackMessage (uint16_t sequence) -> message_ptr
    {
        unsigned int fciCount = 0;
        uint16_t fciPID = 0;

        message_ptr message
            = make_message (RtcpNack::Size (1), Message::Control);
        auto *nack = reinterpret_cast<RtcpNack *> (message->data ());
        nack->preparePacket (ssrc, 1);
        nack->addMissingPacket (&fciCount, &fciPID, sequence);

        return message;
    }

} // namespace rtc
