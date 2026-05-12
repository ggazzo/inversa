// App.tsx — RN shell. Mirrors apps/web/src/App.tsx but adds the RN-only
// providers (GestureHandlerRootView, SafeAreaProvider) and the Tamagui
// provider that the web side doesn't need at this level.

import { useEffect, useState } from 'react';
import { StatusBar } from 'expo-status-bar';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { useSignals } from '@preact/signals-react/runtime';
import { ToastProvider, ToastViewport } from '@tamagui/toast';
import { ScrollView, TamaguiProvider, Theme, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import { theme, mode } from '@inversa/stores';
import {
    TopBar, BrewView, HopAlertOverlay, ToastBridge,
    RecipeSheet, BrewLogSheet,
    WizardEquipment, WizardConnectivity, WizardTuning,
    WizardNotifications, WizardAbout,
    type MenuId,
} from '@inversa/ui';
import { TemperatureChart } from './src/components.native/TemperatureChart';
import config from './tamagui.config';

export default function App() {
    useSignals();
    const [openSheet, setOpenSheet] = useState<MenuId | null>(null);

    useEffect(() => { ConnectionManager.init(); }, []);

    const close = () => setOpenSheet(null);

    return (
        <GestureHandlerRootView style={{ flex: 1 }}>
            <SafeAreaProvider>
                <TamaguiProvider config={config} defaultTheme={theme.value}>
                    <Theme name={theme.value}>
                        {/* native={[]} disables Tamagui's burnt-backed native
                            toast adapter; we use the in-tree ToastViewport
                            instead so the toast Z-layer stays inside our
                            React view hierarchy (matching the web build). */}
                        <ToastProvider swipeDirection="horizontal" duration={3000} native={[]}>
                            <SafeAreaView style={{ flex: 1 }} edges={['top', 'left', 'right']}>
                                <YStack flex={1} backgroundColor="$background">
                                    <TopBar onMenuSelect={setOpenSheet} />

                                    {/* Tamagui's ScrollView (RN ScrollView + tamagui styling)
                                        is the canonical pattern for scrolling content inside a
                                        Tamagui tree. RN's bare <ScrollView> caused weird touch
                                        propagation when nested in Tamagui flex layouts. */}
                                    <ScrollView
                                        flex={1}
                                        contentContainerStyle={{ flexGrow: 1 }}
                                        keyboardShouldPersistTaps="handled"
                                    >
                                        <BrewView
                                            onMenuSelect={setOpenSheet}
                                            onStartManual={() => { mode.value = 'manual'; }}
                                            chart={<TemperatureChart />}
                                        />
                                    </ScrollView>

                                    {/* Only mount the sheet that's actually open. Mounting all
                                        7 at once leaves their Portal containers in the tree at
                                        zIndex 100k+; on RN those swallowed touches even with
                                        `open={false}`. */}
                                    {openSheet === 'recipes'       && <RecipeSheet        open onClose={close} />}
                                    {openSheet === 'brewlog'       && <BrewLogSheet       open onClose={close} />}
                                    {openSheet === 'equipment'     && <WizardEquipment    open onClose={close} />}
                                    {openSheet === 'connectivity'  && <WizardConnectivity open onClose={close} />}
                                    {openSheet === 'tuning'        && <WizardTuning       open onClose={close} />}
                                    {openSheet === 'notifications' && <WizardNotifications open onClose={close} />}
                                    {openSheet === 'about'         && <WizardAbout        open onClose={close} />}

                                    <HopAlertOverlay />
                                    <ToastBridge />
                                </YStack>
                            </SafeAreaView>

                            <ToastViewport top={8} right={8} />
                        </ToastProvider>
                    </Theme>
                </TamaguiProvider>
                <StatusBar style="auto" />
            </SafeAreaProvider>
        </GestureHandlerRootView>
    );
}
