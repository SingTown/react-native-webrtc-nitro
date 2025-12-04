import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { MediaStreamTrack } from './MediaStreamTrack.nitro'

interface MediaStream extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  readonly id: string
  getTracks(): MediaStreamTrack[]
  addTrack(track: MediaStreamTrack): void
  removeTrack(track: MediaStreamTrack): void
  getAudioTracks(): MediaStreamTrack[]
  getVideoTracks(): MediaStreamTrack[]
}

export const MediaStreamExport =
  getHybridObjectConstructor<MediaStream>('MediaStream')
export type MediaStreamExport = MediaStream
export { MediaStreamExport as MediaStream }
