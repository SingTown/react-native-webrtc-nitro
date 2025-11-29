import { getHostComponent, type HybridRef } from 'react-native-nitro-modules'
import WebrtcConfig from '../nitrogen/generated/shared/json/WebrtcViewConfig.json'
import type {
  WebrtcViewProps,
  WebrtcViewMethods,
} from './views/WebrtcView.nitro'

export const WebrtcView = getHostComponent<WebrtcViewProps, WebrtcViewMethods>(
  'WebrtcView',
  () => WebrtcConfig
)

export type WebrtcRef = HybridRef<WebrtcViewProps, WebrtcViewMethods>
