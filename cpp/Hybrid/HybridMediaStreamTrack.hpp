#pragma once
#include "HybridMediaStreamTrackSpec.hpp"
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace margelo::nitro::webrtc
{
    auto uuidv4 () -> std::string;

    class HybridMediaStreamTrack : public HybridMediaStreamTrackSpec
    {
      public:
        std::string id;
        std::string kind;
        MediaStreamTrackState readyState = MediaStreamTrackState::ENDED;
        std::string srcPipeId;
        std::string dstPipeId;
        int subscriptionId = -1;

        std::thread mockThread;

        HybridMediaStreamTrack ()
            : HybridObject (TAG), HybridMediaStreamTrackSpec ()
        {
            id = uuidv4 ();
            srcPipeId = uuidv4 ();
            dstPipeId = uuidv4 ();
            setEnable (true);
        }

        ~HybridMediaStreamTrack () override;

        std::string getId () override;
        std::string getKind () override;
        MediaStreamTrackState getReadyState () override;
        bool getEnable () override;
        void setEnable (bool enable) override;
        std::string get_dstPipId () override;
        void stop () override;

        void addMocker ();
    };
} // namespace margelo::nitro::webrtc
