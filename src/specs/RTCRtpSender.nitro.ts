import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { MediaStreamTrack } from './MediaStreamTrack.nitro.js'

interface RTCRtpSender extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  readonly track: MediaStreamTrack | null
}

const RTCRtpSenderExport =
  getHybridObjectConstructor<RTCRtpSender>('RTCRtpSender')
type RTCRtpSenderExport = RTCRtpSender
export { RTCRtpSenderExport as RTCRtpSender }
