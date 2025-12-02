#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface FramePipeWrapper : NSObject

+ (void)cameraPublishVideo:(CMSampleBufferRef)sampleBuffer
                   pipeIds:(NSArray<NSString *> *)pipeIds;

+ (void)microphonePublishAudio:(CMSampleBufferRef)sampleBuffer
                       pipeIds:(NSArray<NSString *> *)pipeIds;

+ (int)viewSubscribeVideo:(AVSampleBufferDisplayLayer *)displayLayer
                   pipeId:(NSString *)pipeId;

+ (int)speakerSubscribeAudio:(AVAudioPlayerNode *)audioPlayer
                      pipeId:(NSString *)pipeId
                       queue:(dispatch_queue_t)queue;

+ (void)unsubscribe:(int)subscriptionId;

@end

NS_ASSUME_NONNULL_END