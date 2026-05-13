import { type HybridObject } from 'react-native-nitro-modules'

export interface MicrophoneAndroidTuning {
  agcTargetRms?: number
  nearMaxGain?: number
  farMaxGain?: number
  noiseGateOpenRatio?: number
  farThresholdRatio?: number
}

export interface Microphone extends HybridObject<{
  ios: 'swift'
  android: 'kotlin'
}> {
  open(pipeId: string, tuning?: MicrophoneAndroidTuning): Promise<void>
}
