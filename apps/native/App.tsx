// App.tsx — RN shell. Mirrors apps/web/src/App.tsx but uses:
//
//   * SafeAreaProvider + SafeAreaView from `react-native-safe-area-context`
//     so the TopBar respects the iOS notch / Android status bar.
//   * GestureHandlerRootView at the very top — required by
//     react-native-gesture-handler (Tamagui Sheet, Popover, and
//     victory-native's chart-press hooks all rely on it).
//   * TemperatureChart from `src/components.native/`, injected into
//     BrewView via the same `chart` prop the web app uses.
//
// The Toast viewport stays portal-driven by @tamagui/toast's RN
// implementation (backed by `burnt`), so no extra positioning is
// needed here.

import { useEffect, useState } from 'react';
import { StatusBar } from 'expo-status-bar';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { useSignals } from '@preact/signals-react/runtime';
import { ToastProvider, ToastViewport, Toast } from '@tamagui/toast';
import { TamaguiProvider, Theme, YStack } from 'tamagui';
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
    const isOpen = (id: MenuId) => openSheet === id;

    return (
        <GestureHandlerRootView style={{ flex: 1 }}>
            <SafeAreaProvider>
                <TamaguiProvider config={config} defaultTheme={theme.value}>
                    <Theme name={theme.value}>
                        <ToastProvider swipeDirection="horizontal" duration={3000}>
                            <SafeAreaView style={{ flex: 1 }} edges={['top', 'left', 'right']}>
                                <YStack flex={1} backgroundColor="$background">
                                    <TopBar onMenuSelect={setOpenSheet} />

                                    <YStack flex={1}>
                                        <BrewView
                                            onMenuSelect={setOpenSheet}
                                            onStartManual={() => { mode.value = 'manual'; }}
                                            chart={<TemperatureChart />}
                                        />
                                    </YStack>

                                    <RecipeSheet        open={isOpen('recipes')}        onClose={close} />
                                    <BrewLogSheet       open={isOpen('brewlog')}        onClose={close} />
                                    <WizardEquipment    open={isOpen('equipment')}      onClose={close} />
                                    <WizardConnectivity open={isOpen('connectivity')}   onClose={close} />
                                    <WizardTuning       open={isOpen('tuning')}         onClose={close} />
                                    <WizardNotifications open={isOpen('notifications')} onClose={close} />
                                    <WizardAbout        open={isOpen('about')}          onClose={close} />

                                    <HopAlertOverlay />
                                    <ToastBridge />
                                </YStack>
                            </SafeAreaView>

                            <ToastViewport top={8} right={8} />
                            <Toast />
                        </ToastProvider>
                    </Theme>
                </TamaguiProvider>
                <StatusBar style="auto" />
            </SafeAreaProvider>
        </GestureHandlerRootView>
    );
}
