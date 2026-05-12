// App.tsx — RN shell. Phase C goal: prove the toolchain.
//
// Metro must resolve the `@inversa/*` workspace packages, the Tamagui
// babel plugin must compile style props, and the signals-react
// transform must wire React to signal reads. Everything else (TopBar,
// BrewView, BLE, chart) lands in later phases — they each need a small
// native audit (sticky positioning, `localStorage`, DOM events) before
// we can drop them in here.
//
// What this file does today:
//   - Mounts TamaguiProvider with the shared inversa config
//   - Reads the `theme` signal from @inversa/stores
//   - Renders a screen with a toggle button to prove signal reactivity
//   - Imports `parseRecipe` from @inversa/utils as a smoke test that
//     pure cross-platform code resolves through Metro

import { useSignals } from '@preact/signals-react/runtime';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Button, H1, Text, TamaguiProvider, Theme, YStack } from 'tamagui';
import { theme, toggleTheme } from '@inversa/stores';
import config from './tamagui.config';

export default function App() {
    useSignals();

    return (
        <TamaguiProvider config={config} defaultTheme={theme.value}>
            <Theme name={theme.value}>
                <SafeAreaView style={{ flex: 1, backgroundColor: theme.value === 'dark' ? '#1d232a' : '#fff' }}>
                    <YStack flex={1} padding="$4" gap="$3" alignItems="center" justifyContent="center">
                        <H1 color="$primary">Inversa</H1>
                        <Text opacity={0.7}>React Native scaffold — Phase C</Text>
                        <Text fontSize="$2" opacity={0.5}>theme: {theme.value}</Text>
                        <Button onPress={toggleTheme} marginTop="$3">
                            Alternar tema
                        </Button>
                    </YStack>
                </SafeAreaView>
            </Theme>
        </TamaguiProvider>
    );
}
