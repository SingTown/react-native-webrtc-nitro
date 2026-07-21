#import "FramePipeWrapper.h"
#include "FramePipe.hpp"
#include <string>

// Water-level control for the speaker queue: scheduleBuffer's internal
// queue is unbounded, so network jitter makes the backlog (i.e. the
// end-to-end latency) grow monotonically. We keep our own pending queue
// and account for the total buffered duration, discarding the oldest
// chunks once over the cap; only a small amount stays scheduled on the
// player, and each completion callback tops it up. All state is read
// and written on the provided serial queue only.
static const double kSpeakerMaxBufferedSeconds = 0.4;
static const double kSpeakerTargetScheduledSeconds = 0.2;

@interface SpeakerPacer : NSObject
- (instancetype)initWithPlayer:(AVAudioPlayerNode *)player
                         queue:(dispatch_queue_t)queue;
- (void)enqueue:(AVAudioPCMBuffer *)buffer;
@end

@implementation SpeakerPacer
{
    AVAudioPlayerNode *_player;
    dispatch_queue_t _queue;
    NSMutableArray<AVAudioPCMBuffer *> *_pending;
    double _pendingSeconds;
    double _scheduledSeconds;
}

- (instancetype)initWithPlayer:(AVAudioPlayerNode *)player
                         queue:(dispatch_queue_t)queue
{
    self = [super init];
    if (self)
    {
        _player = player;
        _queue = queue;
        _pending = [NSMutableArray new];
        _pendingSeconds = 0;
        _scheduledSeconds = 0;
    }
    return self;
}

- (void)enqueue:(AVAudioPCMBuffer *)buffer
{
    dispatch_async (_queue, ^{
      [self enqueueOnQueue:buffer];
    });
}

- (void)enqueueOnQueue:(AVAudioPCMBuffer *)buffer
{
    [_pending addObject:buffer];
    _pendingSeconds += [self durationOf:buffer];
    while (_scheduledSeconds + _pendingSeconds > kSpeakerMaxBufferedSeconds
           && _pending.count > 1)
    {
        _pendingSeconds -= [self durationOf:_pending.firstObject];
        [_pending removeObjectAtIndex:0];
    }
    [self pumpOnQueue];
}

- (void)pumpOnQueue
{
    while (_scheduledSeconds < kSpeakerTargetScheduledSeconds
           && _pending.count > 0)
    {
        AVAudioPCMBuffer *buffer = _pending.firstObject;
        [_pending removeObjectAtIndex:0];
        double duration = [self durationOf:buffer];
        _pendingSeconds -= duration;
        _scheduledSeconds += duration;
        __weak SpeakerPacer *weakSelf = self;
        [_player scheduleBuffer:buffer
                            atTime:nil
                           options:0
            completionCallbackType:AVAudioPlayerNodeCompletionDataPlayedBack
                 completionHandler:^(
                     AVAudioPlayerNodeCompletionCallbackType type) {
                   SpeakerPacer *strongSelf = weakSelf;
                   if (!strongSelf)
                   {
                       return;
                   }
                   dispatch_async (strongSelf->_queue, ^{
                     strongSelf->_scheduledSeconds -= duration;
                     [strongSelf pumpOnQueue];
                   });
                 }];
    }
}

- (double)durationOf:(AVAudioPCMBuffer *)buffer
{
    double sampleRate = buffer.format.sampleRate;
    if (sampleRate <= 0)
    {
        return 0;
    }
    return (double)buffer.frameLength / sampleRate;
}

@end

@implementation FramePipeWrapper

+ (void)cameraPublishVideo:(CMSampleBufferRef)sampleBuffer
                   pipeIds:(NSArray<NSString *> *)pipeIds
{
    if (pipeIds.count == 0)
    {
        return;
    }

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer (sampleBuffer);
    CVPixelBufferLockBaseAddress (pixelBuffer, kCVPixelBufferLock_ReadOnly);

    size_t width = CVPixelBufferGetWidth (pixelBuffer);
    size_t height = CVPixelBufferGetHeight (pixelBuffer);

    uint8_t *srcY
        = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane (pixelBuffer, 0);
    uint8_t *srcUV
        = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane (pixelBuffer, 1);
    int srcYStride = (int)CVPixelBufferGetBytesPerRowOfPlane (pixelBuffer, 0);
    int srcUVStride = (int)CVPixelBufferGetBytesPerRowOfPlane (pixelBuffer, 1);

    FFmpeg::Frame frame (AV_PIX_FMT_NV12, width, height);
    // Copy Y
    for (size_t i = 0; i < height; ++i)
    {
        memcpy (frame->data[0] + i * frame->linesize[0], srcY + i * srcYStride,
                width);
    }
    // Copy UV
    for (size_t i = 0; i < height / 2; ++i)
    {
        memcpy (frame->data[1] + i * frame->linesize[1],
                srcUV + i * srcUVStride, width);
    }

    CVPixelBufferUnlockBaseAddress (pixelBuffer, kCVPixelBufferLock_ReadOnly);

    for (NSString *pipeId in pipeIds)
    {
        publish (std::string ([pipeId UTF8String]), frame);
    }
}

