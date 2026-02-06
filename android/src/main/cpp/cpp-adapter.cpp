#include "FFmpeg.hpp"
#include "FramePipe.hpp"
#include "WebrtcOnLoad.hpp"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <cstring>
#include <jni.h>
#include <string>
#include <cstdint>

JavaVM *gJvm = nullptr;

struct VideoTransform
{
    static auto normalizeRotation (int rotation) -> int
    {
        int normalized = rotation % 360;
        if (normalized < 0)
        {
            normalized += 360;
        }
        switch (normalized)
        {
            case 0:
            case 90:
            case 180:
            case 270:
                return normalized;
            default:
                return 0;
        }
    }

    static void copySample (uint8_t *dst, const uint8_t *src,
                            int bytesPerSample)
    {
        if (bytesPerSample == 1)
        {
            *dst = *src;
        }
        else
        {
            dst[0] = src[0];
            dst[1] = src[1];
        }
    }

    static void rotateMirrorPlane (const uint8_t *src, int srcStride,
                                   uint8_t *dst, int dstStride, int srcW,
                                   int srcH, int rotation, bool mirror,
                                   int bytesPerSample)
    {
        int dstW = (rotation == 90 || rotation == 270) ? srcH : srcW;
        int dstH = (rotation == 90 || rotation == 270) ? srcW : srcH;

        if (rotation == 0 && !mirror)
        {
            const int rowBytes = srcW * bytesPerSample;
            for (int y = 0; y < srcH; ++y)
            {
                memcpy (dst + y * dstStride, src + y * srcStride, rowBytes);
            }
            return;
        }

        for (int y = 0; y < dstH; ++y)
        {
            uint8_t *dstRow = dst + y * dstStride;
            for (int x = 0; x < dstW; ++x)
            {
                int xTransformed = mirror ? (dstW - 1 - x) : x;
                int srcX = 0;
                int srcY = 0;
                switch (rotation)
                {
                    case 0:
                        srcX = xTransformed;
                        srcY = y;
                        break;
                    case 90:
                        srcX = y;
                        srcY = srcH - 1 - xTransformed;
                        break;
                    case 180:
                        srcX = srcW - 1 - xTransformed;
                        srcY = srcH - 1 - y;
                        break;
                    case 270:
                    default:
                        srcX = srcW - 1 - y;
                        srcY = xTransformed;
                        break;
                }
                const uint8_t *srcSample
                    = src + srcY * srcStride + srcX * bytesPerSample;
                copySample (dstRow + x * bytesPerSample, srcSample,
                            bytesPerSample);
            }
        }
    }

    static void rotateMirrorNV12 (const FFmpeg::Frame &src,
                                  FFmpeg::Frame &dst, int rotation,
                                  bool mirror)
    {
        rotateMirrorPlane (src->data[0], src->linesize[0], dst->data[0],
                           dst->linesize[0], src->width, src->height, rotation,
                           mirror, 1);
        rotateMirrorPlane (src->data[1], src->linesize[1], dst->data[1],
                           dst->linesize[1], src->width / 2, src->height / 2,
                           rotation, mirror, 2);
    }

    static void blitNV12 (const FFmpeg::Frame &src, FFmpeg::Frame &dst,
                          int dstX, int dstY)
    {
        const int srcW = src->width;
        const int srcH = src->height;
        for (int y = 0; y < srcH; ++y)
        {
            const uint8_t *srcRow = src->data[0] + y * src->linesize[0];
            uint8_t *dstRow
                = dst->data[0] + (dstY + y) * dst->linesize[0] + dstX;
            memcpy (dstRow, srcRow, srcW);
        }

        const int srcUVH = srcH / 2;
        const int srcUVBytes = srcW;
        const int dstUVX = dstX / 2;
        const int dstUVY = dstY / 2;
        for (int y = 0; y < srcUVH; ++y)
        {
            const uint8_t *srcRow = src->data[1] + y * src->linesize[1];
            uint8_t *dstRow
                = dst->data[1] + (dstUVY + y) * dst->linesize[1] + dstUVX * 2;
            memcpy (dstRow, srcRow, srcUVBytes);
        }
    }

