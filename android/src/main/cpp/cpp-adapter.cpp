#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include "WebrtcOnLoad.hpp"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <chrono>
#include <jni.h>
#include <string>

JavaVM *gJvm = nullptr;

JNIEXPORT auto JNICALL JNI_OnLoad (JavaVM *vm, void *) -> jint
{
    gJvm = vm;
    return margelo::nitro::webrtc::initialize (vm);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webrtc_HybridWebrtcView_unsubscribe (JNIEnv *, jobject,
                                              jint subscriptionId)
{
    unsubscribe (subscriptionId);
}

extern "C" JNIEXPORT int JNICALL
Java_com_webrtc_HybridWebrtcView_subscribeAudio (JNIEnv *env, jobject,
                                                 jstring pipeId, jobject track)
{
    auto resampler = std::make_shared<FFmpeg::Resampler> ();
    jobject trackGlobal = env->NewGlobalRef (track);

    FrameCallback callback
        = [trackGlobal, resampler] (const std::string &, int,
                                    const FFmpeg::Frame &raw)
    {
        JNIEnv *env;
        gJvm->AttachCurrentThread (&env, nullptr);

        FFmpeg::Frame frame
            = resampler->resample (raw, AV_SAMPLE_FMT_S16, 48000, 2);
        auto *sample = reinterpret_cast<const jbyte *> (frame->data[0]);
        int length = frame->nb_samples * 2 * 2;
        jbyteArray byteArray = env->NewByteArray (length);
        env->SetByteArrayRegion (byteArray, 0, length, sample);

        jclass audioTrackCls = env->GetObjectClass (trackGlobal);
        jmethodID writeMethod
            = env->GetMethodID (audioTrackCls, "write", "([BII)I");
        env->CallIntMethod (trackGlobal, writeMethod, byteArray, 0, length);
    };

    CleanupCallback cleanup = [trackGlobal] (int)
    {
        JNIEnv *env;
        gJvm->AttachCurrentThread (&env, nullptr);
        env->DeleteGlobalRef (trackGlobal);
    };

    std::string pipeIdStr (env->GetStringUTFChars (pipeId, nullptr));
    return subscribe ({ pipeIdStr }, callback);
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