+ (void)microphonePublishAudio:(CMSampleBufferRef)sampleBuffer
                       pipeIds:(NSArray<NSString *> *)pipeIds
{
    if (pipeIds.count == 0)
    {
        return;
    }

    CMFormatDescriptionRef formatDescription
        = CMSampleBufferGetFormatDescription (sampleBuffer);
    const AudioStreamBasicDescription *audioFormat
        = CMAudioFormatDescriptionGetStreamBasicDescription (
            formatDescription);

    if (audioFormat == NULL)
    {
        return;
    }

    if (audioFormat->mFormatID != kAudioFormatLinearPCM)
    {
        NSLog (@"[Microphone] Unsupported format: %u, expected Linear PCM",
               audioFormat->mFormatID);
        return;
    }

    int sampleRate = (int)audioFormat->mSampleRate;
    int channels = audioFormat->mChannelsPerFrame;
    CMItemCount numSamples = CMSampleBufferGetNumSamples (sampleBuffer);

    bool isInterleaved
        = !(audioFormat->mFormatFlags & kAudioFormatFlagIsNonInterleaved);

    int bitsPerChannel = audioFormat->mBitsPerChannel;

    AVSampleFormat sampleFormat;
    if (bitsPerChannel == 16)
    {
        sampleFormat = isInterleaved ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_S16P;
    }
    else if (bitsPerChannel == 32
             && (audioFormat->mFormatFlags & kAudioFormatFlagIsFloat))
    {
        sampleFormat = isInterleaved ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_FLTP;
    }
    else
    {
        NSLog (@"[Microphone] Unsupported bit depth: %d bits", bitsPerChannel);
        return;
    }

    FFmpeg::Frame frame (sampleFormat, sampleRate, channels, (int)numSamples);

    AudioBufferList audioBufferList;
    CMBlockBufferRef blockBufferOut = NULL;
    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer (
        sampleBuffer,
        NULL, // bufferListSizeNeededOut
        &audioBufferList, sizeof (audioBufferList),
        NULL, // blockBufferAllocator
        NULL, // blockBufferMemoryAllocator
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
        &blockBufferOut);
    if (status != noErr || blockBufferOut == NULL)
    {
        NSLog (@"[Microphone] Failed to get audio buffer list: %d", status);
        return;
    }

    UInt32 numBuffers = audioBufferList.mNumberBuffers;
    for (UInt32 i = 0; i < numBuffers; i++)
    {
        AudioBuffer *srcBuffer = &audioBufferList.mBuffers[i];
        memcpy (frame->data[i], srcBuffer->mData, srcBuffer->mDataByteSize);
    }

    CFRelease (blockBufferOut);

    for (NSString *pipeId in pipeIds)
    {
        publish (std::string ([pipeId UTF8String]), frame);
    }
}

