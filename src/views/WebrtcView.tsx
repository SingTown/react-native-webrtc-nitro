import { callback, getHostComponent } from 'react-native-nitro-modules'
import WebrtcViewConfig from '../../nitrogen/generated/shared/json/WebrtcViewConfig.json'
import { type VideoDimensionsEvent } from './WebrtcView.nitro'
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
  onDimensionsChange?: (event: VideoDimensionsEvent) => void
}

export function WebrtcView({
  stream,
  style,
  onDimensionsChange,
}: WebrtcViewComponentProps) {
  const videoPipeId = stream?.getVideoTracks()?.[0]?._dstPipeId
  const audioPipeId = stream?.getAudioTracks()?.[0]?._dstPipeId
  return (
    <WebrtcViewNitro
      audioPipeId={audioPipeId}
      onDimensionsChange={callback(onDimensionsChange)}
      videoPipeId={videoPipeId}
      style={style}
    />
  )
}
