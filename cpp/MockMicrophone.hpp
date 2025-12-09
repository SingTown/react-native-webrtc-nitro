#pragma once
#include "FramePipe.hpp"
#include <thread>

class MockMicrophone
{
  private:
    std::thread mockThread;
    std::atomic<bool> running = true;

  public:
    MockMicrophone (const std::string &srcPipeId)
    {
        mockThread = std::thread (
            [this, srcPipeId] ()
            {
                constexpr int AUDIO_SAMPLE_RATE = 48000;
                constexpr int NB_SAMPLES = 960;
                constexpr int DELAY_MS = 20;
                while (running.load ())
                {
                    FFmpeg::Frame frame (AV_SAMPLE_FMT_S16, AUDIO_SAMPLE_RATE,
                                         2, NB_SAMPLES);
                    frame.fillNoise ();
                    publish (srcPipeId, frame);
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (DELAY_MS));
                }
            });
    }

    void dispose ()
    {
        running.store (false);
        if (mockThread.joinable ())
        {
            mockThread.join ();
        }
    }

    ~MockMicrophone () { dispose (); }
};
