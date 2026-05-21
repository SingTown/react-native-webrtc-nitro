#include "FFmpeg.hpp"
#include <filesystem>
#include <gtest/gtest.h>

using namespace FFmpeg;

constexpr int SAMPLE_RATE = 48000;
constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;

TEST (EncoderTest, testOpusOneFrame)
{
    constexpr int NB_SAMPLES = 960;
    Encoder encoder (AV_CODEC_ID_OPUS);
    Frame inputFrame (AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, 2, NB_SAMPLES);
    inputFrame.fillNoise ();

    encoder.send (inputFrame);
    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0) << "Opus encoder should produce packets";
}

TEST (EncoderTest, testOpusMultiFrame)
{
    constexpr int NB_SAMPLES = 960;
    constexpr int NB_FRAMES = 10;

    Encoder encoder (AV_CODEC_ID_OPUS);

    for (int i = 0; i < NB_FRAMES; ++i)
    {
        Frame inputFrame (AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, 2, NB_SAMPLES);
        inputFrame.fillNoise ();
        encoder.send (inputFrame);
    }

    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0)
        << "Opus encoder should handle multi-frame input";
}

TEST (EncoderTest, testOpusBigFrame)
{
    constexpr int NB_SAMPLES = 960000;
    Encoder encoder (AV_CODEC_ID_OPUS);
    Frame inputFrame (AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, 2, NB_SAMPLES);
    inputFrame.fillNoise ();

    encoder.send (inputFrame);
    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0)
        << "Opus encoder should handle large frames";
}

TEST (EncoderTest, testH264OneFrame)
{
    Encoder encoder (AV_CODEC_ID_H264);
    Frame inputFrame (AV_PIX_FMT_NV12, WIDTH, HEIGHT);
    inputFrame.fillNoise ();

    encoder.send (inputFrame);
    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0) << "H264 encoder should produce packets";
}

TEST (EncoderTest, testH264MultiFrame)
{
    constexpr int NB_FRAMES = 10;
    Encoder encoder (AV_CODEC_ID_H264);

    for (int i = 0; i < NB_FRAMES; ++i)
    {
        Frame inputFrame (AV_PIX_FMT_NV12, WIDTH, HEIGHT);
        inputFrame.fillNoise ();
        encoder.send (inputFrame);
    }

    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0)
        << "H264 encoder should handle multi-frame input";
}

TEST (EncoderTest, testH265OneFrame)
{
    Encoder encoder (AV_CODEC_ID_H265);
    Frame inputFrame (AV_PIX_FMT_NV12, WIDTH, HEIGHT);
    inputFrame.fillNoise ();

    encoder.send (inputFrame);
    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0) << "H265 encoder should produce packets";
}

TEST (EncoderTest, testH265MultiFrame)
{
    constexpr int NB_FRAMES = 10;
    Encoder encoder (AV_CODEC_ID_H265);

    for (int i = 0; i < NB_FRAMES; ++i)
    {
        Frame inputFrame (AV_PIX_FMT_NV12, WIDTH, HEIGHT);
        inputFrame.fillNoise ();
        encoder.send (inputFrame);
    }

    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0)
        << "H265 encoder should handle multi-frame input";
}

TEST (EncoderTest, testAACOneFrame)
{
    constexpr int NB_SAMPLES = 1024;
    Encoder encoder (AV_CODEC_ID_AAC);
    Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1, NB_SAMPLES);
    inputFrame.fillNoise ();

    encoder.send (inputFrame);
    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0) << "AAC encoder should produce packets";
}

TEST (EncoderTest, testAACMultiFrame)
{
    constexpr int NB_SAMPLES = 1024;
    constexpr int NB_FRAMES = 10;

    Encoder encoder (AV_CODEC_ID_AAC);

    for (int i = 0; i < NB_FRAMES; ++i)
    {
        Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1, NB_SAMPLES);
        inputFrame.fillNoise ();
        encoder.send (inputFrame);
    }

    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0)
        << "AAC encoder should handle multi-frame input";
}

TEST (EncoderTest, testAACBigFrame)
{
    constexpr int NB_SAMPLES = 1024000;
    Encoder encoder (AV_CODEC_ID_AAC);
    Frame inputFrame (AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 1, NB_SAMPLES);
    inputFrame.fillNoise ();

    encoder.send (inputFrame);
    encoder.flush ();
    std::vector<Packet> packets = encoder.receive ();

    ASSERT_GT (packets.size (), 0) << "AAC encoder should handle large frames";
}

TEST (EncoderTest, testPng)
{
    Encoder encoder (AV_CODEC_ID_PNG);
    Frame inputFrame (AV_PIX_FMT_NV12, WIDTH, HEIGHT);
    inputFrame.fillNoise ();

    std::string path = testing::TempDir () + "out.png";

    // std::string file = "out.png";
    FILE *file = fopen (path.c_str (), "wb");
    if (file == nullptr)
    {
        FAIL () << "Could not open file: " << path;
        return;
    }

    encoder.send (inputFrame);
    encoder.flush ();
    for (auto &pkt : encoder.receive ())
    {
        fwrite (pkt->data, 1, pkt->size, file);
    }
    fclose (file);

    ASSERT_TRUE (std::filesystem::exists (path));

    auto fileSize = std::filesystem::file_size (path);
    ASSERT_GT (fileSize, 0) << "PNG file should not be empty";

    std::filesystem::remove (path);
}
