import type {
  HybridView,
  HybridViewProps,
  HybridViewMethods,
} from 'react-native-nitro-modules'

export interface VideoDimensionsEvent {
  width: number
  height: number
}

export interface WebrtcViewProps extends HybridViewProps {
  videoPipeId?: string
  audioPipeId?: string
  onDimensionsChange?: (event: VideoDimensionsEvent) => void
}

export interface WebrtcViewMethods extends HybridViewMethods {}
export type WebrtcView = HybridView<
  WebrtcViewProps,
  WebrtcViewMethods,
  { ios: 'swift'; android: 'kotlin' }
>
