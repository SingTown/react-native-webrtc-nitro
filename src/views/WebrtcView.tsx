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
  stream?: MediaStream
  style?: StyleProp<ViewStyle>
}

export function WebrtcView({ stream, style }: WebrtcViewComponentProps) {
  const videoPipeId = stream?.getVideoTracks()?.[0]?._dstPipId
  const audioPipeId = stream?.getAudioTracks()?.[0]?._dstPipId
  return (
    <WebrtcViewNitro
      audioPipeId={audioPipeId}
      videoPipeId={videoPipeId}
      style={style}
    />
  )
}
