import { useState, useEffect } from 'react';
import { Button, StyleSheet, View, Alert } from 'react-native';

import {
  WebrtcView,
  MediaStream,
  MediaDevices,
} from 'react-native-webrtc-nitro';

export default function Camera() {
  const [stream, setStream] = useState<MediaStream | null>(null);

  useEffect(() => {
    let localStream: MediaStream | null = null;
    (async () => {
      try {
        localStream = await MediaDevices.getUserMedia({
          // localStream = await MediaDevices.getMockMedia({
          video: true,
          audio: true,
        });
        setStream(localStream);
      } catch (e) {
        Alert.alert('Permission Error');
        throw e;
      }
    })();

    return () => {
      localStream?.getTracks().forEach(track => {
        track.stop();
      });
      setStream(null);
    };
  }, []);

  return (
    <View style={styles.container}>
      <WebrtcView style={styles.player} stream={stream} />
      <View style={styles.buttonContainer}>
        <Button
          title="Enable Video"
          onPress={() => {
            stream?.getVideoTracks().forEach(track => {
              track.enabled = true;
            });
          }}
        />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Disable Video"
          onPress={() => {
            stream?.getVideoTracks().forEach(track => {
              track.enabled = false;
            });
          }}
        />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Enable Audio"
          onPress={() => {
            stream?.getAudioTracks().forEach(track => {
              track.enabled = true;
            });
          }}
        />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Disable Audio"
          onPress={() => {
            stream?.getAudioTracks().forEach(track => {
              track.enabled = false;
            });
          }}
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    padding: 10,
    flexGrow: 1,
    gap: 10,
  },
  player: {
    height: 240,
  },
  buttonContainer: {
    height: 44,
    margin: 5,
    justifyContent: 'center',
    backgroundColor: '#a8a4a4a3',
  },
});