    static auto scaleCenterCropNV12 (const FFmpeg::Frame &src, int targetW,
                                     int targetH) -> FFmpeg::Frame
    {
        static thread_local FFmpeg::Scaler scaler;
        const int srcW = src->width;
        const int srcH = src->height;

        if (srcW == targetW && srcH == targetH)
        {
            return src;
        }

        double scaleW = static_cast<double>(targetW) / srcW;
        double scaleH = static_cast<double>(targetH) / srcH;
        double scale = scaleW > scaleH ? scaleW : scaleH;
        int scaledW = static_cast<int>(srcW * scale);
        int scaledH = static_cast<int>(srcH * scale);

        scaledW &= ~1;
        scaledH &= ~1;
        if (scaledW < 2)
        {
            scaledW = 2;
        }
        if (scaledH < 2)
        {
            scaledH = 2;
        }

        FFmpeg::Frame scaled
            = (scaledW == srcW && scaledH == srcH)
                  ? src
                  : scaler.scale (src, AV_PIX_FMT_NV12, scaledW, scaledH);
        FFmpeg::Frame dst (AV_PIX_FMT_NV12, targetW, targetH, src->pts);

        int srcX = (scaled->width - targetW) / 2;
        int srcY = (scaled->height - targetH) / 2;
        srcX &= ~1;
        srcY &= ~1;

        for (int y = 0; y < targetH; ++y)
        {
            const uint8_t *srcRow
                = scaled->data[0] + (srcY + y) * scaled->linesize[0] + srcX;
            uint8_t *dstRow = dst->data[0] + y * dst->linesize[0];
            memcpy (dstRow, srcRow, targetW);
        }

        const int uvH = targetH / 2;
        const int srcUVX = srcX / 2;
        const int srcUVY = srcY / 2;
        for (int y = 0; y < uvH; ++y)
        {
            const uint8_t *srcRow = scaled->data[1]
                                    + (srcUVY + y) * scaled->linesize[1]
                                    + srcUVX * 2;
            uint8_t *dstRow = dst->data[1] + y * dst->linesize[1];
            memcpy (dstRow, srcRow, targetW);
        }

        return dst;
    }
};

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

extern "C" JNIEXPORT auto JNICALL Java_com_webrtc_Camera_publishVideo (
    JNIEnv *env, jobject, jobjectArray pipeIds, jobject image, jint rotation,
    jboolean mirror) -> void
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

    int normalizedRotation = VideoTransform::normalizeRotation (rotation);
    bool mirrorFrame = mirror == JNI_TRUE;
    bool needsTransform = normalizedRotation != 0 || mirrorFrame;
    FFmpeg::Frame outputFrame = frame;
    if (needsTransform)
    {
        int outWidth
            = (normalizedRotation == 90 || normalizedRotation == 270) ? height
                                                                      : width;
        int outHeight
            = (normalizedRotation == 90 || normalizedRotation == 270) ? width
                                                                      : height;
        outputFrame
            = FFmpeg::Frame (AV_PIX_FMT_NV12, outWidth, outHeight, frame->pts);
        VideoTransform::rotateMirrorNV12 (frame, outputFrame,
                                          normalizedRotation, mirrorFrame);
    }
    if (normalizedRotation == 90 || normalizedRotation == 270)
    {
        outputFrame = VideoTransform::scaleCenterCropNV12 (outputFrame, height,
                                                           width);
    }

    jsize pipeIdsLength = env->GetArrayLength (pipeIds);
    for (jsize i = 0; i < pipeIdsLength; ++i)
    {
        auto pipeId = (jstring)env->GetObjectArrayElement (pipeIds, i);
        const char *cstr = env->GetStringUTFChars (pipeId, nullptr);
        std::string pipeIdStr (cstr);
        publish (pipeIdStr, outputFrame);
        env->ReleaseStringUTFChars (pipeId, cstr);
        env->DeleteLocalRef (pipeId);
    }
}
