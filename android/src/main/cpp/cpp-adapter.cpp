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

extern "C" JNIEXPORT auto JNICALL
Java_com_webrtc_HybridWebrtcView_subscribeAudio (JNIEnv *env, jobject,
                                                 jstring pipeId, jobject track)
    -> int
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
            = env->GetMethodID (audioTrackCls, "write", "([BIII)I");
        jfieldID writeNonBlockField
            = env->GetStaticFieldID (audioTrackCls, "WRITE_NON_BLOCKING", "I");
        jint WRITE_NON_BLOCKING
            = env->GetStaticIntField (audioTrackCls, writeNonBlockField);
        env->CallIntMethod (trackGlobal, writeMethod, byteArray, 0, length,
                            WRITE_NON_BLOCKING);
    };

    CleanupCallback cleanup = [trackGlobal] (int)
    {
        JNIEnv *env;
        gJvm->AttachCurrentThread (&env, nullptr);
        env->DeleteGlobalRef (trackGlobal);
    };

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
    auto frame = FFmpeg::Frame (AV_SAMPLE_FMT_S16, 48000, 1, size / 2);
    jboolean isCopy = JNI_FALSE;
    jbyte *audioData = env->GetByteArrayElements (audioBuffer, &isCopy);
    memcpy (frame->data[0], audioData, size);
    env->ReleaseByteArrayElements (audioBuffer, audioData, JNI_ABORT);

    std::string pipeIdStr (env->GetStringUTFChars (pipeId, nullptr));
    publish (pipeIdStr, frame);
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
