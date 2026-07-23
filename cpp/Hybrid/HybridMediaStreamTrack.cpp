#include "HybridMediaStreamTrack.hpp"
#include "FramePipe.hpp"
#include <iomanip>
#include <random>

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

void HybridMediaStreamTrack::enable ()
{
    if (getEnabled ())
    {
        return;
    }
    std::string dstPipeId = this->dstPipeId;
    subscriptionId
        = subscribe ({ srcPipeId }, [dstPipeId] (const std::string &, int,
                                                 const FFmpeg::Frame &frame)
                     { publish (dstPipeId, frame); });
}

void HybridMediaStreamTrack::disable ()
{
    if (!getEnabled ())
    {
        return;
    }
    unsubscribe (subscriptionId);
    subscriptionId = -1;
}

auto HybridMediaStreamTrack::getEnabled () -> bool
{
    return subscriptionId > 0;
}

void HybridMediaStreamTrack::setEnabled (bool enabled)
{
    if (enabled)
    {
        this->enable ();
    }
    else
    {
        this->disable ();
    }
};

auto HybridMediaStreamTrack::switchCamera (FacingMode facingMode)
    -> std::shared_ptr<Promise<void>>
{
    if (kind != "video")
    {
        throw std::runtime_error ("Only implemented for video tracks");
    }
    if (camera)
    {
        return camera->switchCamera (facingMode);
    }
    if (mockCamera)
    {
        return Promise<void>::resolved ();
    }
    throw std::runtime_error ("Not implemented for remote tracks");
}
