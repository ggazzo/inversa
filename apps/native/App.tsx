// App.tsx — RN shell. Diagnostic build to isolate the post-connect
// interaction lockup. Everything previously suspected (bare <Toast />,
// burnt adapter, all-mounted sheets) has been removed; this version
// also temporarily drops <ToastViewport> and <HopAlertOverlay> from
// the tree so the only thing layered above BrewView is the TopBar.
//
// Once tap + scroll come back, we can add the pieces back one by one.

import { useEffect, useState } from 'react';
import { StatusBar } from 'expo-status-bar';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { useSignals } from '@preact/signals-react/runtime';
import { ToastProvider } from '@tamagui/toast';
import { ScrollView, TamaguiProvider, Theme, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import { theme, mode } from '@inversa/stores';
import {
    TopBar, BrewView, ToastBridge,
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
                        <ToastProvider swipeDirection="horizontal" duration={3000} native={[]}>
                            <SafeAreaView style={{ flex: 1 }} edges={['top', 'left', 'right']}>
                                <YStack flex={1} backgroundColor="$background">
                                    <TopBar onMenuSelect={setOpenSheet} />

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

                                    {openSheet === 'recipes'       && <RecipeSheet        open onClose={close} />}
                                    {openSheet === 'brewlog'       && <BrewLogSheet       open onClose={close} />}
                                    {openSheet === 'equipment'     && <WizardEquipment    open onClose={close} />}
                                    {openSheet === 'connectivity'  && <WizardConnectivity open onClose={close} />}
                                    {openSheet === 'tuning'        && <WizardTuning       open onClose={close} />}
                                    {openSheet === 'notifications' && <WizardNotifications open onClose={close} />}
                                    {openSheet === 'about'         && <WizardAbout        open onClose={close} />}

                                    <ToastBridge />
                                </YStack>
                            </SafeAreaView>
                            {/* TEMPORARILY REMOVED for diagnostic:
                                <ToastViewport top={8} right={8} />
                                <HopAlertOverlay />
                                These were the most likely candidates for an
                                invisible overlay blocking taps. If they're
                                gone and clicks still don't work, the issue
                                is in the BrewView subtree or the chart. */}
                        </ToastProvider>
                    </Theme>
                </TamaguiProvider>
                <StatusBar style="auto" />
            </SafeAreaProvider>
        </GestureHandlerRootView>
    );
}
