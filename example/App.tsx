import React from 'react';
import { View, StyleSheet } from 'react-native';
import { WebrtcView } from 'react-native-webrtc-nitro';

function App(): React.JSX.Element {
  return (
    <View style={styles.container}>
      <WebrtcView isRed={true} style={styles.view} testID="webrtc" />
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
