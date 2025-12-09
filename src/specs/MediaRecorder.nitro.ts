import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { MediaStream } from './MediaStream.nitro'

interface MediaRecorder extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  stream: MediaStream
  takePhoto(file: string): Promise<void>
  startRecording(file: string): void
  stopRecording(): void
}

const MediaRecorderConstructor =
  getHybridObjectConstructor<MediaRecorder>('MediaRecorder')

const MediaRecorderExport = new Proxy(MediaRecorderConstructor, {
  construct(target, args) {
    const instance = new target()
    instance.stream = args[0] as MediaStream
    return instance
  },
}) as { new (stream: MediaStream): MediaRecorder }

type MediaRecorderExport = MediaRecorder
export { MediaRecorderExport as MediaRecorder }
