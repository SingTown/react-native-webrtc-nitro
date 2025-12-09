import { type HybridObject } from 'react-native-nitro-modules'

export interface Microphone extends HybridObject<{
  ios: 'swift'
  android: 'kotlin'
}> {
  open(pipeId: string): Promise<void>
}