+ (int)viewSubscribeVideo:(AVSampleBufferDisplayLayer *)displayLayer
                   pipeId:(NSString *)pipeId
{

    auto scaler = std::make_shared<FFmpeg::Scaler> ();
    auto callback
        = [displayLayer, scaler] (std::string pipeId, int subscriptionId,
                                  const FFmpeg::Frame &raw)
    {
        FFmpeg::Frame frame
            = scaler->scale (raw, AV_PIX_FMT_NV12, raw->width, raw->height);

        int width = frame->width;
        int height = frame->height;
        CMVideoFormatDescriptionRef formatDescription = NULL;
        OSStatus status = CMVideoFormatDescriptionCreate (
            kCFAllocatorDefault,
            kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, // NV12
            width, height, NULL, &formatDescription);

        if (status != noErr)
        {
            NSLog (@"Failed to create format description: %d", status);
            return;
        }

        CVPixelBufferRef pixelBuffer = NULL;
        NSDictionary *pixelBufferAttributes = @{
            (NSString *)kCVPixelBufferPixelFormatTypeKey :
                @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
            (NSString *)kCVPixelBufferWidthKey : @(width),
            (NSString *)kCVPixelBufferHeightKey : @(height),
            (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{}
        };

        CVReturn result = CVPixelBufferCreate (
            kCFAllocatorDefault, width, height,
            kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            (__bridge CFDictionaryRef)pixelBufferAttributes, &pixelBuffer);

        if (result != kCVReturnSuccess)
        {
            NSLog (@"Failed to create pixel buffer: %d", result);
            return;
        }

        CVPixelBufferLockBaseAddress (pixelBuffer, 0);

        // COPY Y PLANE
        uint8_t *yDestPlane
            = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane (pixelBuffer, 0);
        size_t yDestBytesPerRow
            = CVPixelBufferGetBytesPerRowOfPlane (pixelBuffer, 0);
        uint8_t *ySrcPlane = frame->data[0];
        size_t ySrcBytesPerRow = frame->linesize[0];

        for (int row = 0; row < height; row++)
        {
            memcpy (yDestPlane + row * yDestBytesPerRow,
                    ySrcPlane + row * ySrcBytesPerRow, width);
        }

        // COPY UV PLANE
        uint8_t *uvDestPlane
            = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane (pixelBuffer, 1);
        size_t uvDestBytesPerRow
            = CVPixelBufferGetBytesPerRowOfPlane (pixelBuffer, 1);
        uint8_t *uvSrcPlane = frame->data[1];
        size_t uvSrcBytesPerRow = frame->linesize[1];

        for (int row = 0; row < height / 2; row++)
        {
            memcpy (uvDestPlane + row * uvDestBytesPerRow,
                    uvSrcPlane + row * uvSrcBytesPerRow, width);
        }

        CVPixelBufferUnlockBaseAddress (pixelBuffer, 0);

        CMSampleBufferRef sampleBuffer = NULL;
        CMSampleTimingInfo timingInfo
            = { .duration = kCMTimeInvalid,
                .presentationTimeStamp
                = CMTimeMakeWithSeconds (CACurrentMediaTime (), 1000000000),
                .decodeTimeStamp = kCMTimeInvalid };

        status = CMSampleBufferCreateReadyWithImageBuffer (
            kCFAllocatorDefault, pixelBuffer, formatDescription, &timingInfo,
            &sampleBuffer);

        CVPixelBufferRelease (pixelBuffer);
        CFRelease (formatDescription);

        if (status != noErr)
        {
            NSLog (@"Failed to create sample buffer: %d", status);
            return;
        }

        dispatch_async (dispatch_get_main_queue (), ^{
          if (displayLayer.status == AVQueuedSampleBufferRenderingStatusFailed)
          {
              NSLog (
                  @"[FramePipeWrapper] DisplayLayer status failed, flushing. "
                  @"Error: %@",
                  displayLayer.error);
              [displayLayer flush];
          }

          if (displayLayer.isReadyForMoreMediaData)
          {
              // NSLog(@"[FramePipeWrapper] Enqueuing sample buffer to
              // displayLayer");
              [displayLayer enqueueSampleBuffer:sampleBuffer];
          }
          else
          {
              NSLog (@"[FramePipeWrapper] DisplayLayer not ready for more "
                     @"media data");
          }

          CFRelease (sampleBuffer);
        });
    };

    return subscribe ({ std::string ([pipeId UTF8String]) }, callback);
}

+ (int)speakerSubscribeAudio:(AVAudioPlayerNode *)audioPlayer
                      pipeId:(NSString *)pipeId
                       queue:(dispatch_queue_t)queue
{
    auto resampler = std::make_shared<FFmpeg::Resampler> ();
    SpeakerPacer *pacer = [[SpeakerPacer alloc] initWithPlayer:audioPlayer
                                                         queue:queue];
    FrameCallback callback
        = [audioPlayer, resampler, pacer] (
              std::string pipeId, int subscriptionId, const FFmpeg::Frame &raw)
    {
        AVAudioFormat *format = [audioPlayer outputFormatForBus:0];

        AVSampleFormat avSampleFormat = AV_SAMPLE_FMT_NONE;
        if (format.isInterleaved)
        {
            avSampleFormat = AV_SAMPLE_FMT_FLT;
        }
        else
        {
            avSampleFormat = AV_SAMPLE_FMT_FLTP;
        }
        std::optional<FFmpeg::Frame> frame = resampler->resample (
            raw, avSampleFormat, format.sampleRate, format.channelCount);
        if (!frame.has_value ())
        {
            return;
        }

        AVAudioFrameCount frameCount
            = (AVAudioFrameCount)frame.value ()->nb_samples;
        AVAudioPCMBuffer *buffer =
            [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                          frameCapacity:frameCount];

        if (!buffer)
        {
            return;
        }

        for (int i = 0; i < frame.value ()->ch_layout.nb_channels; i++)
        {
            memcpy (buffer.audioBufferList->mBuffers[i].mData,
                    frame.value ()->data[i],
                    frame.value ()->nb_samples * sizeof (float));
        }
        buffer.frameLength = frameCount;

        [pacer enqueue:buffer];
    };

    return subscribe ({ std::string ([pipeId UTF8String]) }, callback);
}

+ (void)unsubscribe:(int)subscriptionId
{
    unsubscribe (subscriptionId);
}

@end
