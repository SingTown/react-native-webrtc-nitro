import React, { useEffect } from 'react';
import { View, StyleSheet } from 'react-native';
import { WebrtcView } from 'react-native-webrtc-nitro';
import { MediaStream } from 'react-native-webrtc-nitro';
import { MediaDevices } from 'react-native-webrtc-nitro';

function App(): React.JSX.Element {
  const [stream, setStream] = React.useState<MediaStream | undefined>(
    undefined,
  );

  useEffect(() => {
    const fetchStream = async () => {
      const localstream = await MediaDevices.getMockMedia({
        video: true,
        audio: true,
      });

      setStream(localstream);

      // 获取所有轨道
      const tracks = localstream.getTracks();
      console.log('Total tracks:', tracks.length);

      // 遍历轨道
      tracks.forEach((track, index) => {
        console.log(`Track ${index}:`, {
          kind: track.kind,
          dstPipId: track._dstPipId,
        });
      });
    };
    fetchStream();
  }, []);

  return (
    <View style={styles.container}>
      <WebrtcView stream={stream} style={styles.view} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  view: {
    width: 200,
    height: 200,
  },
});

export default App;
