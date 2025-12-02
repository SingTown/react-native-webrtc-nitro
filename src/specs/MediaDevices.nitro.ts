import { type HybridObject } from 'react-native-nitro-modules'
import { NitroModules } from 'react-native-nitro-modules'
import { MediaStream } from './MediaStream.nitro'

export interface MediaStreamConstraints {
  audio?: boolean
  video?: boolean
}

interface MediaDevices extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  getMockMedia(constraints: MediaStreamConstraints): Promise<MediaStream>
}

const MediaDevicesExport =
  NitroModules.createHybridObject<MediaDevices>('MediaDevices')
type MediaDevicesExport = MediaDevices
export { MediaDevicesExport as MediaDevices }
