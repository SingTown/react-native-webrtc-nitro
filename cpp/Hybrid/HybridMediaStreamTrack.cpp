#include "HybridMediaStreamTrack.hpp"
#include "FramePipe.hpp"

using namespace margelo::nitro::webrtc;

auto margelo::nitro::webrtc::uuidv4 () -> std::string
{
    std::random_device rd;
    std::mt19937 gen (rd ());
    std::uniform_int_distribution<uint32_t> dis (0, 0xFFFFFFFF);

    uint32_t d1 = dis (gen);
    uint16_t d2 = dis (gen) & 0xFFFF;
    uint16_t d3 = dis (gen) & 0xFFFF;
    uint16_t d4 = dis (gen) & 0xFFFF;
    uint64_t d5 = (((uint64_t)dis (gen) << 32) | dis (gen));

    d3 = (d3 & 0x0FFF) | 0x4000;
    d4 = (d4 & 0x3FFF) | 0x8000;

    std::ostringstream oss;
    oss << std::hex << std::setfill ('0') << std::nouppercase << std::setw (8)
        << d1 << "-" << std::setw (4) << d2 << "-" << std::setw (4) << d3
        << "-" << std::setw (4) << d4 << "-" << std::setw (12)
        << (d5 & 0xFFFFFFFFFFFFULL);

    return oss.str ();
}

HybridMediaStreamTrack::~HybridMediaStreamTrack ()
{
    unsubscribe (subscriptionId);
    subscriptionId = -1;
}

std::string HybridMediaStreamTrack::getId () { return id; }

std::string HybridMediaStreamTrack::getKind () { return kind; }

MediaStreamTrackState HybridMediaStreamTrack::getReadyState ()
{
    return readyState;
}

bool HybridMediaStreamTrack::getEnable () { return subscriptionId > 0; }

void HybridMediaStreamTrack::setEnable (bool enable)
{
    if (enable == getEnable ())
    {
        return;
    }
    if (enable)
    {
        std::string dstPipeId = this->dstPipeId;
        subscriptionId = subscribe (
            { srcPipeId },
            [dstPipeId] (std::string, int, const FFmpeg::Frame &frame)
            { publish (dstPipeId, frame); });
    }
    else
    {
        unsubscribe (subscriptionId);
        subscriptionId = -1;
    }
}

std::string HybridMediaStreamTrack::get_dstPipId () { return dstPipeId; }

void HybridMediaStreamTrack::stop ()
{
    readyState = MediaStreamTrackState::ENDED;
    setEnable (false);
    if (mockThread.joinable ())
    {
        mockThread.join ();
    }
}

void HybridMediaStreamTrack::addMocker ()
{
    readyState = MediaStreamTrackState::LIVE;
    mockThread = std::thread (
        [this] ()
        {
            while (readyState == MediaStreamTrackState::LIVE)
            {
                if (kind == "audio")
                {
                    constexpr int AUDIO_SAMPLE_RATE = 48000;
                    constexpr int NB_SAMPLES = 960;
                    constexpr int DELAY_MS = 20;
                    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, AUDIO_SAMPLE_RATE,
                                         2, NB_SAMPLES);
                    frame.fillNoise ();
                    publish (srcPipeId, frame);
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (DELAY_MS));
                }
                else if (kind == "video")
                {
                    constexpr int WIDTH = 640;
                    constexpr int HEIGHT = 480;
                    FFmpeg::Frame frame (AV_PIX_FMT_RGB24, WIDTH, HEIGHT);
                    frame.fillNoise ();
                    publish (srcPipeId, frame);
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (33));
                }
            }
        });
    mockThread.detach ();
}
