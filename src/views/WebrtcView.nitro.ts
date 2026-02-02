import type {
  HybridView,
  HybridViewProps,
  HybridViewMethods,
} from 'react-native-nitro-modules'

export type ResizeMode = 'contain' | 'cover' | 'fill'

export interface WebrtcViewProps extends HybridViewProps {
  videoPipeId?: string
  audioPipeId?: string
  resizeMode?: ResizeMode
}

export interface WebrtcViewMethods extends HybridViewMethods {}
export type WebrtcView = HybridView<
  WebrtcViewProps,
  WebrtcViewMethods,
  { ios: 'swift'; android: 'kotlin' }
>
