import { Text, StyleSheet, TouchableOpacity, View } from 'react-native';

type TabProps = {
  title: string;
  value: string;
  isActive: boolean;
  setActiveTab: (tab: string) => void;
};

function Tab({ title, value, isActive, setActiveTab }: TabProps) {
  return (
    <TouchableOpacity
      style={[styles.tabButton, isActive && styles.activeTab]}
      onPress={() => setActiveTab(value)}
    >
      <Text style={isActive && styles.activeText}>{title}</Text>
    </TouchableOpacity>
  );
}

type TabsProps = {
  activeTab: string;
  setActiveTab: (tab: string) => void;
};

export default function Tabs({ activeTab, setActiveTab }: TabsProps) {
  return (
    <View style={styles.tabContainer}>
      <Tab
        title={'Local\nSDP'}
        value="LocalSDP"
        isActive={activeTab === 'LocalSDP'}
        setActiveTab={setActiveTab}
      />
      <Tab
        title={'Local\nCand'}
        value="LocalCand"
        isActive={activeTab === 'LocalCand'}
        setActiveTab={setActiveTab}
      />
      <Tab
        title={'Remote\nSDP'}
        value="RemoteSDP"
        isActive={activeTab === 'RemoteSDP'}
        setActiveTab={setActiveTab}
      />
      <Tab
        title={'Remote\nCand'}
        value="RemoteCand"
        isActive={activeTab === 'RemoteCand'}
        setActiveTab={setActiveTab}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  tabContainer: {
    flexDirection: 'row',
  },
  tabButton: {
    flex: 1,
    alignItems: 'center',
  },
  activeTab: {
    borderBottomWidth: 2,
    borderColor: 'tomato',
  },
  activeText: {
    color: 'tomato',
    fontWeight: 'bold',
  },
});
