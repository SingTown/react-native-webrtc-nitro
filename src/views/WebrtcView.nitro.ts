import type {
  HybridView,
  HybridViewProps,
  HybridViewMethods,
} from 'react-native-nitro-modules'

export interface WebrtcViewProps extends HybridViewProps {
  videoPipeId?: string
  audioPipeId?: string
}

export interface WebrtcViewMethods extends HybridViewMethods {}
export type WebrtcView = HybridView<
  WebrtcViewProps,
  WebrtcViewMethods,
  { ios: 'swift'; android: 'kotlin' }
>
