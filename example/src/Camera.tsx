import { useState, useEffect } from 'react';
import { Button, StyleSheet, View, Alert } from 'react-native';
import { PermissionsAndroid, Platform } from 'react-native';
import RNFS from 'react-native-fs';
import { CameraRoll } from '@react-native-camera-roll/camera-roll';

import {
  Permissions,
  WebrtcView,
  MediaStream,
  MediaDevices,
  MediaRecorder,
  ImageCapture,
} from 'react-native-webrtc-nitro';

type RecordingSession = {
  recorder: MediaRecorder;
  file: string;
};

function getTempPath(prefix: string, extension: string) {
  const temporaryDirectory = RNFS.TemporaryDirectoryPath.replace(/\/$/, '');
  return `${temporaryDirectory}/${prefix}_${Date.now()}.${extension}`;
}

function toFileUri(filePath: string) {
  return filePath.startsWith('file://') ? filePath : `file://${filePath}`;
}

function getErrorMessage(error: unknown) {
  if (error instanceof Error) {
    return error.message;
  }
  return String(error);
}

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
  const [recording, setRecording] = useState<RecordingSession | null>(null);

  useEffect(() => {
    let localStream: MediaStream | null = null;
    (async () => {
      let microphonePermission = await Permissions.query({
        name: 'microphone',
      });
      if (microphonePermission === 'prompt') {
        microphonePermission = await Permissions.request({
          name: 'microphone',
        });
      }

      let cameraPermission = await Permissions.query({ name: 'camera' });
      if (cameraPermission === 'prompt') {
        cameraPermission = await Permissions.request({ name: 'camera' });
      }
      if (
        microphonePermission !== 'granted' ||
        cameraPermission !== 'granted'
      ) {
        Alert.alert('Permissions not granted');
        return;
      }

      try {
        localStream = await MediaDevices.getUserMedia({
          // localStream = await MediaDevices.getMockMedia({
          audio: true,
          video: true,
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

  const takePhoto = async () => {
    if (!stream) {
      return;
    }

    try {
      await requestPermission();
      const pngPath = getTempPath('test_photo', 'png');
      const [videoTrack] = stream.getVideoTracks();
      if (!videoTrack) {
        throw new Error('No video track available');
      }
      const imageCapture = new ImageCapture(videoTrack);
      await imageCapture.takePhoto(pngPath);
      await CameraRoll.save(toFileUri(pngPath), { type: 'photo' });
      console.log('Saved photo to camera roll');
    } catch (error) {
      console.error('Take photo failed', error);
      Alert.alert('Take Photo Failed', getErrorMessage(error));
    }
  };

  const startRecording = async () => {
    if (!stream || recording) {
      return;
    }

    try {
      await requestPermission();
      const mp4Path = getTempPath('test_recording', 'mp4');
      const recorder = new MediaRecorder(stream);
      recorder.startRecording(mp4Path);
      setRecording({ recorder, file: mp4Path });
      console.log('Started recording', mp4Path);
    } catch (error) {
      console.error('Start recording failed', error);
      Alert.alert('Start Recording Failed', getErrorMessage(error));
    }
  };

  const stopRecording = async () => {
    if (!recording) {
      return;
    }

    const currentRecording = recording;
    setRecording(null);

    try {
      currentRecording.recorder.stopRecording();
      await CameraRoll.save(toFileUri(currentRecording.file), {
        type: 'video',
      });
      console.log('Saved recording to camera roll');
    } catch (error) {
      console.error('Stop recording failed', error);
      Alert.alert('Stop Recording Failed', getErrorMessage(error));
    }
  };

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
        <Button title="Take Photo" disabled={!stream} onPress={takePhoto} />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Start Recording"
          disabled={!stream || recording !== null}
          onPress={startRecording}
        />
      </View>
      <View style={styles.buttonContainer}>
        <Button
          title="Stop Recording"
          disabled={!recording}
          onPress={stopRecording}
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
