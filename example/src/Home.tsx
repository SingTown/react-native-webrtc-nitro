import { useState, useEffect } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { Button, StyleSheet, TextInput, View, ScrollView } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';

import type { NativeStackNavigationProp } from '@react-navigation/native-stack';

type RootStackParamList = {
  Home: undefined;
  Offer: undefined;
  Answer: undefined;
  Camera: undefined;
  SelfTest: undefined;
};

type HomeScreenNavigationProp = NativeStackNavigationProp<
  RootStackParamList,
  'Home'
>;

export default function Home({
  navigation,
}: {
  navigation: HomeScreenNavigationProp;
}) {
  const [iceUrl, setIceUrl] = useState('turn:turn.example.com:3478');
  const [iceUsername, setIceUsername] = useState('username');
  const [icePassword, setIcePassword] = useState('password');

  useEffect(() => {
    (async () => {
      const url = await AsyncStorage.getItem('iceUrl');
      const user = await AsyncStorage.getItem('iceUsername');
      const pwd = await AsyncStorage.getItem('icePassword');
      if (url) setIceUrl(url);
      if (user) setIceUsername(user);
      if (pwd) setIcePassword(pwd);
    })();
  }, []);

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <ScrollView>
          <View>
            <TextInput
              style={styles.textInput}
              value={iceUrl}
              onChangeText={text => {
                setIceUrl(text);
                AsyncStorage.setItem('iceUrl', text);
              }}
            />
            <TextInput
              style={styles.textInput}
              value={iceUsername}
              onChangeText={text => {
                setIceUsername(text);
                AsyncStorage.setItem('iceUsername', text);
              }}
            />
            <TextInput
              style={styles.textInput}
              value={icePassword}
              onChangeText={text => {
                setIcePassword(text);
                AsyncStorage.setItem('icePassword', text);
              }}
              secureTextEntry
            />
            <View style={styles.buttonContainer}>
              <Button
                title="Self Video"
                onPress={() => navigation.navigate('SelfVideo')}
              />
            </View>
          </View>
        </ScrollView>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
  },
  container: {
    padding: 10,
    flexGrow: 1,
    gap: 10,
  },
  textInput: {
    padding: 10,
    borderWidth: 1,
    borderColor: 'tomato',
  },
  buttonContainer: {
    height: 44,
    margin: 5,
    justifyContent: 'center',
    backgroundColor: '#a8a4a4a3',
  },
});
