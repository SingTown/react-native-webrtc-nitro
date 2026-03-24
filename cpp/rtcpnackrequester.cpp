#include "rtcpnackrequester.hpp"

namespace rtc
{
    RtcpNackRequester::RtcpNackRequester (SSRC ssrc, AVCodecID codec,
                                          size_t jitterSize,
                                          size_t nackResendIntervalMs,
                                          size_t nackResendTimesMax)
        : ssrc (ssrc), codec (codec), jitterSize (jitterSize),
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

        while (jitterBuffer.size () > 0)
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
                if (jitterBuffer.size () < jitterSize)
                {
                    break;
                }
                if (nackResendTimes >= nackResendTimesMax)
                {
                    clearBuffer ();
                    send (pliMessage ());
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
        if (droppingUntilKeyframe)
        {
            message_vector filtered;
            for (auto &msg : result)
            {
                if (!droppingUntilKeyframe)
                {
                    filtered.push_back (msg);
                    continue;
                }
                if (msg->type != Message::Binary
                    || msg->size () < sizeof (RtpHeader))
                {
                    filtered.push_back (msg);
                    continue;
                }
                auto rtp
                    = reinterpret_cast<RtpHeader *> (msg->data ());
                auto payload = reinterpret_cast<const std::byte *> (
                    rtp->getBody ());
                size_t payloadSize = msg->size () - rtp->getSize ();
                if (payloadSize > 0
                    && checkKeyframe (payload, payloadSize))
                {
                    droppingUntilKeyframe = false;
                    filtered.push_back (msg);
                }
            }
            result.swap (filtered);
        }
        messages.swap (result);
    }

    auto RtcpNackRequester::isSeqNewerOrEqual (uint16_t seq1, uint16_t seq2)
        -> bool
    {
        return (int16_t)(seq1 - seq2) >= 0;
    }

    auto RtcpNackRequester::checkKeyframe (const std::byte *data,
                                           size_t size) -> bool
    {
        if (codec == AV_CODEC_ID_H264)
        {
            if (size < 1)
                return false;
            uint8_t nalType = static_cast<uint8_t> (data[0]) & 0x1F;
            if (nalType == 7 || nalType == 5)
                return true; // SPS or IDR
            if (nalType == 28 && size >= 2)
            { // FU-A
                bool startBit = static_cast<uint8_t> (data[1]) >> 7;
                uint8_t fuNalType
                    = static_cast<uint8_t> (data[1]) & 0x1F;
                return startBit && fuNalType == 5;
            }
        }
        else if (codec == AV_CODEC_ID_H265)
        {
            if (size < 2)
                return false;
            uint8_t nalType
                = (static_cast<uint8_t> (data[0]) & 0x7E) >> 1;
            if (nalType == 19 || nalType == 20)
                return true; // IDR
            if (nalType == 32)
                return true; // VPS
            if (nalType == 49 && size >= 3)
            { // FU
                bool startBit = static_cast<uint8_t> (data[2]) >> 7;
                uint8_t fuNalType
                    = static_cast<uint8_t> (data[2]) & 0x3F;
                return startBit
                       && (fuNalType == 19 || fuNalType == 20);
            }
        }
        return false;
    }

    void RtcpNackRequester::clearBuffer ()
    {
        initialized = false;
        jitterBuffer.clear ();
        nackResendTimes = 0;
        nextNackTime = std::chrono::steady_clock::now ();
        droppingUntilKeyframe = true;
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

    auto RtcpNackRequester::pliMessage () -> message_ptr
    {
        message_ptr message
            = make_message (RtcpPli::Size (), Message::Control);
        auto *pli = reinterpret_cast<RtcpPli *> (message->data ());
        pli->preparePacket (ssrc);
        return message;
    }

} // namespace rtc
