#include "HybridRTCRtpReceiver.hpp"

using namespace margelo::nitro::webrtc;

std::variant<margelo::nitro::NullType,
             std::shared_ptr<HybridMediaStreamTrackSpec>>
HybridRTCRtpReceiver::getTrack ()
{
    if (track)
    {
        return track;
    }
    else
    {
        return nitro::null;
    }
}

static auto getAttributeTrackid (const std::string &attribute) -> std::string
{
    std::regex re (R"(msid:([^\s]+)(?:\s+([^\s]+))?)");
    std::smatch match;
    if (std::regex_match (attribute, match, re) && match.size () == 3)
    {
        return match[2];
    }
    return "";
}

auto getAttributeMsid (const std::string &attribute) -> std::string
{
    std::regex re (R"(msid:([^\s]+)(?:\s+([^\s]+))?)");
    std::smatch match;
    if (std::regex_match (attribute, match, re) && match.size () == 3)
    {
        return match[1];
    }
    return "";
}

static auto getMediaTrackid (const rtc::Description::Media &media)
    -> std::string
{
    for (std::string &attr : media.attributes ())
    {
        std::string trackid = getAttributeTrackid (attr);
        if (!trackid.empty ())
        {
            return trackid;
        }
    }
    return "";
}

auto getMediaMsids (const rtc::Description::Media &media)
    -> std::vector<std::string>
{
    std::vector<std::string> msids;
    for (std::string &attr : media.attributes ())
    {
        std::string msid = getAttributeMsid (attr);
        if (!msid.empty ())
        {
            msids.push_back (msid);
        }
    }
    return msids;
}

void HybridRTCRtpReceiver::receive (const rtc::Description::Media &media)
{
    if (!track)
    {
        return;
    }

    // update track id
    std::string trackid = getMediaTrackid (media);
    if (!trackid.empty ())
    {
        track->id = trackid;
    }

    // update streams
    auto msids = getMediaMsids (media);
    if (msids.empty ())
    {
        msids.push_back (uuidv4 ());
    }
    for (const std::string &msid : msids)
    {
        std::shared_ptr<HybridMediaStream> existingStream;
        for (const auto &stream : streams)
        {
            if (stream->id == msid)
            {
                existingStream = stream;
                break;
            }
        }
        if (!existingStream)
        {
            existingStream = std::make_shared<HybridMediaStream> ();
            existingStream->id = msid;
            streams.push_back (existingStream);
        }
        existingStream->addTrack (track);
    }
}
