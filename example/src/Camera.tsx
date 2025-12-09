import { useState, useEffect } from 'react';
import { Button, StyleSheet, View, Alert } from 'react-native';
import { PermissionsAndroid, Platform } from 'react-native';
import RNFS from 'react-native-fs';
import { CameraRoll } from '@react-native-camera-roll/camera-roll';
import path from 'path-browserify';

import {
  WebrtcView,
  MediaStream,
  MediaDevices,
  MediaRecorder,
} from 'react-native-webrtc-nitro';

async function hasAndroidPermission() {
  const apiLevel = Number(Platform.Version);
  const getCheckPermissionPromise = () => {
    if (apiLevel >= 33) {
      return Promise.all([
        PermissionsAndroid.check(
          PermissionsAndroid.PERMISSIONS.READ_MEDIA_IMAGES,
        ),
        PermissionsAndroid.check(
          PermissionsAndroid.PERMISSIONS.READ_MEDIA_VIDEO,
        ),
      ]).then(
        ([hasReadMediaImagesPermission, hasReadMediaVideoPermission]) =>
          hasReadMediaImagesPermission && hasReadMediaVideoPermission,
      );
    } else {
      return PermissionsAndroid.check(
        PermissionsAndroid.PERMISSIONS.READ_EXTERNAL_STORAGE,
      );
    }
  };

  const hasPermission = await getCheckPermissionPromise();
  if (hasPermission) {
    return true;
  }
  const getRequestPermissionPromise = () => {
    if (apiLevel >= 33) {
      return PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.READ_MEDIA_IMAGES,
        PermissionsAndroid.PERMISSIONS.READ_MEDIA_VIDEO,
      ]).then(
        statuses =>
          statuses[PermissionsAndroid.PERMISSIONS.READ_MEDIA_IMAGES] ===
            PermissionsAndroid.RESULTS.GRANTED &&
          statuses[PermissionsAndroid.PERMISSIONS.READ_MEDIA_VIDEO] ===
            PermissionsAndroid.RESULTS.GRANTED,
      );
    } else {
      return PermissionsAndroid.request(
        PermissionsAndroid.PERMISSIONS.READ_EXTERNAL_STORAGE,
      ).then(status => status === PermissionsAndroid.RESULTS.GRANTED);
    }
  };

  return await getRequestPermissionPromise();
}

async function requestPermission() {
  if (Platform.OS === 'android' && !(await hasAndroidPermission())) {
    throw new Error('Storage permission denied');
  }
}

export default function Camera() {
  const [stream, setStream] = useState<MediaStream | null>(null);
  const mp4path = path.join(RNFS.TemporaryDirectoryPath, 'test_recording.mp4');
  console.log('mp4path', mp4path);
  const pngPath = path.join(RNFS.TemporaryDirectoryPath, 'test_photo.png');
  const [recording, setRecording] = useState<MediaRecorder | null>(null);

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
      <View style={styles.buttonContainer}>
        <Button
          title="Take Photo"
          onPress={async () => {
            await requestPermission();
            let new_recording = new MediaRecorder(stream!);
            console.log('Taking photo...', pngPath);
            await new_recording.takePhoto(pngPath);
            console.log('new_recording.takePhoto done');
            await CameraRoll.save(pngPath, { type: 'photo' });
            console.log('CameraRoll.save');
          }}
        />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Start Recording"
          onPress={async () => {
            if (recording) {
              return;
            }
            await requestPermission();
            let new_recording = new MediaRecorder(stream!);
            new_recording.startRecording(mp4path);
            setRecording(new_recording);
          }}
        />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Stop Recording"
          onPress={async () => {
            if (!recording) {
              return;
            }
            recording.stopRecording();
            await CameraRoll.save(mp4path, { type: 'video' });
            console.log('Saved recording to camera roll');
            setRecording(null);
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
