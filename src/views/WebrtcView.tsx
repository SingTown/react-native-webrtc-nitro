import { getHostComponent } from 'react-native-nitro-modules'
import WebrtcViewConfig from '../../nitrogen/generated/shared/json/WebrtcViewConfig.json'
import { type WebrtcViewProps } from './WebrtcView.nitro'
import { type WebrtcViewMethods } from './WebrtcView.nitro'
import { MediaStream } from '../specs/MediaStream.nitro'
import React from 'react'
import type { StyleProp, ViewStyle } from 'react-native'

const WebrtcViewNitro = getHostComponent<WebrtcViewProps, WebrtcViewMethods>(
  'WebrtcViewNitro',
  () => WebrtcViewConfig
)

export interface WebrtcViewComponentProps {
  stream: MediaStream | null
  style?: StyleProp<ViewStyle>
  resizeMode?: 'contain' | 'cover' | 'fill'
}

export function WebrtcView({ stream, style, resizeMode }: WebrtcViewComponentProps) {
  const videoPipeId = stream?.getVideoTracks()?.[0]?._dstPipeId
  const audioPipeId = stream?.getAudioTracks()?.[0]?._dstPipeId
  return (
    <WebrtcViewNitro
      audioPipeId={audioPipeId}
      videoPipeId={videoPipeId}
      style={style}
      resizeMode={resizeMode}
    />
  )
}
