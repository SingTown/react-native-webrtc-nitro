import { type HybridObject } from 'react-native-nitro-modules'
import type { FacingMode } from './MediaStreamTrack.nitro'

export interface Camera extends HybridObject<{
  ios: 'swift'
  android: 'kotlin'
}> {
  open(pipeId: string): Promise<void>
  switchCamera(facingMode: FacingMode): Promise<void>
}
