// App.tsx — RN shell. Mirrors apps/web/src/App.tsx with RN-only
// providers wrapped around it.
//
// The post-connect interaction lockup we hit on iOS was a perf issue:
// the sim bridge defaults to `--scale 60` (1 wall sec = 60 sim sec)
// and pushes telemetry at ~60Hz. The chart's polyline rebuild +
// signal-driven re-renders saturated the JS thread, so native
// gesture events queued faster than they could be drained — taps
// and scroll went dead. Fixed in packages/stores by throttling
// history pushes to 1 Hz (the chart's actual usefulness ceiling).

import { useEffect, useState } from 'react';
import { Dimensions, Platform } from 'react-native';
import { activateKeepAwakeAsync, deactivateKeepAwake } from 'expo-keep-awake';
import { StatusBar } from 'expo-status-bar';

// Lazy-require so a stale dev-client (built before
// expo-screen-orientation was installed) doesn't redbox on boot.
// Falls back to a no-op so the rotation policy below is harmless
// until the user runs `expo run:android` to relink the pod.
let ScreenOrientation: any = null;
try { ScreenOrientation = require('expo-screen-orientation'); }
catch { ScreenOrientation = null; }
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { useSignals } from '@preact/signals-react/runtime';
import { ToastProvider, ToastViewport } from '@tamagui/toast';
import { PortalProvider, ScrollView, TamaguiProvider, Theme, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import { isConnected, theme, manualIntent } from '@inversa/stores';
import {
    TopBar, BrewView, HopAlertOverlay, ToastBridge,
    RecipeSheet, BrewLogSheet, DevicePickerSheet, DebugSheet, WatchdogSheet,
    SchedulerSheet,
    CalibrationSheet,
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

    // Rotation policy: phones lock to portrait; tablets get full
    // freedom. Detection: Platform.isPad on iOS, screen min-edge
    // ≥ 600dp on Android (the conventional sw600dp tablet bucket).
    // `orientation: default` in app.json lets the OS rotate; the
    // lockAsync below clamps phones back to portrait at boot.
    useEffect(() => {
        if (!ScreenOrientation?.lockAsync) return;
        const { width, height } = Dimensions.get('window');
        const minEdge = Math.min(width, height);
        const isTablet = Platform.OS === 'ios'
            ? (Platform as any).isPad === true
            : minEdge >= 600;
        if (isTablet) {
            ScreenOrientation.unlockAsync().catch(() => {});
        } else {
            ScreenOrientation.lockAsync(ScreenOrientation.OrientationLock.PORTRAIT_UP).catch(() => {});
        }
    }, []);

    // Keep screen on while there's an active link. A brew session
    // runs for hours — a sleeping phone misses hop-addition alerts
    // and timer transitions. Once disconnected we release the lock
    // so the phone can sleep normally and save battery.
    const connected = isConnected.value;
    useEffect(() => {
        if (!connected) return;
        const tag = 'inversa-brew';
        activateKeepAwakeAsync(tag);
        return () => { deactivateKeepAwake(tag); };
    }, [connected]);

    const close = () => setOpenSheet(null);

    return (
        <GestureHandlerRootView style={{ flex: 1 }}>
            <SafeAreaProvider>
                <TamaguiProvider config={config} defaultTheme={theme.value}>
                    <Theme name={theme.value}>
                      {/* PortalProvider is required by Tamagui's Sheet /
                          AlertDialog (they dispatch to PortalDispatchContext).
                          The web tree gets one via TamaguiProvider
                          automatically; RN doesn't, so mount it explicitly.
                          shouldAddRootHost gives every Portal a default
                          root container. */}
                      <PortalProvider shouldAddRootHost>
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
                                            onStartManual={() => { manualIntent.value = true; }}
                                            chart={<TemperatureChart />}
                                        />
                                    </ScrollView>

                                    {openSheet === 'recipes'       && <RecipeSheet        open onClose={close} />}
                                    {openSheet === 'brewlog'       && <BrewLogSheet       open onClose={close} />}
                                    {openSheet === 'equipment'     && <WizardEquipment    open onClose={close} />}
                                    {openSheet === 'calibration'   && <CalibrationSheet   open onClose={close} />}
                                    {openSheet === 'connectivity'  && <WizardConnectivity open onClose={close} />}
                                    {openSheet === 'tuning'        && <WizardTuning       open onClose={close} />}
                                    {openSheet === 'notifications' && <WizardNotifications open onClose={close} />}
                                    {openSheet === 'about'         && <WizardAbout        open onClose={close} />}
                                    {openSheet === 'debug'         && <DebugSheet         open onClose={close} />}
                                    {openSheet === 'watchdog'      && <WatchdogSheet      open onClose={close} />}
                                    {openSheet === 'scheduler'     && <SchedulerSheet     open onClose={close} />}

                                    <DevicePickerSheet />

                                    <HopAlertOverlay />
                                    <ToastBridge />
                                </YStack>
                            </SafeAreaView>

                            <ToastViewport top={8} right={8} />
                        </ToastProvider>
                      </PortalProvider>
                    </Theme>
                </TamaguiProvider>
                <StatusBar style="auto" />
            </SafeAreaProvider>
        </GestureHandlerRootView>
    );
}
