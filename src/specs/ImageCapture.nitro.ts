import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { MediaStreamTrack } from './MediaStreamTrack.nitro'

interface ImageCapture extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  track: MediaStreamTrack
  takePhoto(file: string): Promise<void>
}

const ImageCaptureConstructor =
  getHybridObjectConstructor<ImageCapture>('ImageCapture')

const ImageCaptureExport = new Proxy(ImageCaptureConstructor, {
  construct(target, args) {
    const instance = new target()
    instance.track = args[0] as MediaStreamTrack
    return instance
  },
}) as { new (track: MediaStreamTrack): ImageCapture }

type ImageCaptureExport = ImageCapture
export { ImageCaptureExport as ImageCapture }
