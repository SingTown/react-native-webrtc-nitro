import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'

export type MediaStreamTrackState = 'ended' | 'live'

interface MediaStreamTrack extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly id: string
  readonly kind: string
  readonly readyState: MediaStreamTrackState
  enable: boolean
  readonly _dstPipId: string
  stop(): void
}

const MediaStreamTrackExport =
  getHybridObjectConstructor<MediaStreamTrack>('MediaStreamTrack')
type MediaStreamTrackExport = MediaStreamTrack
export { MediaStreamTrackExport as MediaStreamTrack }
