import { type HybridObject } from 'react-native-nitro-modules'
import { NitroModules } from 'react-native-nitro-modules'
import { MediaStream } from './MediaStream.nitro'
import { type MicrophoneAndroidTuning } from './Microphone.nitro'

export interface AudioAndroidConstraints {
  inputTuning?: MicrophoneAndroidTuning
}

export interface AudioConstraints {
  android?: AudioAndroidConstraints
}

export interface MediaStreamConstraints {
  audio?: boolean | AudioConstraints
  video?: boolean
}

interface MediaDevices extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  getMockMedia(constraints: MediaStreamConstraints): Promise<MediaStream>
  getUserMedia(constraints: MediaStreamConstraints): Promise<MediaStream>
}

const MediaDevicesExport =
  NitroModules.createHybridObject<MediaDevices>('MediaDevices')
type MediaDevicesExport = MediaDevices
export { MediaDevicesExport as MediaDevices }
