import { type HybridObject } from 'react-native-nitro-modules'
import { getHybridObjectConstructor } from 'react-native-nitro-modules'
import { MediaStreamTrack } from './MediaStreamTrack.nitro'
import { RTCRtpTransceiver } from './RTCRtpTransceiver.nitro'
import { type RTCRtpTransceiverDirection } from './RTCRtpTransceiver.nitro'
import { MediaStream } from './MediaStream.nitro'

type RTCIceGatheringState = 'complete' | 'gathering' | 'new'
type RTCPeerConnectionState =
  | 'closed'
  | 'connected'
  | 'connecting'
  | 'disconnected'
  | 'failed'
  | 'new'
type RTCSdpType = 'answer' | 'offer' | 'pranswer' | 'rollback'

export interface Event {
  readonly NONE: 0
}

export interface RTCIceServer {
  credential?: string
  urls: string[] | string
  username?: string
}

export interface RTCConfiguration {
  iceServers?: RTCIceServer[]
}

export interface RTCRtpTransceiverInit {
  direction?: RTCRtpTransceiverDirection
  streams?: MediaStream[]
}

export interface RTCIceCandidate {
  readonly candidate: string
}

export interface RTCIceCandidateInit {
  candidate?: string
  sdpMid?: string | null
}

export interface RTCPeerConnectionIceEvent {
  readonly candidate: RTCIceCandidate | null
}

export interface RTCSessionDescriptionInit {
  sdp?: string
  type: RTCSdpType
}

export interface RTCTrackEvent {
  track: MediaStreamTrack
  streams: MediaStream[]
}

type stateChangeHandler = (event: Event) => void
type iceCandidateHandler = (event: RTCPeerConnectionIceEvent) => void
type trackHandler = (event: RTCTrackEvent) => void

interface RTCPeerConnection extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly connectionState: RTCPeerConnectionState
  readonly iceGatheringState: RTCIceGatheringState
  readonly localDescription: string
  readonly remoteDescription: string

  onconnectionstatechange?: stateChangeHandler
  onicegatheringstatechange?: stateChangeHandler
  onicecandidate?: iceCandidateHandler
  ontrack?: trackHandler

  setConfiguration(config?: RTCConfiguration): void
  close(): void
  setLocalDescription(description?: RTCSessionDescriptionInit): Promise<void>
  setRemoteDescription(description: RTCSessionDescriptionInit): Promise<void>
  addIceCandidate(candidate?: RTCIceCandidateInit | null): Promise<void>

  addTransceiver(
    trackOrKind: MediaStreamTrack | string,
    init?: RTCRtpTransceiverInit
  ): RTCRtpTransceiver
  getTransceivers(): RTCRtpTransceiver[]

  createOffer(): Promise<RTCSessionDescriptionInit>
  createAnswer(): Promise<RTCSessionDescriptionInit>
}

const RTCPeerConnectionConstructor =
  getHybridObjectConstructor<RTCPeerConnection>('RTCPeerConnection')
const RTCPeerConnectionExport = new Proxy(RTCPeerConnectionConstructor, {
  construct(target, args) {
    const config = args[0] as RTCConfiguration | undefined
    const instance = new target()
    instance.setConfiguration(config)
    return instance
  },
}) as { new (config?: RTCConfiguration): RTCPeerConnection }
type RTCPeerConnectionExport = RTCPeerConnection
export { RTCPeerConnectionExport as RTCPeerConnection }
