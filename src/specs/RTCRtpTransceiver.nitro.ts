import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { RTCRtpSender } from './RTCRtpSender.nitro'
import { RTCRtpReceiver } from './RTCRtpReceiver.nitro'

export type RTCRtpTransceiverDirection =
  | 'inactive'
  | 'recvonly'
  | 'sendonly'
  | 'sendrecv'

interface RTCRtpTransceiver extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly mid: string | null
  direction: RTCRtpTransceiverDirection
  readonly sender: RTCRtpSender
  readonly receiver: RTCRtpReceiver
}

const RTCRtpTransceiverExport =
  getHybridObjectConstructor<RTCRtpTransceiver>('RTCRtpTransceiver')
type RTCRtpTransceiverExport = RTCRtpTransceiver
export { RTCRtpTransceiverExport as RTCRtpTransceiver }
