#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include "WebrtcOnLoad.hpp"
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fbjni/fbjni.h>
#include <jni.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

JavaVM *gJvm = nullptr;

namespace
{
    constexpr int kMicrophoneSampleRate = 48000;
    constexpr int kMicrophoneChannelCount = 1;
    std::mutex gMicrophoneAudioFiltersMutex;
    std::unordered_map<std::string, std::shared_ptr<FFmpeg::AudioFilter>>
        gMicrophoneAudioFilters;

    // Elastic buffer for speaker output: the decode thread only enqueues
    // and returns immediately, while a dedicated thread feeds AudioTrack
    // with blocking writes (paced by hardware back-pressure). The queue
    // absorbs network jitter without dropping samples or blocking decode;
    // when it exceeds the cap, the oldest chunks are discarded so that
    // end-to-end latency stays bounded. On start and whenever the queue
    // drains, writing is held until the pre-buffer level is reached —
    // otherwise the steady-state level is ~0 and any jitter larger than
    // the hardware buffer causes an underrun (heard as stutter).
    constexpr size_t kSpeakerBytesPerSecond = 48000 * 2 * 2;
    constexpr size_t kSpeakerMaxQueuedBytes
        = kSpeakerBytesPerSecond * 400 / 1000;
    constexpr size_t kSpeakerPreBufferBytes
        = kSpeakerBytesPerSecond * 200 / 1000;

    class AudioTrackWriter
    {
      public:
        // trackGlobal must be a NewGlobalRef reference; ownership moves
        // to this object and the writer thread releases it before exit.
        explicit AudioTrackWriter (jobject trackGlobal)
            : trackGlobal (trackGlobal), thread ([this] { run (); })
        {
        }

        ~AudioTrackWriter () { stop (); }

        void enqueue (std::vector<uint8_t> &&chunk)
        {
            if (chunk.empty ())
            {
                return;
            }
            {
                std::lock_guard<std::mutex> lock (mutex);
                if (!running)
                {
                    return;
                }
                queuedBytes += chunk.size ();
                queue.push_back (std::move (chunk));
                while (queuedBytes > kSpeakerMaxQueuedBytes
                       && queue.size () > 1)
                {
                    queuedBytes -= queue.front ().size ();
                    queue.pop_front ();
                }
                if (buffering && queuedBytes >= kSpeakerPreBufferBytes)
                {
                    buffering = false;
                }
            }
            cv.notify_one ();
        }

        void stop ()
        {
            {
                std::lock_guard<std::mutex> lock (mutex);
                running = false;
            }
            cv.notify_all ();
            if (thread.joinable ())
            {
                thread.join ();
            }
        }

      private:
        void run ()
        {
            JNIEnv *env = nullptr;
            gJvm->AttachCurrentThread (&env, nullptr);

            jclass audioTrackCls = env->GetObjectClass (trackGlobal);
            jmethodID writeMethod
                = env->GetMethodID (audioTrackCls, "write", "([BIII)I");
            jfieldID writeBlockingField
                = env->GetStaticFieldID (audioTrackCls, "WRITE_BLOCKING", "I");
            jint WRITE_BLOCKING
                = env->GetStaticIntField (audioTrackCls, writeBlockingField);

            while (true)
            {
                std::vector<uint8_t> chunk;
                {
                    std::unique_lock<std::mutex> lock (mutex);
                    cv.wait (lock,
                             [this]
                             {
                                 return !running
                                        || (!buffering && !queue.empty ());
                             });
                    if (!running)
                    {
                        break;
                    }
                    chunk = std::move (queue.front ());
                    queue.pop_front ();
                    queuedBytes -= chunk.size ();
                    if (queue.empty ())
                    {
                        // Cushion exhausted, rebuild it; the hardware still
                        // holds up to 80ms to play out.
                        buffering = true;
                        __android_log_print (
                            ANDROID_LOG_INFO, "WebrtcSpeaker",
                            "queue drained, rebuffering to %zu bytes",
                            kSpeakerPreBufferBytes);
                    }
                }

                auto length = static_cast<jsize> (chunk.size ());
                jbyteArray byteArray = env->NewByteArray (length);
                env->SetByteArrayRegion (
                    byteArray, 0, length,
                    reinterpret_cast<const jbyte *> (chunk.data ()));
                jint written
                    = env->CallIntMethod (trackGlobal, writeMethod, byteArray,
                                          0, length, WRITE_BLOCKING);
                env->DeleteLocalRef (byteArray);
                if (env->ExceptionCheck ())
                {
                    env->ExceptionClear ();
                    break;
                }
                if (written < 0)
                {
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (20));
                }
            }

            env->DeleteLocalRef (audioTrackCls);
            env->DeleteGlobalRef (trackGlobal);
            gJvm->DetachCurrentThread ();
        }

        jobject trackGlobal;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::vector<uint8_t>> queue;
        size_t queuedBytes = 0;
        bool running = true;
        bool buffering = true;
        // Must be declared last: all other members have to be constructed
        // before the thread starts
        std::thread thread;
    };
}

