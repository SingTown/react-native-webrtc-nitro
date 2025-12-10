# react-native-webrtc-nitro

[![Version](https://img.shields.io/npm/v/react-native-webrtc-nitro.svg)](https://www.npmjs.com/package/react-native-webrtc-nitro)
[![Downloads](https://img.shields.io/npm/dm/react-native-webrtc-nitro.svg)](https://www.npmjs.com/package/react-native-webrtc-nitro)
[![License](https://img.shields.io/npm/l/react-native-webrtc-nitro.svg)](https://github.com/SingTown/react-native-webrtc-nitro/blob/main/LICENSE)

A high-performance WebRTC library built on [Nitro Modules](https://github.com/mrousavy/nitro), providing powerful real-time audio and video communication capabilities for React Native applications.

## ✨ Features

- 🚀 **High Performance**: Built on Nitro Modules for native-level performance
- 🎥 **Complete WebRTC API**: Support for audio/video calls, data channels, and more
- 📹 **Media Recording**: Built-in MediaRecorder support for recording audio/video streams
- 🎬 **FFmpeg Integration**: Powerful audio/video encoding, decoding, transcoding, and processing capabilities
- 🎞️ **H.264/H.265 Support**: Hardware-accelerated H.264 and H.265 (HEVC) encoding and decoding
- 🎵 **Opus Audio Codec**: High-quality Opus audio encoding and decoding for efficient audio streaming
- 📱 **Cross-Platform**: Perfect support for iOS and Android
- 🔧 **TypeScript**: Full type definitions
- 🎨 **Nitro Views**: High-performance video rendering views

## 📋 Requirements

- React Native >= 0.76.0
- Node.js >= 18.0.0
- iOS >= 13.0
- Android API >= 24

> [!IMPORTANT]  
> To use `Nitro Views` features, React Native >= 0.78.0 is required

## 📦 Installation

Using pnpm:

```bash
pnpm add react-native-webrtc-nitro react-native-nitro-modules
```

Using npm:

```bash
npm install react-native-webrtc-nitro react-native-nitro-modules
```

Using yarn:

```bash
yarn add react-native-webrtc-nitro react-native-nitro-modules
```

### iOS Configuration

```bash
cd ios && pod install
```

Add permission descriptions to `Info.plist`:

```xml
<key>NSCameraUsageDescription</key>
<string>Camera access is required for video calls</string>
<key>NSMicrophoneUsageDescription</key>
<string>Microphone access is required for audio calls</string>
```

### Android Configuration

Add permissions to `AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.CAMERA" />
<uses-permission android:name="android.permission.RECORD_AUDIO" />
<uses-permission android:name="android.permission.INTERNET" />
```

## 🚀 Quick Start

### Create Offer (Caller Side)

```typescript
import {
  MediaDevices,
  RTCPeerConnection,
  Permissions
} from 'react-native-webrtc-nitro';

// 1. Check and request permissions
let cameraPermission = await Permissions.query({ name: 'camera' });
if (cameraPermission !== 'granted') {
  cameraPermission = await Permissions.request({ name: 'camera' });
}

let microphonePermission = await Permissions.query({ name: 'microphone' });
if (microphonePermission !== 'granted') {
  microphonePermission = await Permissions.request({ name: 'microphone' });
}

if (cameraPermission !== 'granted' || microphonePermission !== 'granted') {
  console.error('Camera and microphone permissions are required');
  return;
}

// 2. Get local media stream
const localStream = await MediaDevices.getUserMedia({
  audio: true,
  video: true
});

// 3. Create RTCPeerConnection
const peerConnection = new RTCPeerConnection({
  iceServers: [
    { urls: 'stun:stun.l.google.com:19302' }
  ]
});

// 4. Add local stream to connection
localStream.getTracks().forEach(track => {
  peerConnection.addTransceiver(track, {
    direction: 'sendrecv',
    streams: [localStream]
  });
});

// 5. Handle ICE candidates
peerConnection.onicecandidate = (event) => {
  if (event.candidate) {
    // Send ICE candidate to remote peer via signaling server
    signalingServer.send({ 
      type: 'candidate', 
      candidate: event.candidate 
    });
  }
};

// 6. Handle remote stream
peerConnection.ontrack = (event) => {
  const remoteStream = event.streams[0];
  setRemoteStream(remoteStream);
};

// 7. Create and send offer
const offer = await peerConnection.createOffer();
await peerConnection.setLocalDescription(offer);

// Send offer to remote peer via signaling server
signalingServer.send({ 
  type: 'offer', 
  sdp: offer.sdp 
});

// 8. Receive answer from remote peer
signalingServer.on('answer', async (answer) => {
  await peerConnection.setRemoteDescription({
    type: 'answer',
    sdp: answer.sdp
  });
});

// 9. Receive ICE candidates from remote peer
signalingServer.on('candidate', async (candidate) => {
  await peerConnection.addIceCandidate({
    candidate: candidate.candidate,
    sdpMid: candidate.sdpMid
  });
});
```

### Create Answer (Callee Side)

```typescript
import {
  MediaDevices,
  RTCPeerConnection,
  Permissions
} from 'react-native-webrtc-nitro';

// 1. Check and request permissions
let cameraPermission = await Permissions.query({ name: 'camera' });
if (cameraPermission !== 'granted') {
  cameraPermission = await Permissions.request({ name: 'camera' });
}

let microphonePermission = await Permissions.query({ name: 'microphone' });
if (microphonePermission !== 'granted') {
  microphonePermission = await Permissions.request({ name: 'microphone' });
}

if (cameraPermission !== 'granted' || microphonePermission !== 'granted') {
  console.error('Camera and microphone permissions are required');
  return;
}

// 2. Get local media stream
const localStream = await MediaDevices.getUserMedia({
  audio: true,
  video: true
});

// 3. Create RTCPeerConnection
const peerConnection = new RTCPeerConnection({
  iceServers: [
    { urls: 'stun:stun.l.google.com:19302' }
  ]
});

// 4. Add local stream to connection
localStream.getTracks().forEach(track => {
  peerConnection.addTransceiver(track, {
    direction: 'sendrecv',
    streams: [localStream]
  });
});

// 5. Handle ICE candidates
peerConnection.onicecandidate = (event) => {
  if (event.candidate) {
    // Send ICE candidate to remote peer via signaling server
    signalingServer.send({ 
      type: 'candidate', 
      candidate: event.candidate 
    });
  }
};

// 6. Handle remote stream
peerConnection.ontrack = (event) => {
  const remoteStream = event.streams[0];
  setRemoteStream(remoteStream);
};

// 7. Receive offer from remote peer
signalingServer.on('offer', async (offer) => {
  // Set remote description
  await peerConnection.setRemoteDescription({
    type: 'offer',
    sdp: offer.sdp
  });

  // Create and send answer
  const answer = await peerConnection.createAnswer();
  await peerConnection.setLocalDescription(answer);

  // Send answer back to caller via signaling server
  signalingServer.send({ 
    type: 'answer', 
    sdp: answer.sdp 
  });
});

// 8. Receive ICE candidates from remote peer
signalingServer.on('candidate', async (candidate) => {
  await peerConnection.addIceCandidate({
    candidate: candidate.candidate,
    sdpMid: candidate.sdpMid
  });
});
```

### Video Rendering

```typescript
import { WebrtcView } from 'react-native-webrtc-nitro';

function VideoCall() {
  const [localStream, setLocalStream] = useState<MediaStream | null>(null);
  const [remoteStream, setRemoteStream] = useState<MediaStream | null>(null);

  return (
    <View style={styles.container}>
      {/* Local video */}
      <WebrtcView
        style={styles.localVideo}
        stream={localStream}
      />
      
      {/* Remote video */}
      <WebrtcView
        style={styles.remoteVideo}
        stream={remoteStream}
      />
    </View>
  );
}
```

### Media Recording

```typescript
import { MediaRecorder } from 'react-native-webrtc-nitro';
import RNFS from 'react-native-fs';

// Create recorder with media stream
const recorder = new MediaRecorder(mediaStream);

// Take a photo from the stream
const photoPath = `${RNFS.DocumentDirectoryPath}/photo.jpg`;
await recorder.takePhoto(photoPath);
console.log('Photo saved to:', photoPath);

// Start video recording
const videoPath = `${RNFS.DocumentDirectoryPath}/video.mp4`;
recorder.startRecording(videoPath);

// Stop recording after some time
setTimeout(() => {
  recorder.stopRecording();
  console.log('Video saved to:', videoPath);
}, 10000);
```

## 📚 API Documentation

### Permissions

Permission management module

```typescript
// Query permission status
const cameraStatus = await Permissions.query({ name: 'camera' });
const microphoneStatus = await Permissions.query({ name: 'microphone' });
// Returns: 'granted' | 'denied' | 'prompt'

// Request permissions
const cameraPermission = await Permissions.request({ name: 'camera' });
const microphonePermission = await Permissions.request({ name: 'microphone' });
// Returns: 'granted' | 'denied' | 'prompt'
```

### MediaDevices

Media device access module

```typescript
// Get user media
const stream = await MediaDevices.getUserMedia({
  audio: true,
  video: true
});

// Get audio only
const audioStream = await MediaDevices.getUserMedia({
  audio: true,
  video: false
});

// Get video only
const videoStream = await MediaDevices.getUserMedia({
  audio: false,
  video: true
});

// Get mock media for testing
const mockStream = await MediaDevices.getMockMedia({
  audio: true,
  video: true
});
```

### MediaStream

Media stream management

```typescript
// Get stream ID
const streamId = stream.id;

// Get all tracks
const allTracks = stream.getTracks();

// Get audio tracks
const audioTracks = stream.getAudioTracks();

// Get video tracks
const videoTracks = stream.getVideoTracks();

// Add track to stream
stream.addTrack(track);

// Remove track from stream
stream.removeTrack(track);
```

### MediaStreamTrack

Media track operations

```typescript
// Get track ID
const trackId = track.id;

// Get track kind ('audio' or 'video')
const trackKind = track.kind;

// Get track state ('live' or 'ended')
const trackState = track.readyState;

// Enable/disable track
track.enabled = true;
track.enabled = false;

// Stop track
track.stop();
```

### RTCPeerConnection

Peer-to-peer connection

```typescript
// Create connection
const pc = new RTCPeerConnection(config);

// Add transceiver with track
const transceiver = pc.addTransceiver(track, {
  direction: 'sendrecv', // 'sendrecv' | 'sendonly' | 'recvonly' | 'inactive'
  streams: [stream]
});

// Add transceiver with media kind
const videoTransceiver = pc.addTransceiver('video', {
  direction: 'recvonly'
});

// Get all transceivers
const transceivers = pc.getTransceivers();

// Create offer
const offer = await pc.createOffer();
await pc.setLocalDescription(offer);

// Create answer
const answer = await pc.createAnswer();
await pc.setLocalDescription(answer);

// Set remote description
await pc.setRemoteDescription(answer);

// Add ICE candidate
await pc.addIceCandidate(candidate);

// Get connection state
const connectionState = pc.connectionState; // 'new' | 'connecting' | 'connected' | 'disconnected' | 'failed' | 'closed'
const iceGatheringState = pc.iceGatheringState; // 'new' | 'gathering' | 'complete'

// Get local/remote description
const localDesc = pc.localDescription;
const remoteDesc = pc.remoteDescription;

// Event listeners
pc.onicecandidate = (event) => { };
pc.ontrack = (event) => { };
pc.onconnectionstatechange = (event) => { };
pc.onicegatheringstatechange = (event) => { };

// Close connection
pc.close();
```

### MediaRecorder

Media recording

```typescript
import { MediaRecorder } from 'react-native-webrtc-nitro';
import RNFS from 'react-native-fs';

// Create recorder
const recorder = new MediaRecorder(stream);

// Take photo
const photoPath = `${RNFS.DocumentDirectoryPath}/photo.jpg`;
await recorder.takePhoto(photoPath);

// Start recording video
const videoPath = `${RNFS.DocumentDirectoryPath}/video.mp4`;
recorder.startRecording(videoPath);

// Stop recording
recorder.stopRecording();
```

## 🏗️ Architecture

```
react-native-webrtc-nitro
├── src/                    # TypeScript API
│   ├── specs/             # Nitro spec definitions
│   └── views/             # View components
├── cpp/                    # C++ core implementation
│   ├── FFmpeg/            # FFmpeg wrapper
│   ├── Hybrid/            # Hybrid object implementations
│   └── FramePipe/         # Frame processing pipeline
├── ios/                    # iOS platform code
├── android/                # Android platform code
└── 3rdparty/              # Third-party libraries
    ├── ffmpeg/
    ├── libdatachannel/
    └── opus/
```

## 🔧 Development

### Setup Development Environment

```bash
# Clone repository
git clone https://github.com/SingTown/react-native-webrtc-nitro.git
cd react-native-webrtc-nitro

# Build native libraries first
cd 3rdparty

# Download dependencies
./download.sh

# Build for iOS
./build_ios.sh

# Build for Android
./build_android.sh

cd ..

# Install dependencies
pnpm install
```

### Run Example

```bash
cd example

# iOS
pnpm ios

# Android
pnpm android
```

## 📝 Example Project

Check the `example` directory for a complete sample application, including:

- Audio/video calls
- Media recording
- Multi-party conferencing

## 🤝 Contributing

Pull requests are welcome! For major changes, please open an issue first to discuss what you would like to change.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📄 License

MIT License - see the [LICENSE](LICENSE) file for details

## 🙏 Acknowledgments

- [Nitro Modules](https://github.com/mrousavy/nitro) - Powerful React Native native module framework
- [libdatachannel](https://github.com/paullouisageneau/libdatachannel) - WebRTC data channel implementation
- [FFmpeg](https://ffmpeg.org/) - Audio/video processing library

## 📮 Contact

- GitHub: [@SingTown](https://github.com/SingTown)
- Issues: [Submit an issue](https://github.com/SingTown/react-native-webrtc-nitro/issues)

## 🌟 Star History

If this project helps you, please give us a ⭐️!

---

Built with ❤️ using [Nitro Modules](https://github.com/mrousavy/nitro)
