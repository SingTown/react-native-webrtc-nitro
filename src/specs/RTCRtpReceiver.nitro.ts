import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { MediaStreamTrack } from './MediaStreamTrack.nitro.js'

interface RTCRtpReceiver extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  readonly track: MediaStreamTrack | null
}

const RTCRtpReceiverExport =
  getHybridObjectConstructor<RTCRtpReceiver>('RTCRtpReceiver')
type RTCRtpReceiverExport = RTCRtpReceiver
export { RTCRtpReceiverExport as RTCRtpReceiver }