JNIEXPORT auto JNICALL JNI_OnLoad (JavaVM *vm, void *) -> jint
{
    gJvm = vm;
    return facebook::jni::initialize (
        vm, [] () { margelo::nitro::webrtc::registerAllNatives (); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_webrtc_HybridWebrtcView_unsubscribe (JNIEnv *, jobject,
                                              jint subscriptionId)
{
    unsubscribe (subscriptionId);
}

extern "C" JNIEXPORT auto JNICALL
Java_com_webrtc_HybridWebrtcView_subscribeAudio (JNIEnv *env, jobject,
                                                 jstring pipeId, jobject track)
    -> int
{
    auto resampler = std::make_shared<FFmpeg::Resampler> ();
    jobject trackGlobal = env->NewGlobalRef (track);
    auto writer = std::make_shared<AudioTrackWriter> (trackGlobal);

    FrameCallback callback = [writer, resampler] (const std::string &, int,
                                                  const FFmpeg::Frame &raw)
    {
        FFmpeg::Frame frame
            = resampler->resample (raw, AV_SAMPLE_FMT_S16, 48000, 2);
        auto *sample = reinterpret_cast<const uint8_t *> (frame->data[0]);
        size_t length = static_cast<size_t> (frame->nb_samples) * 2 * 2;
        writer->enqueue (std::vector<uint8_t> (sample, sample + length));
    };

    CleanupCallback cleanup = [writer] (int) { writer->stop (); };

    std::string pipeIdStr (env->GetStringUTFChars (pipeId, nullptr));
    return subscribe ({ pipeIdStr }, callback, cleanup);
}

extern "C" JNIEXPORT auto JNICALL
Java_com_webrtc_HybridWebrtcView_subscribeVideo (JNIEnv *env, jobject,
                                                 jstring pipeId,
                                                 jobject surface) -> jint
{
    if (!surface)
    {
        return -1;
    }
    ANativeWindow *window = ANativeWindow_fromSurface (env, surface);
    if (!window)
    {
        return -1;
    }

    auto scaler = std::make_shared<FFmpeg::Scaler> ();
    FrameCallback callback
        = [window, scaler] (const std::string &, int, const FFmpeg::Frame &raw)
    {
        FFmpeg::Frame frame
            = scaler->scale (raw, AV_PIX_FMT_RGBA, raw->width, raw->height);

        ANativeWindow_setBuffersGeometry (window, frame->width, frame->height,
                                          WINDOW_FORMAT_RGBA_8888);

        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock (window, &buffer, nullptr) < 0)
        {
            return;
        }

        auto *dst = static_cast<uint8_t *> (buffer.bits);
        for (int y = 0; y < frame->height; ++y)
        {
            uint8_t *srcRow = frame->data[0] + y * frame->linesize[0];
            uint8_t *dstRow = dst + y * buffer.stride * 4;
            memcpy (dstRow, srcRow, frame->width * 4);
        }

        ANativeWindow_unlockAndPost (window);
    };
    CleanupCallback cleanup
        = [window] (int) { ANativeWindow_release (window); };
    std::string pipeIdStr (env->GetStringUTFChars (pipeId, nullptr));
    return subscribe ({ pipeIdStr }, callback, cleanup);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webrtc_HybridMicrophone_publishAudio (JNIEnv *env, jobject,
                                               jstring pipeId,
                                               jbyteArray audioBuffer,
                                               jint size)

{
    if (pipeId == nullptr || audioBuffer == nullptr || size <= 0)
    {
        return;
    }

    const char *pipeIdChars = env->GetStringUTFChars (pipeId, nullptr);
    if (pipeIdChars == nullptr)
    {
        return;
    }
    std::string pipeIdStr (pipeIdChars);
    env->ReleaseStringUTFChars (pipeId, pipeIdChars);
    if (pipeIdStr.empty ())
    {
        return;
    }

    auto frame
        = FFmpeg::Frame (AV_SAMPLE_FMT_S16, kMicrophoneSampleRate,
                         kMicrophoneChannelCount, size / sizeof (int16_t));
    env->GetByteArrayRegion (audioBuffer, 0, size,
                             reinterpret_cast<jbyte *> (frame->data[0]));
    if (env->ExceptionCheck ())
    {
        return;
    }

    std::shared_ptr<FFmpeg::AudioFilter> audioFilter;
    {
        std::lock_guard lock (gMicrophoneAudioFiltersMutex);
        auto existing = gMicrophoneAudioFilters.find (pipeIdStr);
        if (existing != gMicrophoneAudioFilters.end ())
        {
            audioFilter = existing->second;
        }
        else
        {
            audioFilter = std::make_shared<FFmpeg::AudioFilter> ();
            gMicrophoneAudioFilters.emplace (pipeIdStr, audioFilter);
        }
    }

    std::vector<FFmpeg::Frame> filteredFrames = audioFilter->filter (frame);
    for (const FFmpeg::Frame &filteredFrame : filteredFrames)
    {
        publish (pipeIdStr, filteredFrame);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_webrtc_HybridMicrophone_resetAudioFilter (JNIEnv *env, jobject,
                                                   jstring pipeId)
{
    if (pipeId == nullptr)
    {
        return;
    }

    const char *pipeIdChars = env->GetStringUTFChars (pipeId, nullptr);
    if (pipeIdChars == nullptr)
    {
        return;
    }
    std::string pipeIdStr (pipeIdChars);
    env->ReleaseStringUTFChars (pipeId, pipeIdChars);
    if (pipeIdStr.empty ())
    {
        return;
    }

    std::lock_guard lock (gMicrophoneAudioFiltersMutex);
    gMicrophoneAudioFilters.erase (pipeIdStr);
}

extern "C" JNIEXPORT void JNICALL Java_com_webrtc_Camera_publishVideo (
    JNIEnv *env, jobject, jobjectArray pipeIds, jobject image)
{
    jclass imageClass = env->GetObjectClass (image);
    jmethodID getWidthMethod
        = env->GetMethodID (imageClass, "getWidth", "()I");
    jmethodID getHeightMethod
        = env->GetMethodID (imageClass, "getHeight", "()I");
    jint width = env->CallIntMethod (image, getWidthMethod);
    jint height = env->CallIntMethod (image, getHeightMethod);

    jmethodID getPlanesMethod = env->GetMethodID (
        imageClass, "getPlanes", "()[Landroid/media/Image$Plane;");
    auto planeArray
        = (jobjectArray)env->CallObjectMethod (image, getPlanesMethod);

    jobject yPlane = env->GetObjectArrayElement (planeArray, 0);
    jobject uPlane = env->GetObjectArrayElement (planeArray, 1);
    jobject vPlane = env->GetObjectArrayElement (planeArray, 2);
    jclass planeClass = env->GetObjectClass (yPlane);
    jmethodID getBufferMethod = env->GetMethodID (planeClass, "getBuffer",
                                                  "()Ljava/nio/ByteBuffer;");
    jmethodID getRowStrideMethod
        = env->GetMethodID (planeClass, "getRowStride", "()I");
    jmethodID getPixelStrideMethod
        = env->GetMethodID (planeClass, "getPixelStride", "()I");

    jobject yByteBuffer = env->CallObjectMethod (yPlane, getBufferMethod);
    auto *yBufferPtr
        = static_cast<uint8_t *> (env->GetDirectBufferAddress (yByteBuffer));
    jint yRowStride = env->CallIntMethod (yPlane, getRowStrideMethod);

    jobject uByteBuffer = env->CallObjectMethod (uPlane, getBufferMethod);
    auto *uBufferPtr
        = static_cast<uint8_t *> (env->GetDirectBufferAddress (uByteBuffer));
    jint uRowStride = env->CallIntMethod (uPlane, getRowStrideMethod);
    jint uPixelStride = env->CallIntMethod (uPlane, getPixelStrideMethod);

    jobject vByteBuffer = env->CallObjectMethod (vPlane, getBufferMethod);
    auto *vBufferPtr
        = static_cast<uint8_t *> (env->GetDirectBufferAddress (vByteBuffer));
    jint vRowStride = env->CallIntMethod (vPlane, getRowStrideMethod);
    jint vPixelStride = env->CallIntMethod (vPlane, getPixelStrideMethod);

    FFmpeg::Frame frame (AV_PIX_FMT_NV12, width, height);

    // Copy Y
    for (int y = 0; y < height; ++y)
    {
        memcpy (frame->data[0] + y * frame->linesize[0],
                yBufferPtr + y * yRowStride, width);
    }

    // Copy UV
    for (int y = 0; y < height / 2; ++y)
    {
        for (int x = 0; x < width / 2; ++x)
        {
            frame->data[1][y * frame->linesize[1] + x * 2]
                = uBufferPtr[y * uRowStride + x * uPixelStride];
            frame->data[1][y * frame->linesize[1] + x * 2 + 1]
                = vBufferPtr[y * vRowStride + x * vPixelStride];
        }
    }

    env->DeleteLocalRef (yPlane);
    env->DeleteLocalRef (uPlane);
    env->DeleteLocalRef (vPlane);
    env->DeleteLocalRef (planeArray);
    env->DeleteLocalRef (imageClass);
    env->DeleteLocalRef (planeClass);

    jsize pipeIdsLength = env->GetArrayLength (pipeIds);
    for (jsize i = 0; i < pipeIdsLength; ++i)
    {
        auto pipeId = (jstring)env->GetObjectArrayElement (pipeIds, i);
        const char *cstr = env->GetStringUTFChars (pipeId, nullptr);
        std::string pipeIdStr (cstr);
        publish (pipeIdStr, frame);
        env->ReleaseStringUTFChars (pipeId, cstr);
        env->DeleteLocalRef (pipeId);
    }
}
