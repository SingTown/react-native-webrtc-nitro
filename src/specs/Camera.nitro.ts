import { type HybridObject } from 'react-native-nitro-modules'

export interface Camera extends HybridObject<{
  ios: 'swift'
  android: 'kotlin'
}> {
  open(pipeId: string): Promise<void>
}
