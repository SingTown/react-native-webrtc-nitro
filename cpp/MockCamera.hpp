#pragma once
#include "FramePipe.hpp"
#include <thread>

class MockCamera
{
  private:
    std::thread mockThread;
    std::atomic<bool> running = true;

  public:
    MockCamera (const std::string &srcPipeId)
    {
        mockThread = std::thread (
            [this, srcPipeId] ()
            {
                constexpr int WIDTH = 640;
                constexpr int HEIGHT = 480;
                while (running.load ())
                {
                    FFmpeg::Frame frame (AV_PIX_FMT_RGB24, WIDTH, HEIGHT);
                    frame.fillNoise ();
                    publish (srcPipeId, frame);
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (33));
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

    ~MockCamera () { dispose (); }
};
