import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'

export type MediaStreamTrackState = 'ended' | 'live'

export type FacingMode = 'user' | 'environment'

interface MediaStreamTrack extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly id: string
  readonly kind: string
  readonly readyState: MediaStreamTrackState
  enabled: boolean
  readonly _srcPipeId: string
  readonly _dstPipeId: string
  stop(): void
  switchCamera(facingMode: FacingMode): Promise<void>
}

const MediaStreamTrackExport =
  getHybridObjectConstructor<MediaStreamTrack>('MediaStreamTrack')
type MediaStreamTrackExport = MediaStreamTrack
export { MediaStreamTrackExport as MediaStreamTrack }
